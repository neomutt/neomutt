/**
 * @file
 * IMAP network mailbox
 *
 * @authors
 * Copyright (C) 1996-1998,2012 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1996-1999 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2009,2012,2017 Brendan Cully <brendan@kublai.com>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @page imap_imap IMAP network mailbox
 *
 * Support for IMAP4rev1, with the occasional nod to IMAP 4.
 */

#include "config.h"
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "imap_private.h"
#include "mutt/mutt.h"
#include "conn/conn.h"
#include "mutt.h"
#include "imap.h"
#include "auth.h"
#include "bcache.h"
#include "body.h"
#include "buffy.h"
#include "context.h"
#include "envelope.h"
#include "globals.h"
#include "header.h"
#include "mailbox.h"
#include "message.h"
#include "mutt_account.h"
#include "mutt_curses.h"
#include "mutt_logging.h"
#include "mutt_socket.h"
#include "mx.h"
#include "options.h"
#include "pattern.h"
#include "progress.h"
#include "protos.h"
#include "sort.h"
#include "tags.h"
#include "url.h"
#ifdef USE_HCACHE
#include "hcache/hcache.h"
#endif

/**
 * check_capabilities - Make sure we can log in to this server
 * @param idata Server data
 * @retval  0 Success
 * @retval -1 Failure
 */
static int check_capabilities(struct ImapData *idata)
{
  if (imap_exec(idata, "CAPABILITY", 0) != 0)
  {
    imap_error("check_capabilities", idata->buf);
    return -1;
  }

  if (!(mutt_bit_isset(idata->capabilities, IMAP4) ||
        mutt_bit_isset(idata->capabilities, IMAP4REV1)))
  {
    mutt_error(
        _("This IMAP server is ancient. NeoMutt does not work with it."));
    return -1;
  }

  return 0;
}

/**
 * get_flags - Make a simple list out of a FLAGS response
 * @param hflags List to store flags
 * @param s      String containing flags
 * @retval ptr End of the flags
 * @retval ptr NULL Failure
 *
 * return stream following FLAGS response
 */
static char *get_flags(struct ListHead *hflags, char *s)
{
  /* sanity-check string */
  if (mutt_str_strncasecmp("FLAGS", s, 5) != 0)
  {
    mutt_debug(1, "not a FLAGS response: %s\n", s);
    return NULL;
  }
  s += 5;
  SKIPWS(s);
  if (*s != '(')
  {
    mutt_debug(1, "bogus FLAGS response: %s\n", s);
    return NULL;
  }

  /* update caller's flags handle */
  while (*s && *s != ')')
  {
    s++;
    SKIPWS(s);
    const char *flag_word = s;
    while (*s && (*s != ')') && !ISSPACE(*s))
      s++;
    const char ctmp = *s;
    *s = '\0';
    if (*flag_word)
      mutt_list_insert_tail(hflags, mutt_str_strdup(flag_word));
    *s = ctmp;
  }

  /* note bad flags response */
  if (*s != ')')
  {
    mutt_debug(1, "Unterminated FLAGS response: %s\n", s);
    mutt_list_free(hflags);

    return NULL;
  }

  s++;

  return s;
}

/**
 * set_flag - append str to flags if we currently have permission according to aclbit
 * @param[in]  idata  Server data
 * @param[in]  aclbit Permissions, e.g. #MUTT_ACL_WRITE
 * @param[in]  flag   Does the email have the flag set?
 * @param[in]  str    Server flag name
 * @param[out] flags  Buffer for server command
 * @param[in]  flsize Length of buffer
 */
static void set_flag(struct ImapData *idata, int aclbit, int flag,
                     const char *str, char *flags, size_t flsize)
{
  if (mutt_bit_isset(idata->ctx->rights, aclbit))
    if (flag && imap_has_flag(&idata->flags, str))
      mutt_str_strcat(flags, flsize, str);
}

/**
 * make_msg_set - Make a message set
 * @param[in]  idata   Server data
 * @param[in]  buf     Buffer to store message set
 * @param[in]  flag    Flags to match, e.g. #MUTT_DELETED
 * @param[in]  changed Matched messages that have been altered
 * @param[in]  invert  Flag matches should be inverted
 * @param[out] pos     Cursor used for multiple calls to this function
 * @retval num Messages in the set
 *
 * @note Headers must be in SORT_ORDER. See imap_exec_msgset() for args.
 * Pos is an opaque pointer a la strtok(). It should be 0 at first call.
 */
static int make_msg_set(struct ImapData *idata, struct Buffer *buf, int flag,
                        bool changed, bool invert, int *pos)
{
  int count = 0;             /* number of messages in message set */
  unsigned int setstart = 0; /* start of current message range */
  int n;
  bool started = false;
  struct Header **hdrs = idata->ctx->hdrs;

  for (n = *pos; n < idata->ctx->msgcount && buf->dptr - buf->data < IMAP_MAX_CMDLEN; n++)
  {
    bool match = false; /* whether current message matches flag condition */
    /* don't include pending expunged messages */
    if (hdrs[n]->active)
    {
      switch (flag)
      {
        case MUTT_DELETED:
          if (hdrs[n]->deleted != HEADER_DATA(hdrs[n])->deleted)
            match = invert ^ hdrs[n]->deleted;
          break;
        case MUTT_FLAG:
          if (hdrs[n]->flagged != HEADER_DATA(hdrs[n])->flagged)
            match = invert ^ hdrs[n]->flagged;
          break;
        case MUTT_OLD:
          if (hdrs[n]->old != HEADER_DATA(hdrs[n])->old)
            match = invert ^ hdrs[n]->old;
          break;
        case MUTT_READ:
          if (hdrs[n]->read != HEADER_DATA(hdrs[n])->read)
            match = invert ^ hdrs[n]->read;
          break;
        case MUTT_REPLIED:
          if (hdrs[n]->replied != HEADER_DATA(hdrs[n])->replied)
            match = invert ^ hdrs[n]->replied;
          break;
        case MUTT_TAG:
          if (hdrs[n]->tagged)
            match = true;
          break;
        case MUTT_TRASH:
          if (hdrs[n]->deleted && !hdrs[n]->purge)
            match = true;
          break;
      }
    }

    if (match && (!changed || hdrs[n]->changed))
    {
      count++;
      if (setstart == 0)
      {
        setstart = HEADER_DATA(hdrs[n])->uid;
        if (!started)
        {
          mutt_buffer_printf(buf, "%u", HEADER_DATA(hdrs[n])->uid);
          started = true;
        }
        else
          mutt_buffer_printf(buf, ",%u", HEADER_DATA(hdrs[n])->uid);
      }
      /* tie up if the last message also matches */
      else if (n == idata->ctx->msgcount - 1)
        mutt_buffer_printf(buf, ":%u", HEADER_DATA(hdrs[n])->uid);
    }
    /* End current set if message doesn't match or we've reached the end
     * of the mailbox via inactive messages following the last match. */
    else if (setstart && (hdrs[n]->active || n == idata->ctx->msgcount - 1))
    {
      if (HEADER_DATA(hdrs[n - 1])->uid > setstart)
        mutt_buffer_printf(buf, ":%u", HEADER_DATA(hdrs[n - 1])->uid);
      setstart = 0;
    }
  }

  *pos = n;

  return count;
}

/**
 * compare_flags_for_copy - Compare local flags against the server
 * @param h Header of email
 * @retval true  Flags have changed
 * @retval false Flags match cached server flags
 *
 * The comparison of flags EXCLUDES the deleted flag.
 */
static bool compare_flags_for_copy(struct Header *h)
{
  struct ImapHeaderData *hd = (struct ImapHeaderData *) h->data;

  if (h->read != hd->read)
    return true;
  if (h->old != hd->old)
    return true;
  if (h->flagged != hd->flagged)
    return true;
  if (h->replied != hd->replied)
    return true;

  return false;
}

/**
 * sync_helper - Sync flag changes to the server
 * @param idata Server data
 * @param right ACL, e.g. #MUTT_ACL_DELETE
 * @param flag  Mutt flag, e.g. MUTT_DELETED
 * @param name  Name of server flag
 * @retval >=0 Success, number of messages
 * @retval  -1 Failure
 */
static int sync_helper(struct ImapData *idata, int right, int flag, const char *name)
{
  int count = 0;
  int rc;
  char buf[LONG_STRING];

  if (!idata->ctx)
    return -1;

  if (!mutt_bit_isset(idata->ctx->rights, right))
    return 0;

  if (right == MUTT_ACL_WRITE && !imap_has_flag(&idata->flags, name))
    return 0;

  snprintf(buf, sizeof(buf), "+FLAGS.SILENT (%s)", name);
  rc = imap_exec_msgset(idata, "UID STORE", buf, flag, 1, 0);
  if (rc < 0)
    return rc;
  count += rc;

  buf[0] = '-';
  rc = imap_exec_msgset(idata, "UID STORE", buf, flag, 1, 1);
  if (rc < 0)
    return rc;
  count += rc;

  return count;
}

/**
 * get_mailbox - Split mailbox URI
 * @param path   Mailbox URI
 * @param hidata Server data
 * @param buf    Buffer to store mailbox name
 * @param blen   Length of buffer
 * @retval  0 Success
 * @retval -1 Failure
 *
 * Split up a mailbox URI.  The connection info is stored in the ImapData and
 * the mailbox name is stored in buf.
 */
static int get_mailbox(const char *path, struct ImapData **hidata, char *buf, size_t blen)
{
  struct ImapMbox mx;

  if (imap_parse_path(path, &mx))
  {
    mutt_debug(1, "Error parsing %s\n", path);
    return -1;
  }
  if (!(*hidata = imap_conn_find(&(mx.account), ImapPassive ? MUTT_IMAP_CONN_NONEW : 0)) ||
      (*hidata)->state < IMAP_AUTHENTICATED)
  {
    FREE(&mx.mbox);
    return -1;
  }

  imap_fix_path(*hidata, mx.mbox, buf, blen);
  if (!*buf)
    mutt_str_strfcpy(buf, "INBOX", blen);
  FREE(&mx.mbox);

  return 0;
}

/**
 * do_search - Perform a search of messages
 * @param search  List of pattern to match
 * @param allpats Must all patterns match?
 * @retval num Number of patterns search that should be done server-side
 *
 * Count the number of patterns that can be done by the server (are full-text).
 */
static int do_search(const struct Pattern *search, int allpats)
{
  int rc = 0;
  const struct Pattern *pat = NULL;

  for (pat = search; pat; pat = pat->next)
  {
    switch (pat->op)
    {
      case MUTT_BODY:
      case MUTT_HEADER:
      case MUTT_WHOLE_MSG:
        if (pat->stringmatch)
          rc++;
        break;
      case MUTT_SERVERSEARCH:
        rc++;
        break;
      default:
        if (pat->child && do_search(pat->child, 1))
          rc++;
    }

    if (!allpats)
      break;
  }

  return rc;
}

/**
 * compile_search - Convert NeoMutt pattern to IMAP search
 * @param ctx Context
 * @param pat Pattern to convert
 * @param buf Buffer for result
 * @retval  0 Success
 * @retval -1 Failure
 *
 * Convert neomutt Pattern to IMAP SEARCH command containing only elements
 * that require full-text search (neomutt already has what it needs for most
 * match types, and does a better job (eg server doesn't support regexes).
 */
static int compile_search(struct Context *ctx, const struct Pattern *pat, struct Buffer *buf)
{
  if (do_search(pat, 0) == 0)
    return 0;

  if (pat->not)
    mutt_buffer_addstr(buf, "NOT ");

  if (pat->child)
  {
    int clauses;

    clauses = do_search(pat->child, 1);
    if (clauses > 0)
    {
      const struct Pattern *clause = pat->child;

      mutt_buffer_addch(buf, '(');

      while (clauses)
      {
        if (do_search(clause, 0))
        {
          if (pat->op == MUTT_OR && clauses > 1)
            mutt_buffer_addstr(buf, "OR ");
          clauses--;

          if (compile_search(ctx, clause, buf) < 0)
            return -1;

          if (clauses)
            mutt_buffer_addch(buf, ' ');
        }
        clause = clause->next;
      }

      mutt_buffer_addch(buf, ')');
    }
  }
  else
  {
    char term[STRING];
    char *delim = NULL;

    switch (pat->op)
    {
      case MUTT_HEADER:
        mutt_buffer_addstr(buf, "HEADER ");

        /* extract header name */
        delim = strchr(pat->p.str, ':');
        if (!delim)
        {
          mutt_error(_("Header search without header name: %s"), pat->p.str);
          return -1;
        }
        *delim = '\0';
        imap_quote_string(term, sizeof(term), pat->p.str);
        mutt_buffer_addstr(buf, term);
        mutt_buffer_addch(buf, ' ');

        /* and field */
        *delim = ':';
        delim++;
        SKIPWS(delim);
        imap_quote_string(term, sizeof(term), delim);
        mutt_buffer_addstr(buf, term);
        break;
      case MUTT_BODY:
        mutt_buffer_addstr(buf, "BODY ");
        imap_quote_string(term, sizeof(term), pat->p.str);
        mutt_buffer_addstr(buf, term);
        break;
      case MUTT_WHOLE_MSG:
        mutt_buffer_addstr(buf, "TEXT ");
        imap_quote_string(term, sizeof(term), pat->p.str);
        mutt_buffer_addstr(buf, term);
        break;
      case MUTT_SERVERSEARCH:
      {
        struct ImapData *idata = ctx->data;
        if (!mutt_bit_isset(idata->capabilities, X_GM_EXT1))
        {
          mutt_error(_("Server-side custom search not supported: %s"), pat->p.str);
          return -1;
        }
      }
        mutt_buffer_addstr(buf, "X-GM-RAW ");
        imap_quote_string(term, sizeof(term), pat->p.str);
        mutt_buffer_addstr(buf, term);
        break;
    }
  }

  return 0;
}

/**
 * longest_common_prefix - Find longest prefix common to two strings
 * @param dest  Destination buffer
 * @param src   Source buffer
 * @param start Starting offset into string
 * @param dlen  Destination buffer length
 * @retval num Length of the common string
 *
 * Trim dest to the length of the longest prefix it shares with src.
 */
static size_t longest_common_prefix(char *dest, const char *src, size_t start, size_t dlen)
{
  size_t pos = start;

  while (pos < dlen && dest[pos] && dest[pos] == src[pos])
    pos++;
  dest[pos] = '\0';

  return pos;
}

/**
 * complete_hosts - Look for completion matches for mailboxes
 * @param buf Partial mailbox name to complete
 * @param buflen  Length of buffer
 * @retval  0 Success
 * @retval -1 Failure
 *
 * look for IMAP URLs to complete from defined mailboxes. Could be extended to
 * complete over open connections and account/folder hooks too.
 */
static int complete_hosts(char *buf, size_t buflen)
{
  struct Buffy *mailbox = NULL;
  struct Connection *conn = NULL;
  int rc = -1;
  size_t matchlen;

  matchlen = mutt_str_strlen(buf);
  for (mailbox = Incoming; mailbox; mailbox = mailbox->next)
  {
    if (mutt_str_strncmp(buf, mailbox->path, matchlen) == 0)
    {
      if (rc)
      {
        mutt_str_strfcpy(buf, mailbox->path, buflen);
        rc = 0;
      }
      else
        longest_common_prefix(buf, mailbox->path, matchlen, buflen);
    }
  }

  TAILQ_FOREACH(conn, mutt_socket_head(), entries)
  {
    struct Url url;
    char urlstr[LONG_STRING];

    if (conn->account.type != MUTT_ACCT_TYPE_IMAP)
      continue;

    mutt_account_tourl(&conn->account, &url);
    /* FIXME: how to handle multiple users on the same host? */
    url.user = NULL;
    url.path = NULL;
    url_tostring(&url, urlstr, sizeof(urlstr), 0);
    if (mutt_str_strncmp(buf, urlstr, matchlen) == 0)
    {
      if (rc)
      {
        mutt_str_strfcpy(buf, urlstr, buflen);
        rc = 0;
      }
      else
        longest_common_prefix(buf, urlstr, matchlen, buflen);
    }
  }

  return rc;
}

/**
 * imap_access - Check permissions on an IMAP mailbox
 * @param path Mailbox path
 * @retval  0 Success
 * @retval <0 Failure
 *
 * TODO: ACL checks. Right now we assume if it exists we can mess with it.
 */
int imap_access(const char *path)
{
  struct ImapData *idata = NULL;
  struct ImapMbox mx;
  char buf[LONG_STRING];
  char mailbox[LONG_STRING];
  char mbox[LONG_STRING];
  int rc;

  if (imap_parse_path(path, &mx))
    return -1;

  idata = imap_conn_find(&mx.account, ImapPassive ? MUTT_IMAP_CONN_NONEW : 0);
  if (!idata)
  {
    FREE(&mx.mbox);
    return -1;
  }

  imap_fix_path(idata, mx.mbox, mailbox, sizeof(mailbox));
  if (!*mailbox)
    mutt_str_strfcpy(mailbox, "INBOX", sizeof(mailbox));

  /* we may already be in the folder we're checking */
  if (mutt_str_strcmp(idata->mailbox, mx.mbox) == 0)
  {
    FREE(&mx.mbox);
    return 0;
  }
  FREE(&mx.mbox);

  if (imap_mboxcache_get(idata, mailbox, 0))
  {
    mutt_debug(3, "found %s in cache\n", mailbox);
    return 0;
  }

  imap_munge_mbox_name(idata, mbox, sizeof(mbox), mailbox);

  if (mutt_bit_isset(idata->capabilities, IMAP4REV1))
    snprintf(buf, sizeof(buf), "STATUS %s (UIDVALIDITY)", mbox);
  else if (mutt_bit_isset(idata->capabilities, STATUS))
    snprintf(buf, sizeof(buf), "STATUS %s (UID-VALIDITY)", mbox);
  else
  {
    mutt_debug(2, "STATUS not supported?\n");
    return -1;
  }

  rc = imap_exec(idata, buf, IMAP_CMD_FAIL_OK);
  if (rc < 0)
  {
    mutt_debug(1, "Can't check STATUS of %s\n", mbox);
    return rc;
  }

  return 0;
}

/**
 * imap_create_mailbox - Create a new mailbox
 * @param idata   Server data
 * @param mailbox Mailbox to create
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_create_mailbox(struct ImapData *idata, char *mailbox)
{
  char buf[LONG_STRING], mbox[LONG_STRING];

  imap_munge_mbox_name(idata, mbox, sizeof(mbox), mailbox);
  snprintf(buf, sizeof(buf), "CREATE %s", mbox);

  if (imap_exec(idata, buf, 0) != 0)
  {
    mutt_error(_("CREATE failed: %s"), imap_cmd_trailer(idata));
    return -1;
  }

  return 0;
}

/**
 * imap_rename_mailbox - Rename a mailbox
 * @param idata   Server data
 * @param mx      Existing mailbox
 * @param newname New name for mailbox
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_rename_mailbox(struct ImapData *idata, struct ImapMbox *mx, const char *newname)
{
  char oldmbox[LONG_STRING];
  char newmbox[LONG_STRING];
  char buf[LONG_STRING];

  imap_munge_mbox_name(idata, oldmbox, sizeof(oldmbox), mx->mbox);
  imap_munge_mbox_name(idata, newmbox, sizeof(newmbox), newname);

  snprintf(buf, sizeof(buf), "RENAME %s %s", oldmbox, newmbox);

  if (imap_exec(idata, buf, 0) != 0)
    return -1;

  return 0;
}

/**
 * imap_delete_mailbox - Delete a mailbox
 * @param ctx Context
 * @param mx  Mailbox to delete
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_delete_mailbox(struct Context *ctx, struct ImapMbox *mx)
{
  char buf[PATH_MAX], mbox[PATH_MAX];
  struct ImapData *idata = NULL;

  if (!ctx || !ctx->data)
  {
    idata = imap_conn_find(&mx->account, ImapPassive ? MUTT_IMAP_CONN_NONEW : 0);
    if (!idata)
    {
      FREE(&mx->mbox);
      return -1;
    }
  }
  else
  {
    idata = ctx->data;
  }

  imap_munge_mbox_name(idata, mbox, sizeof(mbox), mx->mbox);
  snprintf(buf, sizeof(buf), "DELETE %s", mbox);

  if (imap_exec(idata, buf, 0) != 0)
    return -1;

  return 0;
}

/**
 * imap_logout_all - close all open connections
 *
 * Quick and dirty until we can make sure we've got all the context we need.
 */
void imap_logout_all(void)
{
  struct ConnectionList *head = mutt_socket_head();
  struct Connection *np, *tmp;
  TAILQ_FOREACH_SAFE(np, head, entries, tmp)
  {
    if (np->account.type == MUTT_ACCT_TYPE_IMAP && np->fd >= 0)
    {
      TAILQ_REMOVE(head, np, entries);
      mutt_message(_("Closing connection to %s..."), np->account.host);
      imap_logout((struct ImapData **) (void *) &np->data);
      mutt_clear_error();
      mutt_socket_free(np);
    }
  }
}

/**
 * imap_read_literal - Read bytes bytes from server into file
 * @param fp    File handle for email file
 * @param idata Server data
 * @param bytes Number of bytes to read
 * @param pbar  Progress bar
 * @retval  0 Success
 * @retval -1 Failure
 *
 * Not explicitly buffered, relies on FILE buffering.
 *
 * @note Strips `\r` from `\r\n`.
 *       Apparently even literals use `\r\n`-terminated strings ?!
 */
int imap_read_literal(FILE *fp, struct ImapData *idata, unsigned long bytes,
                      struct Progress *pbar)
{
  char c;
  bool r = false;
  struct Buffer *buf = NULL;

  if (DebugLevel >= IMAP_LOG_LTRL)
    buf = mutt_buffer_alloc(bytes + 10);

  mutt_debug(2, "reading %ld bytes\n", bytes);

  for (unsigned long pos = 0; pos < bytes; pos++)
  {
    if (mutt_socket_readchar(idata->conn, &c) != 1)
    {
      mutt_debug(1, "error during read, %ld bytes read\n", pos);
      idata->status = IMAP_FATAL;

      mutt_buffer_free(&buf);
      return -1;
    }

    if (r && c != '\n')
      fputc('\r', fp);

    if (c == '\r')
    {
      r = true;
      continue;
    }
    else
      r = false;

    fputc(c, fp);

    if (pbar && !(pos % 1024))
      mutt_progress_update(pbar, pos, -1);
    if (DebugLevel >= IMAP_LOG_LTRL)
      mutt_buffer_addch(buf, c);
  }

  if (DebugLevel >= IMAP_LOG_LTRL)
  {
    mutt_debug(IMAP_LOG_LTRL, "\n%s", buf->data);
    mutt_buffer_free(&buf);
  }
  return 0;
}

/**
 * imap_expunge_mailbox - Purge messages from the server
 * @param idata Server data
 *
 * Purge IMAP portion of expunged messages from the context. Must not be done
 * while something has a handle on any headers (eg inside pager or editor).
 * That is, check IMAP_REOPEN_ALLOW.
 */
void imap_expunge_mailbox(struct ImapData *idata)
{
  struct Header *h = NULL;
  int cacheno;
  short old_sort;

#ifdef USE_HCACHE
  idata->hcache = imap_hcache_open(idata, NULL);
#endif

  old_sort = Sort;
  Sort = SORT_ORDER;
  mutt_sort_headers(idata->ctx, 0);

  for (int i = 0; i < idata->ctx->msgcount; i++)
  {
    h = idata->ctx->hdrs[i];

    if (h->index == INT_MAX)
    {
      mutt_debug(2, "Expunging message UID %u.\n", HEADER_DATA(h)->uid);

      h->active = false;
      idata->ctx->size -= h->content->length;

      imap_cache_del(idata, h);
#ifdef USE_HCACHE
      imap_hcache_del(idata, HEADER_DATA(h)->uid);
#endif

      /* free cached body from disk, if necessary */
      cacheno = HEADER_DATA(h)->uid % IMAP_CACHE_LEN;
      if (idata->cache[cacheno].uid == HEADER_DATA(h)->uid &&
          idata->cache[cacheno].path)
      {
        unlink(idata->cache[cacheno].path);
        FREE(&idata->cache[cacheno].path);
      }

      mutt_hash_int_delete(idata->uid_hash, HEADER_DATA(h)->uid, h);

      imap_free_header_data((struct ImapHeaderData **) &h->data);
    }
    else
    {
      h->index = i;
      /* NeoMutt has several places where it turns off h->active as a
       * hack.  For example to avoid FLAG updates, or to exclude from
       * imap_exec_msgset.
       *
       * Unfortunately, when a reopen is allowed and the IMAP_EXPUNGE_PENDING
       * flag becomes set (e.g. a flag update to a modified header),
       * this function will be called by imap_cmd_finish().
       *
       * The mx_update_tables() will free and remove these "inactive" headers,
       * despite that an EXPUNGE was not received for them.
       * This would result in memory leaks and segfaults due to dangling
       * pointers in the msn_index and uid_hash.
       *
       * So this is another hack to work around the hacks.  We don't want to
       * remove the messages, so make sure active is on.
       */
      h->active = true;
    }
  }

#ifdef USE_HCACHE
  imap_hcache_close(idata);
#endif

  /* We may be called on to expunge at any time. We can't rely on the caller
   * to always know to rethread */
  mx_update_tables(idata->ctx, false);
  Sort = old_sort;
  mutt_sort_headers(idata->ctx, 1);
}

/**
 * imap_conn_find - Find an open IMAP connection
 * @param account Account to search
 * @param flags   Flags, e.g. #MUTT_IMAP_CONN_NONEW
 * @retval ptr  Matching connection
 * @retval NULL Failure
 *
 * Find an open IMAP connection matching account, or open a new one if none can
 * be found.
 */
struct ImapData *imap_conn_find(const struct Account *account, int flags)
{
  struct Connection *conn = NULL;
  struct Account *creds = NULL;
  struct ImapData *idata = NULL;
  bool new = false;

  while ((conn = mutt_conn_find(conn, account)))
  {
    if (!creds)
      creds = &conn->account;
    else
      memcpy(&conn->account, creds, sizeof(struct Account));

    idata = conn->data;
    if (flags & MUTT_IMAP_CONN_NONEW)
    {
      if (!idata)
      {
        /* This should only happen if we've come to the end of the list */
        mutt_socket_free(conn);
        return NULL;
      }
      else if (idata->state < IMAP_AUTHENTICATED)
        continue;
    }
    if (flags & MUTT_IMAP_CONN_NOSELECT && idata && idata->state >= IMAP_SELECTED)
      continue;
    if (idata && idata->status == IMAP_FATAL)
      continue;
    break;
  }
  if (!conn)
    return NULL; /* this happens when the initial connection fails */

  if (!idata)
  {
    /* The current connection is a new connection */
    idata = imap_new_idata();
    if (!idata)
    {
      mutt_socket_free(conn);
      return NULL;
    }

    conn->data = idata;
    idata->conn = conn;
    new = true;
  }

  if (idata->state == IMAP_DISCONNECTED)
    imap_open_connection(idata);
  if (idata->state == IMAP_CONNECTED)
  {
    if (imap_authenticate(idata) == IMAP_AUTH_SUCCESS)
    {
      idata->state = IMAP_AUTHENTICATED;
      FREE(&idata->capstr);
      new = true;
      if (idata->conn->ssf)
        mutt_debug(2, "Communication encrypted at %d bits\n", idata->conn->ssf);
    }
    else
      mutt_account_unsetpass(&idata->conn->account);
  }
  if (new && idata->state == IMAP_AUTHENTICATED)
  {
    /* capabilities may have changed */
    imap_exec(idata, "CAPABILITY", IMAP_CMD_QUEUE);
    /* enable RFC6855, if the server supports that */
    if (mutt_bit_isset(idata->capabilities, ENABLE))
      imap_exec(idata, "ENABLE UTF8=ACCEPT", IMAP_CMD_QUEUE);
    /* get root delimiter, '/' as default */
    idata->delim = '/';
    imap_exec(idata, "LIST \"\" \"\"", IMAP_CMD_QUEUE);
    /* we may need the root delimiter before we open a mailbox */
    imap_exec(idata, NULL, IMAP_CMD_FAIL_OK);
  }

  return idata;
}

/**
 * imap_open_connection - Open an IMAP connection
 * @param idata Server data
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_open_connection(struct ImapData *idata)
{
  char buf[LONG_STRING];

  if (mutt_socket_open(idata->conn) < 0)
    return -1;

  idata->state = IMAP_CONNECTED;

  if (imap_cmd_step(idata) != IMAP_CMD_OK)
  {
    imap_close_connection(idata);
    return -1;
  }

  if (mutt_str_strncasecmp("* OK", idata->buf, 4) == 0)
  {
    if ((mutt_str_strncasecmp("* OK [CAPABILITY", idata->buf, 16) != 0) &&
        check_capabilities(idata))
    {
      goto bail;
    }
#ifdef USE_SSL
    /* Attempt STARTTLS if available and desired. */
    if (!idata->conn->ssf && (SslForceTls || mutt_bit_isset(idata->capabilities, STARTTLS)))
    {
      int rc;

      if (SslForceTls)
        rc = MUTT_YES;
      else if ((rc = query_quadoption(SslStarttls,
                                      _("Secure connection with TLS?"))) == MUTT_ABORT)
      {
        goto err_close_conn;
      }
      if (rc == MUTT_YES)
      {
        rc = imap_exec(idata, "STARTTLS", IMAP_CMD_FAIL_OK);
        if (rc == -1)
          goto bail;
        if (rc != -2)
        {
          if (mutt_ssl_starttls(idata->conn))
          {
            mutt_error(_("Could not negotiate TLS connection"));
            goto err_close_conn;
          }
          else
          {
            /* RFC2595 demands we recheck CAPABILITY after TLS completes. */
            if (imap_exec(idata, "CAPABILITY", 0))
              goto bail;
          }
        }
      }
    }

    if (SslForceTls && !idata->conn->ssf)
    {
      mutt_error(_("Encrypted connection unavailable"));
      goto err_close_conn;
    }
#endif
  }
  else if (mutt_str_strncasecmp("* PREAUTH", idata->buf, 9) == 0)
  {
    idata->state = IMAP_AUTHENTICATED;
    if (check_capabilities(idata) != 0)
      goto bail;
    FREE(&idata->capstr);
  }
  else
  {
    imap_error("imap_open_connection()", buf);
    goto bail;
  }

  return 0;

#ifdef USE_SSL
err_close_conn:
  imap_close_connection(idata);
#endif
bail:
  FREE(&idata->capstr);
  return -1;
}

/**
 * imap_close_connection - Close an IMAP connection
 * @param idata Server data
 */
void imap_close_connection(struct ImapData *idata)
{
  if (idata->state != IMAP_DISCONNECTED)
  {
    mutt_socket_close(idata->conn);
    idata->state = IMAP_DISCONNECTED;
  }
  idata->seqno = idata->nextcmd = idata->lastcmd = idata->status = false;
  memset(idata->cmds, 0, sizeof(struct ImapCommand) * idata->cmdslots);
}

/**
 * imap_logout - Gracefully log out of server
 * @param idata Server data
 */
void imap_logout(struct ImapData **idata)
{
  /* we set status here to let imap_handle_untagged know we _expect_ to
   * receive a bye response (so it doesn't freak out and close the conn) */
  (*idata)->status = IMAP_BYE;
  imap_cmd_start(*idata, "LOGOUT");
  if (ImapPollTimeout <= 0 || mutt_socket_poll((*idata)->conn, ImapPollTimeout) != 0)
  {
    while (imap_cmd_step(*idata) == IMAP_CMD_CONTINUE)
      ;
  }

  mutt_socket_close((*idata)->conn);
  imap_free_idata(idata);
}

/**
 * imap_has_flag - Does the flag exist in the list
 * @param flag_list List of server flags
 * @param flag      Flag to find
 * @retval true Flag exists
 *
 * Do a caseless comparison of the flag against a flag list, return true if
 * found or flag list has '\*'.
 */
bool imap_has_flag(struct ListHead *flag_list, const char *flag)
{
  if (STAILQ_EMPTY(flag_list))
    return false;

  struct ListNode *np;
  STAILQ_FOREACH(np, flag_list, entries)
  {
    if (mutt_str_strncasecmp(np->data, flag, strlen(np->data)) == 0)
      return true;

    if (mutt_str_strncmp(np->data, "\\*", strlen(np->data)) == 0)
      return true;
  }

  return false;
}

/**
 * imap_exec_msgset - Prepare commands for all messages matching conditions
 * @param idata   ImapData containing context containing header set
 * @param pre     prefix commands
 * @param post    postfix commands
 * @param flag    flag type on which to filter, e.g. MUTT_REPLIED
 * @param changed include only changed messages in message set
 * @param invert  invert sense of flag, eg MUTT_READ matches unread messages
 * @retval num Matched messages
 * @retval -1  Failure
 *
 * pre/post: commands are of the form "%s %s %s %s", tag, pre, message set, post
 * Prepares commands for all messages matching conditions
 * (must be flushed with imap_exec)
 */
int imap_exec_msgset(struct ImapData *idata, const char *pre, const char *post,
                     int flag, int changed, int invert)
{
  struct Header **hdrs = NULL;
  short oldsort;
  int pos;
  int rc;
  int count = 0;

  struct Buffer *cmd = mutt_buffer_new();

  /* We make a copy of the headers just in case resorting doesn't give
   exactly the original order (duplicate messages?), because other parts of
   the ctx are tied to the header order. This may be overkill. */
  oldsort = Sort;
  if (Sort != SORT_ORDER)
  {
    hdrs = idata->ctx->hdrs;
    idata->ctx->hdrs = mutt_mem_malloc(idata->ctx->msgcount * sizeof(struct Header *));
    memcpy(idata->ctx->hdrs, hdrs, idata->ctx->msgcount * sizeof(struct Header *));

    Sort = SORT_ORDER;
    qsort(idata->ctx->hdrs, idata->ctx->msgcount, sizeof(struct Header *),
          mutt_get_sort_func(SORT_ORDER));
  }

  pos = 0;

  do
  {
    cmd->dptr = cmd->data;
    mutt_buffer_printf(cmd, "%s ", pre);
    rc = make_msg_set(idata, cmd, flag, changed, invert, &pos);
    if (rc > 0)
    {
      mutt_buffer_printf(cmd, " %s", post);
      if (imap_exec(idata, cmd->data, IMAP_CMD_QUEUE))
      {
        rc = -1;
        goto out;
      }
      count += rc;
    }
  } while (rc > 0);

  rc = count;

out:
  mutt_buffer_free(&cmd);
  if (oldsort != Sort)
  {
    Sort = oldsort;
    FREE(&idata->ctx->hdrs);
    idata->ctx->hdrs = hdrs;
  }

  return rc;
}

/**
 * imap_sync_message_for_copy - Update server to reflect the flags of a single message
 * @param[in]  idata        Server data
 * @param[in]  hdr          Header of the email
 * @param[in]  cmd          Buffer for the command string
 * @param[out] err_continue Did the user force a continue?
 * @retval  0 Success
 * @retval -1 Failure
 *
 * Update the IMAP server to reflect the flags for a single message before
 * performing a "UID COPY".
 *
 * @note This does not sync the "deleted" flag state, because it is not
 *       desirable to propagate that flag into the copy.
 */
int imap_sync_message_for_copy(struct ImapData *idata, struct Header *hdr,
                               struct Buffer *cmd, int *err_continue)
{
  char flags[LONG_STRING];
  char *tags;
  char uid[11];

  if (!compare_flags_for_copy(hdr))
  {
    if (hdr->deleted == HEADER_DATA(hdr)->deleted)
      hdr->changed = false;
    return 0;
  }

  snprintf(uid, sizeof(uid), "%u", HEADER_DATA(hdr)->uid);
  cmd->dptr = cmd->data;
  mutt_buffer_addstr(cmd, "UID STORE ");
  mutt_buffer_addstr(cmd, uid);

  flags[0] = '\0';

  set_flag(idata, MUTT_ACL_SEEN, hdr->read, "\\Seen ", flags, sizeof(flags));
  set_flag(idata, MUTT_ACL_WRITE, hdr->old, "Old ", flags, sizeof(flags));
  set_flag(idata, MUTT_ACL_WRITE, hdr->flagged, "\\Flagged ", flags, sizeof(flags));
  set_flag(idata, MUTT_ACL_WRITE, hdr->replied, "\\Answered ", flags, sizeof(flags));
  set_flag(idata, MUTT_ACL_DELETE, HEADER_DATA(hdr)->deleted, "\\Deleted ",
           flags, sizeof(flags));

  if (mutt_bit_isset(idata->ctx->rights, MUTT_ACL_WRITE))
  {
    /* restore system flags */
    if (HEADER_DATA(hdr)->flags_system)
      mutt_str_strcat(flags, sizeof(flags), HEADER_DATA(hdr)->flags_system);
    /* set custom flags */
    tags = driver_tags_get_with_hidden(&hdr->tags);
    if (tags)
    {
      mutt_str_strcat(flags, sizeof(flags), tags);
      FREE(&tags);
    }
  }

  mutt_str_remove_trailing_ws(flags);

  /* UW-IMAP is OK with null flags, Cyrus isn't. The only solution is to
   * explicitly revoke all system flags (if we have permission) */
  if (!*flags)
  {
    set_flag(idata, MUTT_ACL_SEEN, 1, "\\Seen ", flags, sizeof(flags));
    set_flag(idata, MUTT_ACL_WRITE, 1, "Old ", flags, sizeof(flags));
    set_flag(idata, MUTT_ACL_WRITE, 1, "\\Flagged ", flags, sizeof(flags));
    set_flag(idata, MUTT_ACL_WRITE, 1, "\\Answered ", flags, sizeof(flags));
    set_flag(idata, MUTT_ACL_DELETE, !HEADER_DATA(hdr)->deleted, "\\Deleted ",
             flags, sizeof(flags));

    /* erase custom flags */
    if (mutt_bit_isset(idata->ctx->rights, MUTT_ACL_WRITE) && HEADER_DATA(hdr)->flags_remote)
      mutt_str_strcat(flags, sizeof(flags), HEADER_DATA(hdr)->flags_remote);

    mutt_str_remove_trailing_ws(flags);

    mutt_buffer_addstr(cmd, " -FLAGS.SILENT (");
  }
  else
    mutt_buffer_addstr(cmd, " FLAGS.SILENT (");

  mutt_buffer_addstr(cmd, flags);
  mutt_buffer_addstr(cmd, ")");

  /* dumb hack for bad UW-IMAP 4.7 servers spurious FLAGS updates */
  hdr->active = false;

  /* after all this it's still possible to have no flags, if you
   * have no ACL rights */
  if (*flags && (imap_exec(idata, cmd->data, 0) != 0) && err_continue &&
      (*err_continue != MUTT_YES))
  {
    *err_continue = imap_continue("imap_sync_message: STORE failed", idata->buf);
    if (*err_continue != MUTT_YES)
    {
      hdr->active = true;
      return -1;
    }
  }

  /* server have now the updated flags */
  FREE(&HEADER_DATA(hdr)->flags_remote);
  HEADER_DATA(hdr)->flags_remote = driver_tags_get_with_hidden(&hdr->tags);

  hdr->active = true;
  if (hdr->deleted == HEADER_DATA(hdr)->deleted)
    hdr->changed = false;

  return 0;
}

/**
 * imap_check_mailbox - use the NOOP or IDLE command to poll for new mail
 * @param ctx   Context
 * @param force Don't wait
 * @retval #MUTT_REOPENED  mailbox has been externally modified
 * @retval #MUTT_NEW_MAIL  new mail has arrived
 * @retval 0               no change
 * @retval -1              error
 */
int imap_check_mailbox(struct Context *ctx, int force)
{
  return imap_check(ctx->data, force);
}

/**
 * imap_check - Check for new mail
 * @param idata Server data
 * @param force Force a refresh
 * @retval >0 Success, e.g. #MUTT_REOPENED
 * @retval -1 Failure
 */
int imap_check(struct ImapData *idata, int force)
{
  /* overload keyboard timeout to avoid many mailbox checks in a row.
   * Most users don't like having to wait exactly when they press a key. */
  int result = 0;

  /* try IDLE first, unless force is set */
  if (!force && ImapIdle && mutt_bit_isset(idata->capabilities, IDLE) &&
      (idata->state != IMAP_IDLE || time(NULL) >= idata->lastread + ImapKeepalive))
  {
    if (imap_cmd_idle(idata) < 0)
      return -1;
  }
  if (idata->state == IMAP_IDLE)
  {
    while ((result = mutt_socket_poll(idata->conn, 0)) > 0)
    {
      if (imap_cmd_step(idata) != IMAP_CMD_CONTINUE)
      {
        mutt_debug(1, "Error reading IDLE response\n");
        return -1;
      }
    }
    if (result < 0)
    {
      mutt_debug(1, "Poll failed, disabling IDLE\n");
      mutt_bit_unset(idata->capabilities, IDLE);
    }
  }

  if ((force || (idata->state != IMAP_IDLE && time(NULL) >= idata->lastread + Timeout)) &&
      imap_exec(idata, "NOOP", IMAP_CMD_POLL) != 0)
  {
    return -1;
  }

  /* We call this even when we haven't run NOOP in case we have pending
   * changes to process, since we can reopen here. */
  imap_cmd_finish(idata);

  if (idata->check_status & IMAP_EXPUNGE_PENDING)
    result = MUTT_REOPENED;
  else if (idata->check_status & IMAP_NEWMAIL_PENDING)
    result = MUTT_NEW_MAIL;
  else if (idata->check_status & IMAP_FLAGS_PENDING)
    result = MUTT_FLAGS;

  idata->check_status = 0;

  return result;
}

/**
 * imap_buffy_check - Check for new mail in subscribed folders
 * @param check_stats Check for message stats too
 * @retval num Number of mailboxes with new mail
 * @retval 0   Failure
 *
 * Given a list of mailboxes rather than called once for each so that it can
 * batch the commands and save on round trips.
 */
int imap_buffy_check(int check_stats)
{
  struct ImapData *idata = NULL;
  struct ImapData *lastdata = NULL;
  struct Buffy *mailbox = NULL;
  char name[LONG_STRING];
  char command[LONG_STRING];
  char munged[LONG_STRING];
  int buffies = 0;

  for (mailbox = Incoming; mailbox; mailbox = mailbox->next)
  {
    /* Init newly-added mailboxes */
    if (!mailbox->magic)
    {
      if (mx_is_imap(mailbox->path))
        mailbox->magic = MUTT_IMAP;
    }

    if (mailbox->magic != MUTT_IMAP)
      continue;

    if (get_mailbox(mailbox->path, &idata, name, sizeof(name)) < 0)
    {
      mailbox->new = false;
      continue;
    }

    /* Don't issue STATUS on the selected mailbox, it will be NOOPed or
     * IDLEd elsewhere.
     * idata->mailbox may be NULL for connections other than the current
     * mailbox's, and shouldn't expand to INBOX in that case. #3216. */
    if (idata->mailbox && (imap_mxcmp(name, idata->mailbox) == 0))
    {
      mailbox->new = false;
      continue;
    }

    if (!mutt_bit_isset(idata->capabilities, IMAP4REV1) &&
        !mutt_bit_isset(idata->capabilities, STATUS))
    {
      mutt_debug(2, "Server doesn't support STATUS\n");
      continue;
    }

    if (lastdata && idata != lastdata)
    {
      /* Send commands to previous server. Sorting the buffy list
       * may prevent some infelicitous interleavings */
      if (imap_exec(lastdata, NULL, IMAP_CMD_FAIL_OK | IMAP_CMD_POLL) == -1)
        mutt_debug(1, "#1 Error polling mailboxes\n");

      lastdata = NULL;
    }

    if (!lastdata)
      lastdata = idata;

    imap_munge_mbox_name(idata, munged, sizeof(munged), name);
    if (check_stats)
    {
      snprintf(command, sizeof(command),
               "STATUS %s (UIDNEXT UIDVALIDITY UNSEEN RECENT MESSAGES)", munged);
    }
    else
    {
      snprintf(command, sizeof(command),
               "STATUS %s (UIDNEXT UIDVALIDITY UNSEEN RECENT)", munged);
    }

    if (imap_exec(idata, command, IMAP_CMD_QUEUE | IMAP_CMD_POLL) < 0)
    {
      mutt_debug(1, "Error queueing command\n");
      return 0;
    }
  }

  if (lastdata && (imap_exec(lastdata, NULL, IMAP_CMD_FAIL_OK | IMAP_CMD_POLL) == -1))
  {
    mutt_debug(1, "#2 Error polling mailboxes\n");
    return 0;
  }

  /* collect results */
  for (mailbox = Incoming; mailbox; mailbox = mailbox->next)
  {
    if (mailbox->magic == MUTT_IMAP && mailbox->new)
      buffies++;
  }

  return buffies;
}

/**
 * imap_status - Get the status of a mailbox
 * @param path  Path of mailbox
 * @param queue true if the command should be queued for the next call
 * @retval -1  Error
 * @retval >=0 Count of messages in mailbox
 *
 * If queue is true, the command will be sent now and be expected to have been
 * run on the next call (for pipelining the postponed count).
 */
int imap_status(char *path, int queue)
{
  static int queued = 0;

  struct ImapData *idata = NULL;
  char buf[LONG_STRING];
  char mbox[LONG_STRING];
  struct ImapStatus *status = NULL;

  if (get_mailbox(path, &idata, buf, sizeof(buf)) < 0)
    return -1;

  /* We are in the folder we're polling - just return the mailbox count.
   *
   * Note that imap_mxcmp() converts NULL to "INBOX", so we need to
   * make sure the idata really is open to a folder. */
  if (idata->ctx && !imap_mxcmp(buf, idata->mailbox))
    return idata->ctx->msgcount;
  else if (mutt_bit_isset(idata->capabilities, IMAP4REV1) ||
           mutt_bit_isset(idata->capabilities, STATUS))
  {
    imap_munge_mbox_name(idata, mbox, sizeof(mbox), buf);
    snprintf(buf, sizeof(buf), "STATUS %s (%s)", mbox, "MESSAGES");
    imap_unmunge_mbox_name(idata, mbox);
  }
  else
  {
    /* Server does not support STATUS, and this is not the current mailbox.
     * There is no lightweight way to check recent arrivals */
    return -1;
  }

  if (queue)
  {
    imap_exec(idata, buf, IMAP_CMD_QUEUE);
    queued = 1;
    return 0;
  }
  else if (!queued)
    imap_exec(idata, buf, 0);

  queued = 0;
  status = imap_mboxcache_get(idata, mbox, 0);
  if (status)
    return status->messages;

  return 0;
}

/**
 * imap_mboxcache_get - Open an hcache for a mailbox
 * @param idata  Server data
 * @param mbox   Mailbox to cache
 * @param create Should it be created if it doesn't exist?
 * @retval ptr  Stats of cached mailbox
 * @retval ptr  Stats of new cache entry
 * @retval NULL Not in cache and create is false
 *
 * return cached mailbox stats or NULL if create is 0
 */
struct ImapStatus *imap_mboxcache_get(struct ImapData *idata, const char *mbox, bool create)
{
  struct ImapStatus *status = NULL;
#ifdef USE_HCACHE
  header_cache_t *hc = NULL;
  void *uidvalidity = NULL;
  void *uidnext = NULL;
#endif

  struct ListNode *np;
  STAILQ_FOREACH(np, &idata->mboxcache, entries)
  {
    status = (struct ImapStatus *) np->data;

    if (imap_mxcmp(mbox, status->name) == 0)
      return status;
  }
  status = NULL;

  /* lame */
  if (create)
  {
    struct ImapStatus *scache = mutt_mem_calloc(1, sizeof(struct ImapStatus));
    scache->name = (char *) mbox;
    mutt_list_insert_tail(&idata->mboxcache, (char *) scache);
    status = imap_mboxcache_get(idata, mbox, 0);
    status->name = mutt_str_strdup(mbox);
  }

#ifdef USE_HCACHE
  hc = imap_hcache_open(idata, mbox);
  if (hc)
  {
    uidvalidity = mutt_hcache_fetch_raw(hc, "/UIDVALIDITY", 12);
    uidnext = mutt_hcache_fetch_raw(hc, "/UIDNEXT", 8);
    if (uidvalidity)
    {
      if (!status)
      {
        mutt_hcache_free(hc, &uidvalidity);
        mutt_hcache_free(hc, &uidnext);
        mutt_hcache_close(hc);
        return imap_mboxcache_get(idata, mbox, 1);
      }
      status->uidvalidity = *(unsigned int *) uidvalidity;
      status->uidnext = uidnext ? *(unsigned int *) uidnext : 0;
      mutt_debug(3, "hcache uidvalidity %u, uidnext %u\n", status->uidvalidity,
                 status->uidnext);
    }
    mutt_hcache_free(hc, &uidvalidity);
    mutt_hcache_free(hc, &uidnext);
    mutt_hcache_close(hc);
  }
#endif

  return status;
}

/**
 * imap_mboxcache_free - Free the cached ImapStatus
 * @param idata Server data
 */
void imap_mboxcache_free(struct ImapData *idata)
{
  struct ImapStatus *status = NULL;

  struct ListNode *np;
  STAILQ_FOREACH(np, &idata->mboxcache, entries)
  {
    status = (struct ImapStatus *) np->data;
    FREE(&status->name);
  }

  mutt_list_free(&idata->mboxcache);
}

/**
 * imap_search - Find a matching mailbox
 * @param ctx Context
 * @param pat Pattern to match
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_search(struct Context *ctx, const struct Pattern *pat)
{
  struct Buffer buf;
  struct ImapData *idata = ctx->data;
  for (int i = 0; i < ctx->msgcount; i++)
    ctx->hdrs[i]->matched = false;

  if (do_search(pat, 1) == 0)
    return 0;

  mutt_buffer_init(&buf);
  mutt_buffer_addstr(&buf, "UID SEARCH ");
  if (compile_search(ctx, pat, &buf) < 0)
  {
    FREE(&buf.data);
    return -1;
  }
  if (imap_exec(idata, buf.data, 0) < 0)
  {
    FREE(&buf.data);
    return -1;
  }

  FREE(&buf.data);
  return 0;
}

/**
 * imap_subscribe - Subscribe to a mailbox
 * @param path      Mailbox path
 * @param subscribe True: subscribe, false: unsubscribe
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_subscribe(char *path, bool subscribe)
{
  struct ImapData *idata = NULL;
  char buf[LONG_STRING];
  char mbox[LONG_STRING];
  char errstr[STRING];
  struct Buffer err, token;
  struct ImapMbox mx;

  if (!mx_is_imap(path) || imap_parse_path(path, &mx) || !mx.mbox)
  {
    mutt_error(_("Bad mailbox name"));
    return -1;
  }
  idata = imap_conn_find(&(mx.account), 0);
  if (!idata)
    goto fail;

  imap_fix_path(idata, mx.mbox, buf, sizeof(buf));
  if (!*buf)
    mutt_str_strfcpy(buf, "INBOX", sizeof(buf));

  if (ImapCheckSubscribed)
  {
    mutt_buffer_init(&token);
    mutt_buffer_init(&err);
    err.data = errstr;
    err.dsize = sizeof(errstr);
    snprintf(mbox, sizeof(mbox), "%smailboxes \"%s\"", subscribe ? "" : "un", path);
    if (mutt_parse_rc_line(mbox, &token, &err))
      mutt_debug(1, "Error adding subscribed mailbox: %s\n", errstr);
    FREE(&token.data);
  }

  if (subscribe)
    mutt_message(_("Subscribing to %s..."), buf);
  else
    mutt_message(_("Unsubscribing from %s..."), buf);
  imap_munge_mbox_name(idata, mbox, sizeof(mbox), buf);

  snprintf(buf, sizeof(buf), "%sSUBSCRIBE %s", subscribe ? "" : "UN", mbox);

  if (imap_exec(idata, buf, 0) < 0)
    goto fail;

  imap_unmunge_mbox_name(idata, mx.mbox);
  if (subscribe)
    mutt_message(_("Subscribed to %s"), mx.mbox);
  else
    mutt_message(_("Unsubscribed from %s"), mx.mbox);
  FREE(&mx.mbox);
  return 0;

fail:
  FREE(&mx.mbox);
  return -1;
}

/**
 * imap_complete - Try to complete an IMAP folder path
 * @param buf Buffer for result
 * @param buflen Length of buffer
 * @param path Partial mailbox name to complete
 * @retval  0 Success
 * @retval -1 Failure
 *
 * Given a partial IMAP folder path, return a string which adds as much to the
 * path as is unique
 */
int imap_complete(char *buf, size_t buflen, char *path)
{
  struct ImapData *idata = NULL;
  char list[LONG_STRING];
  char tmp[LONG_STRING];
  struct ImapList listresp;
  char completion[LONG_STRING];
  int clen;
  size_t matchlen = 0;
  int completions = 0;
  struct ImapMbox mx;
  int rc;

  if (imap_parse_path(path, &mx))
  {
    mutt_str_strfcpy(buf, path, buflen);
    return complete_hosts(buf, buflen);
  }

  /* don't open a new socket just for completion. Instead complete over
   * known mailboxes/hooks/etc */
  idata = imap_conn_find(&(mx.account), MUTT_IMAP_CONN_NONEW);
  if (!idata)
  {
    FREE(&mx.mbox);
    mutt_str_strfcpy(buf, path, buflen);
    return complete_hosts(buf, buflen);
  }

  /* reformat path for IMAP list, and append wildcard */
  /* don't use INBOX in place of "" */
  if (mx.mbox && mx.mbox[0])
    imap_fix_path(idata, mx.mbox, list, sizeof(list));
  else
    list[0] = '\0';

  /* fire off command */
  snprintf(tmp, sizeof(tmp), "%s \"\" \"%s%%\"", ImapListSubscribed ? "LSUB" : "LIST", list);

  imap_cmd_start(idata, tmp);

  /* and see what the results are */
  mutt_str_strfcpy(completion, NONULL(mx.mbox), sizeof(completion));
  idata->cmdtype = IMAP_CT_LIST;
  idata->cmddata = &listresp;
  do
  {
    listresp.name = NULL;
    rc = imap_cmd_step(idata);

    if (rc == IMAP_CMD_CONTINUE && listresp.name)
    {
      /* if the folder isn't selectable, append delimiter to force browse
       * to enter it on second tab. */
      if (listresp.noselect)
      {
        clen = strlen(listresp.name);
        listresp.name[clen++] = listresp.delim;
        listresp.name[clen] = '\0';
      }
      /* copy in first word */
      if (!completions)
      {
        mutt_str_strfcpy(completion, listresp.name, sizeof(completion));
        matchlen = strlen(completion);
        completions++;
        continue;
      }

      matchlen = longest_common_prefix(completion, listresp.name, 0, matchlen);
      completions++;
    }
  } while (rc == IMAP_CMD_CONTINUE);
  idata->cmddata = NULL;

  if (completions)
  {
    /* reformat output */
    imap_qualify_path(buf, buflen, &mx, completion);
    mutt_pretty_mailbox(buf, buflen);

    FREE(&mx.mbox);
    return 0;
  }

  return -1;
}

/**
 * imap_fast_trash - Use server COPY command to copy deleted messages to trash
 * @param ctx  Context
 * @param dest Mailbox to move to
 * @retval -1 Error
 * @retval  0 Success
 * @retval  1 Non-fatal error - try fetch/append
 */
int imap_fast_trash(struct Context *ctx, char *dest)
{
  char mbox[LONG_STRING];
  char mmbox[LONG_STRING];
  char prompt[LONG_STRING];
  int rc;
  struct ImapMbox mx;
  bool triedcreate = false;
  struct Buffer *sync_cmd = NULL;
  int err_continue = MUTT_NO;

  struct ImapData *idata = ctx->data;

  if (imap_parse_path(dest, &mx))
  {
    mutt_debug(1, "bad destination %s\n", dest);
    return -1;
  }

  /* check that the save-to folder is in the same account */
  if (mutt_account_match(&(idata->conn->account), &(mx.account)) == 0)
  {
    mutt_debug(3, "%s not same server as %s\n", dest, ctx->path);
    return 1;
  }

  imap_fix_path(idata, mx.mbox, mbox, sizeof(mbox));
  if (!*mbox)
    mutt_str_strfcpy(mbox, "INBOX", sizeof(mbox));
  imap_munge_mbox_name(idata, mmbox, sizeof(mmbox), mbox);

  sync_cmd = mutt_buffer_new();
  for (int i = 0; i < ctx->msgcount; i++)
  {
    if (ctx->hdrs[i]->active && ctx->hdrs[i]->changed &&
        ctx->hdrs[i]->deleted && !ctx->hdrs[i]->purge)
    {
      rc = imap_sync_message_for_copy(idata, ctx->hdrs[i], sync_cmd, &err_continue);
      if (rc < 0)
      {
        mutt_debug(1, "could not sync\n");
        goto out;
      }
    }
  }

  /* loop in case of TRYCREATE */
  do
  {
    rc = imap_exec_msgset(idata, "UID COPY", mmbox, MUTT_TRASH, 0, 0);
    if (!rc)
    {
      mutt_debug(1, "No messages to trash\n");
      rc = -1;
      goto out;
    }
    else if (rc < 0)
    {
      mutt_debug(1, "could not queue copy\n");
      goto out;
    }
    else
    {
      mutt_message(ngettext("Copying %d message to %s...", "Copying %d messages to %s...", rc),
                   rc, mbox);
    }

    /* let's get it on */
    rc = imap_exec(idata, NULL, IMAP_CMD_FAIL_OK);
    if (rc == -2)
    {
      if (triedcreate)
      {
        mutt_debug(1, "Already tried to create mailbox %s\n", mbox);
        break;
      }
      /* bail out if command failed for reasons other than nonexistent target */
      if (mutt_str_strncasecmp(imap_get_qualifier(idata->buf), "[TRYCREATE]", 11) != 0)
        break;
      mutt_debug(3, "server suggests TRYCREATE\n");
      snprintf(prompt, sizeof(prompt), _("Create %s?"), mbox);
      if (Confirmcreate && mutt_yesorno(prompt, 1) != MUTT_YES)
      {
        mutt_clear_error();
        goto out;
      }
      if (imap_create_mailbox(idata, mbox) < 0)
        break;
      triedcreate = true;
    }
  } while (rc == -2);

  if (rc != 0)
  {
    imap_error("imap_fast_trash", idata->buf);
    goto out;
  }

  rc = 0;

out:
  mutt_buffer_free(&sync_cmd);
  FREE(&mx.mbox);

  return (rc < 0) ? -1 : rc;
}

/**
 * imap_mbox_open - Implements MxOps::mbox_open()
 */
static int imap_mbox_open(struct Context *ctx)
{
  struct ImapData *idata = NULL;
  struct ImapStatus *status = NULL;
  char buf[PATH_MAX];
  char bufout[PATH_MAX];
  int count = 0;
  struct ImapMbox mx, pmx;
  int rc;

  if (imap_parse_path(ctx->path, &mx))
  {
    mutt_error(_("%s is an invalid IMAP path"), ctx->path);
    return -1;
  }

  /* we require a connection which isn't currently in IMAP_SELECTED state */
  idata = imap_conn_find(&(mx.account), MUTT_IMAP_CONN_NOSELECT);
  if (!idata)
    goto fail_noidata;
  if (idata->state < IMAP_AUTHENTICATED)
    goto fail;

  /* once again the context is new */
  ctx->data = idata;

  /* Clean up path and replace the one in the ctx */
  imap_fix_path(idata, mx.mbox, buf, sizeof(buf));
  if (!*buf)
    mutt_str_strfcpy(buf, "INBOX", sizeof(buf));
  FREE(&(idata->mailbox));
  idata->mailbox = mutt_str_strdup(buf);
  imap_qualify_path(buf, sizeof(buf), &mx, idata->mailbox);

  FREE(&(ctx->path));
  FREE(&(ctx->realpath));
  ctx->path = mutt_str_strdup(buf);
  ctx->realpath = mutt_str_strdup(ctx->path);

  idata->ctx = ctx;

  /* clear mailbox status */
  idata->status = false;
  memset(idata->ctx->rights, 0, sizeof(idata->ctx->rights));
  idata->new_mail_count = 0;
  idata->max_msn = 0;

  mutt_message(_("Selecting %s..."), idata->mailbox);
  imap_munge_mbox_name(idata, buf, sizeof(buf), idata->mailbox);

  /* pipeline ACL test */
  if (mutt_bit_isset(idata->capabilities, ACL))
  {
    snprintf(bufout, sizeof(bufout), "MYRIGHTS %s", buf);
    imap_exec(idata, bufout, IMAP_CMD_QUEUE);
  }
  /* assume we have all rights if ACL is unavailable */
  else
  {
    mutt_bit_set(idata->ctx->rights, MUTT_ACL_LOOKUP);
    mutt_bit_set(idata->ctx->rights, MUTT_ACL_READ);
    mutt_bit_set(idata->ctx->rights, MUTT_ACL_SEEN);
    mutt_bit_set(idata->ctx->rights, MUTT_ACL_WRITE);
    mutt_bit_set(idata->ctx->rights, MUTT_ACL_INSERT);
    mutt_bit_set(idata->ctx->rights, MUTT_ACL_POST);
    mutt_bit_set(idata->ctx->rights, MUTT_ACL_CREATE);
    mutt_bit_set(idata->ctx->rights, MUTT_ACL_DELETE);
  }
  /* pipeline the postponed count if possible */
  pmx.mbox = NULL;
  if (mx_is_imap(Postponed) && !imap_parse_path(Postponed, &pmx) &&
      mutt_account_match(&pmx.account, &mx.account))
  {
    imap_status(Postponed, 1);
  }
  FREE(&pmx.mbox);

  if (ImapCheckSubscribed)
    imap_exec(idata, "LSUB \"\" \"*\"", IMAP_CMD_QUEUE);
  snprintf(bufout, sizeof(bufout), "%s %s", ctx->readonly ? "EXAMINE" : "SELECT", buf);

  idata->state = IMAP_SELECTED;

  imap_cmd_start(idata, bufout);

  status = imap_mboxcache_get(idata, idata->mailbox, 1);

  do
  {
    char *pc = NULL;

    rc = imap_cmd_step(idata);
    if (rc != IMAP_CMD_CONTINUE)
      break;

    pc = idata->buf + 2;

    /* Obtain list of available flags here, may be overridden by a
     * PERMANENTFLAGS tag in the OK response */
    if (mutt_str_strncasecmp("FLAGS", pc, 5) == 0)
    {
      /* don't override PERMANENTFLAGS */
      if (STAILQ_EMPTY(&idata->flags))
      {
        mutt_debug(3, "Getting mailbox FLAGS\n");
        pc = get_flags(&idata->flags, pc);
        if (!pc)
          goto fail;
      }
    }
    /* PERMANENTFLAGS are massaged to look like FLAGS, then override FLAGS */
    else if (mutt_str_strncasecmp("OK [PERMANENTFLAGS", pc, 18) == 0)
    {
      mutt_debug(3, "Getting mailbox PERMANENTFLAGS\n");
      /* safe to call on NULL */
      mutt_list_free(&idata->flags);
      /* skip "OK [PERMANENT" so syntax is the same as FLAGS */
      pc += 13;
      pc = get_flags(&(idata->flags), pc);
      if (!pc)
        goto fail;
    }
    /* save UIDVALIDITY for the header cache */
    else if (mutt_str_strncasecmp("OK [UIDVALIDITY", pc, 14) == 0)
    {
      mutt_debug(3, "Getting mailbox UIDVALIDITY\n");
      pc += 3;
      pc = imap_next_word(pc);
      if (mutt_str_atoui(pc, &idata->uid_validity) < 0)
        goto fail;
      status->uidvalidity = idata->uid_validity;
    }
    else if (mutt_str_strncasecmp("OK [UIDNEXT", pc, 11) == 0)
    {
      mutt_debug(3, "Getting mailbox UIDNEXT\n");
      pc += 3;
      pc = imap_next_word(pc);
      if (mutt_str_atoui(pc, &idata->uidnext) < 0)
        goto fail;
      status->uidnext = idata->uidnext;
    }
    else
    {
      pc = imap_next_word(pc);
      if (mutt_str_strncasecmp("EXISTS", pc, 6) == 0)
      {
        count = idata->new_mail_count;
        idata->new_mail_count = 0;
      }
    }
  } while (rc == IMAP_CMD_CONTINUE);

  if (rc == IMAP_CMD_NO)
  {
    char *s = imap_next_word(idata->buf); /* skip seq */
    s = imap_next_word(s);                /* Skip response */
    mutt_error("%s", s);
    goto fail;
  }

  if (rc != IMAP_CMD_OK)
    goto fail;

  /* check for READ-ONLY notification */
  if ((mutt_str_strncasecmp(imap_get_qualifier(idata->buf), "[READ-ONLY]", 11) == 0) &&
      !mutt_bit_isset(idata->capabilities, ACL))
  {
    mutt_debug(2, "Mailbox is read-only.\n");
    ctx->readonly = true;
  }

  /* dump the mailbox flags we've found */
  if (DebugLevel > 2)
  {
    if (STAILQ_EMPTY(&idata->flags))
      mutt_debug(3, "No folder flags found\n");
    else
    {
      struct ListNode *np;
      struct Buffer flag_buffer;
      mutt_buffer_init(&flag_buffer);
      mutt_buffer_printf(&flag_buffer, "Mailbox flags: ");
      STAILQ_FOREACH(np, &idata->flags, entries)
      {
        mutt_buffer_printf(&flag_buffer, "[%s] ", np->data);
      }
      mutt_debug(3, "%s\n", flag_buffer.data);
      FREE(&flag_buffer.data);
    }
  }

  if (!(mutt_bit_isset(idata->ctx->rights, MUTT_ACL_DELETE) ||
        mutt_bit_isset(idata->ctx->rights, MUTT_ACL_SEEN) ||
        mutt_bit_isset(idata->ctx->rights, MUTT_ACL_WRITE) ||
        mutt_bit_isset(idata->ctx->rights, MUTT_ACL_INSERT)))
  {
    ctx->readonly = true;
  }

  ctx->hdrmax = count;
  ctx->hdrs = mutt_mem_calloc(count, sizeof(struct Header *));
  ctx->v2r = mutt_mem_calloc(count, sizeof(int));
  ctx->msgcount = 0;

  if (count && (imap_read_headers(idata, 1, count) < 0))
  {
    mutt_error(_("Error opening mailbox"));
    goto fail;
  }

  mutt_debug(2, "msgcount is %d\n", ctx->msgcount);
  FREE(&mx.mbox);
  return 0;

fail:
  if (idata->state == IMAP_SELECTED)
    idata->state = IMAP_AUTHENTICATED;
fail_noidata:
  FREE(&mx.mbox);
  return -1;
}

/**
 * imap_mbox_open_append - Implements MxOps::mbox_open_append()
 */
static int imap_mbox_open_append(struct Context *ctx, int flags)
{
  struct ImapData *idata = NULL;
  char mailbox[PATH_MAX];
  struct ImapMbox mx;
  int rc;

  if (imap_parse_path(ctx->path, &mx))
    return -1;

  /* in APPEND mode, we appear to hijack an existing IMAP connection -
   * ctx is brand new and mostly empty */

  idata = imap_conn_find(&(mx.account), 0);
  if (!idata)
  {
    FREE(&mx.mbox);
    return -1;
  }

  ctx->data = idata;

  imap_fix_path(idata, mx.mbox, mailbox, sizeof(mailbox));
  if (!*mailbox)
    mutt_str_strfcpy(mailbox, "INBOX", sizeof(mailbox));
  FREE(&mx.mbox);

  rc = imap_access(ctx->path);
  if (rc == 0)
    return 0;

  if (rc == -1)
    return -1;

  char buf[PATH_MAX + 64];
  snprintf(buf, sizeof(buf), _("Create %s?"), mailbox);
  if (Confirmcreate && mutt_yesorno(buf, 1) != MUTT_YES)
    return -1;

  if (imap_create_mailbox(idata, mailbox) < 0)
    return -1;

  return 0;
}

/**
 * imap_mbox_close - Implements MxOps::mbox_close()
 * @retval 0 Always
 */
static int imap_mbox_close(struct Context *ctx)
{
  struct ImapData *idata = ctx->data;
  /* Check to see if the mailbox is actually open */
  if (!idata)
    return 0;

  /* imap_mbox_open_append() borrows the struct ImapData temporarily,
   * just for the connection, but does not set idata->ctx to the
   * open-append ctx.
   *
   * So when these are equal, it means we are actually closing the
   * mailbox and should clean up idata.  Otherwise, we don't want to
   * touch idata - it's still being used.
   */
  if (ctx == idata->ctx)
  {
    if (idata->status != IMAP_FATAL && idata->state >= IMAP_SELECTED)
    {
      /* mx_mbox_close won't sync if there are no deleted messages
       * and the mailbox is unchanged, so we may have to close here */
      if (!ctx->deleted)
        imap_exec(idata, "CLOSE", IMAP_CMD_QUEUE);
      idata->state = IMAP_AUTHENTICATED;
    }

    idata->reopen &= IMAP_REOPEN_ALLOW;
    FREE(&(idata->mailbox));
    mutt_list_free(&idata->flags);
    idata->ctx = NULL;

    mutt_hash_destroy(&idata->uid_hash);
    FREE(&idata->msn_index);
    idata->msn_index_size = 0;
    idata->max_msn = 0;

    for (int i = 0; i < IMAP_CACHE_LEN; i++)
    {
      if (idata->cache[i].path)
      {
        unlink(idata->cache[i].path);
        FREE(&idata->cache[i].path);
      }
    }

    mutt_bcache_close(&idata->bcache);
  }

  /* free IMAP part of headers */
  for (int i = 0; i < ctx->msgcount; i++)
  {
    /* mailbox may not have fully loaded */
    if (ctx->hdrs[i] && ctx->hdrs[i]->data)
      imap_free_header_data((struct ImapHeaderData **) &(ctx->hdrs[i]->data));
  }

  return 0;
}

/**
 * imap_msg_open_new - Implements MxOps::msg_open_new()
 */
static int imap_msg_open_new(struct Context *ctx, struct Message *msg, struct Header *hdr)
{
  char tmp[PATH_MAX];

  mutt_mktemp(tmp, sizeof(tmp));
  msg->fp = mutt_file_fopen(tmp, "w");
  if (!msg->fp)
  {
    mutt_perror(tmp);
    return -1;
  }
  msg->path = mutt_str_strdup(tmp);
  return 0;
}

/**
 * imap_mbox_check - Implements MxOps::mbox_check()
 * @param ctx        Context
 * @param index_hint Remember our place in the index
 * @retval >0 Success, e.g. #MUTT_REOPENED
 * @retval -1 Failure
 */
static int imap_mbox_check(struct Context *ctx, int *index_hint)
{
  int rc;
  (void) index_hint;

  imap_allow_reopen(ctx);
  rc = imap_check(ctx->data, 0);
  imap_disallow_reopen(ctx);

  return rc;
}

/**
 * imap_sync_mailbox - Sync all the changes to the server
 * @param ctx     Context
 * @param expunge 0 or 1 - do expunge?
 * @retval  0 Success
 * @retval -1 Error
 */
int imap_sync_mailbox(struct Context *ctx, int expunge)
{
  struct Context *appendctx = NULL;
  struct Header *h = NULL;
  struct Header **hdrs = NULL;
  int oldsort;
  int rc;

  struct ImapData *idata = ctx->data;

  if (idata->state < IMAP_SELECTED)
  {
    mutt_debug(2, "no mailbox selected\n");
    return -1;
  }

  /* This function is only called when the calling code expects the context
   * to be changed. */
  imap_allow_reopen(ctx);

  rc = imap_check(idata, 0);
  if (rc != 0)
    return rc;

  /* if we are expunging anyway, we can do deleted messages very quickly... */
  if (expunge && mutt_bit_isset(ctx->rights, MUTT_ACL_DELETE))
  {
    rc = imap_exec_msgset(idata, "UID STORE", "+FLAGS.SILENT (\\Deleted)",
                          MUTT_DELETED, 1, 0);
    if (rc < 0)
    {
      mutt_error(_("Expunge failed"));
      goto out;
    }

    if (rc > 0)
    {
      /* mark these messages as unchanged so second pass ignores them. Done
       * here so BOGUS UW-IMAP 4.7 SILENT FLAGS updates are ignored. */
      for (int i = 0; i < ctx->msgcount; i++)
        if (ctx->hdrs[i]->deleted && ctx->hdrs[i]->changed)
          ctx->hdrs[i]->active = false;
      mutt_message(ngettext("Marking %d message deleted...",
                            "Marking %d messages deleted...", rc),
                   rc);
    }
  }

#ifdef USE_HCACHE
  idata->hcache = imap_hcache_open(idata, NULL);
#endif

  /* save messages with real (non-flag) changes */
  for (int i = 0; i < ctx->msgcount; i++)
  {
    h = ctx->hdrs[i];

    if (h->deleted)
    {
      imap_cache_del(idata, h);
#ifdef USE_HCACHE
      imap_hcache_del(idata, HEADER_DATA(h)->uid);
#endif
    }

    if (h->active && h->changed)
    {
#ifdef USE_HCACHE
      imap_hcache_put(idata, h);
#endif
      /* if the message has been rethreaded or attachments have been deleted
       * we delete the message and reupload it.
       * This works better if we're expunging, of course. */
      if ((h->env && (h->env->refs_changed || h->env->irt_changed)) ||
          h->attach_del || h->xlabel_changed)
      {
        /* L10N: The plural is choosen by the last %d, i.e. the total number */
        mutt_message(ngettext("Saving changed message... [%d/%d]",
                              "Saving changed messages... [%d/%d]", ctx->msgcount),
                     i + 1, ctx->msgcount);
        if (!appendctx)
          appendctx = mx_mbox_open(ctx->path, MUTT_APPEND | MUTT_QUIET, NULL);
        if (!appendctx)
          mutt_debug(1, "Error opening mailbox in append mode\n");
        else
          mutt_save_message_ctx(h, 1, 0, 0, appendctx);
        h->xlabel_changed = false;
      }
    }
  }

#ifdef USE_HCACHE
  imap_hcache_close(idata);
#endif

  /* presort here to avoid doing 10 resorts in imap_exec_msgset */
  oldsort = Sort;
  if (Sort != SORT_ORDER)
  {
    hdrs = ctx->hdrs;
    ctx->hdrs = mutt_mem_malloc(ctx->msgcount * sizeof(struct Header *));
    memcpy(ctx->hdrs, hdrs, ctx->msgcount * sizeof(struct Header *));

    Sort = SORT_ORDER;
    qsort(ctx->hdrs, ctx->msgcount, sizeof(struct Header *), mutt_get_sort_func(SORT_ORDER));
  }

  rc = sync_helper(idata, MUTT_ACL_DELETE, MUTT_DELETED, "\\Deleted");
  if (rc >= 0)
    rc |= sync_helper(idata, MUTT_ACL_WRITE, MUTT_FLAG, "\\Flagged");
  if (rc >= 0)
    rc |= sync_helper(idata, MUTT_ACL_WRITE, MUTT_OLD, "Old");
  if (rc >= 0)
    rc |= sync_helper(idata, MUTT_ACL_SEEN, MUTT_READ, "\\Seen");
  if (rc >= 0)
    rc |= sync_helper(idata, MUTT_ACL_WRITE, MUTT_REPLIED, "\\Answered");

  if (oldsort != Sort)
  {
    Sort = oldsort;
    FREE(&ctx->hdrs);
    ctx->hdrs = hdrs;
  }

  /* Flush the queued flags if any were changed in sync_helper. */
  if (rc > 0)
    if (imap_exec(idata, NULL, 0) != IMAP_CMD_OK)
      rc = -1;

  if (rc < 0)
  {
    if (ctx->closing)
    {
      if (mutt_yesorno(_("Error saving flags. Close anyway?"), 0) == MUTT_YES)
      {
        rc = 0;
        idata->state = IMAP_AUTHENTICATED;
        goto out;
      }
    }
    else
      mutt_error(_("Error saving flags"));
    rc = -1;
    goto out;
  }

  /* Update local record of server state to reflect the synchronization just
   * completed.  imap_read_headers always overwrites hcache-origin flags, so
   * there is no need to mutate the hcache after flag-only changes. */
  for (int i = 0; i < ctx->msgcount; i++)
  {
    HEADER_DATA(ctx->hdrs[i])->deleted = ctx->hdrs[i]->deleted;
    HEADER_DATA(ctx->hdrs[i])->flagged = ctx->hdrs[i]->flagged;
    HEADER_DATA(ctx->hdrs[i])->old = ctx->hdrs[i]->old;
    HEADER_DATA(ctx->hdrs[i])->read = ctx->hdrs[i]->read;
    HEADER_DATA(ctx->hdrs[i])->replied = ctx->hdrs[i]->replied;
    ctx->hdrs[i]->changed = false;
  }
  ctx->changed = false;

  /* We must send an EXPUNGE command if we're not closing. */
  if (expunge && !(ctx->closing) && mutt_bit_isset(ctx->rights, MUTT_ACL_DELETE))
  {
    mutt_message(_("Expunging messages from server..."));
    /* Set expunge bit so we don't get spurious reopened messages */
    idata->reopen |= IMAP_EXPUNGE_EXPECTED;
    if (imap_exec(idata, "EXPUNGE", 0) != 0)
    {
      idata->reopen &= ~IMAP_EXPUNGE_EXPECTED;
      imap_error(_("imap_sync_mailbox: EXPUNGE failed"), idata->buf);
      rc = -1;
      goto out;
    }
    idata->reopen &= ~IMAP_EXPUNGE_EXPECTED;
  }

  if (expunge && ctx->closing)
  {
    imap_exec(idata, "CLOSE", IMAP_CMD_QUEUE);
    idata->state = IMAP_AUTHENTICATED;
  }

  if (MessageCacheClean)
    imap_cache_clean(idata);

  rc = 0;

out:
  if (appendctx)
  {
    mx_fastclose_mailbox(appendctx);
    FREE(&appendctx);
  }
  return rc;
}

/**
 * imap_tags_edit - Implements MxOps::tags_edit()
 */
static int imap_tags_edit(struct Context *ctx, const char *tags, char *buf, size_t buflen)
{
  char *new = NULL;
  char *checker = NULL;
  struct ImapData *idata = (struct ImapData *) ctx->data;

  /* Check for \* flags capability */
  if (!imap_has_flag(&idata->flags, NULL))
  {
    mutt_error(_("IMAP server doesn't support custom flags"));
    return -1;
  }

  *buf = '\0';
  if (tags)
    strncpy(buf, tags, buflen);

  if (mutt_get_field("Tags: ", buf, buflen, 0) != 0)
    return -1;

  /* each keyword must be atom defined by rfc822 as:
   *
   * atom           = 1*<any CHAR except specials, SPACE and CTLs>
   * CHAR           = ( 0.-127. )
   * specials       = "(" / ")" / "<" / ">" / "@"
   *                  / "," / ";" / ":" / "\" / <">
   *                  / "." / "[" / "]"
   * SPACE          = ( 32. )
   * CTLS           = ( 0.-31., 127.)
   *
   * And must be separated by one space.
   */

  new = buf;
  checker = buf;
  SKIPWS(checker);
  while (*checker != '\0')
  {
    if (*checker < 32 || *checker >= 127 || // We allow space because it's the separator
        *checker == 40 ||                   // (
        *checker == 41 ||                   // )
        *checker == 60 ||                   // <
        *checker == 62 ||                   // >
        *checker == 64 ||                   // @
        *checker == 44 ||                   // ,
        *checker == 59 ||                   // ;
        *checker == 58 ||                   // :
        *checker == 92 ||                   // backslash
        *checker == 34 ||                   // "
        *checker == 46 ||                   // .
        *checker == 91 ||                   // [
        *checker == 93)                     // ]
    {
      mutt_error(_("Invalid IMAP flags"));
      return 0;
    }

    /* Skip duplicate space */
    while (*checker == ' ' && *(checker + 1) == ' ')
      checker++;

    /* copy char to new and go the next one */
    *new ++ = *checker++;
  }
  *new = '\0';
  new = buf; /* rewind */
  mutt_str_remove_trailing_ws(new);

  if (mutt_str_strcmp(tags, buf) == 0)
    return 0;
  return 1;
}

/**
 * imap_tags_commit - Implements MxOps::tags_commit()
 *
 * This method update the server flags on the server by
 * removing the last know custom flags of a header
 * and adds the local flags
 *
 * If everything success we push the local flags to the
 * last know custom flags (flags_remote).
 *
 * Also this method check that each flags is support by the server
 * first and remove unsupported one.
 */
static int imap_tags_commit(struct Context *ctx, struct Header *hdr, char *buf)
{
  struct Buffer *cmd = NULL;
  char uid[11];

  struct ImapData *idata = ctx->data;

  if (*buf == '\0')
    buf = NULL;

  if (!mutt_bit_isset(idata->ctx->rights, MUTT_ACL_WRITE))
    return 0;

  snprintf(uid, sizeof(uid), "%u", HEADER_DATA(hdr)->uid);

  /* Remove old custom flags */
  if (HEADER_DATA(hdr)->flags_remote)
  {
    cmd = mutt_buffer_new();
    if (!cmd)
    {
      mutt_debug(1, "unable to allocate buffer\n");
      return -1;
    }
    cmd->dptr = cmd->data;
    mutt_buffer_addstr(cmd, "UID STORE ");
    mutt_buffer_addstr(cmd, uid);
    mutt_buffer_addstr(cmd, " -FLAGS.SILENT (");
    mutt_buffer_addstr(cmd, HEADER_DATA(hdr)->flags_remote);
    mutt_buffer_addstr(cmd, ")");

    /* Should we return here, or we are fine and we could
     * continue to add new flags *
     */
    if (imap_exec(idata, cmd->data, 0) != 0)
    {
      mutt_buffer_free(&cmd);
      return -1;
    }

    mutt_buffer_free(&cmd);
  }

  /* Add new custom flags */
  if (buf)
  {
    cmd = mutt_buffer_new();
    if (!cmd)
    {
      mutt_debug(1, "fail to remove old flags\n");
      return -1;
    }
    cmd->dptr = cmd->data;
    mutt_buffer_addstr(cmd, "UID STORE ");
    mutt_buffer_addstr(cmd, uid);
    mutt_buffer_addstr(cmd, " +FLAGS.SILENT (");
    mutt_buffer_addstr(cmd, buf);
    mutt_buffer_addstr(cmd, ")");

    if (imap_exec(idata, cmd->data, 0) != 0)
    {
      mutt_debug(1, "fail to add new flags\n");
      mutt_buffer_free(&cmd);
      return -1;
    }

    mutt_buffer_free(&cmd);
  }

  /* We are good sync them */
  mutt_debug(1, "NEW TAGS: %d\n", buf);
  driver_tags_replace(&hdr->tags, buf);
  FREE(&HEADER_DATA(hdr)->flags_remote);
  HEADER_DATA(hdr)->flags_remote = driver_tags_get_with_hidden(&hdr->tags);
  return 0;
}

// clang-format off
/**
 * struct mx_imap_ops - Mailbox callback functions for IMAP mailboxes
 */
struct MxOps mx_imap_ops = {
  .mbox_open        = imap_mbox_open,
  .mbox_open_append = imap_mbox_open_append,
  .mbox_check       = imap_mbox_check,
  .mbox_sync        = NULL, /* imap syncing is handled by imap_sync_mailbox */
  .mbox_close       = imap_mbox_close,
  .msg_open         = imap_msg_open,
  .msg_open_new     = imap_msg_open_new,
  .msg_commit       = imap_msg_commit,
  .msg_close        = imap_msg_close,
  .tags_edit        = imap_tags_edit,
  .tags_commit      = imap_tags_commit,
};
// clang-format on
