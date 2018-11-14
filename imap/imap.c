/**
 * @file
 * IMAP network mailbox
 *
 * @authors
 * Copyright (C) 1996-1998,2012 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1996-1999 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2009,2012,2017 Brendan Cully <brendan@kublai.com>
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "imap_private.h"
#include "mutt/mutt.h"
#include "email/lib.h"
#include "conn/conn.h"
#include "mutt.h"
#include "imap.h"
#include "account.h"
#include "auth.h"
#include "bcache.h"
#include "commands.h"
#include "context.h"
#include "curs_lib.h"
#include "globals.h"
#include "hook.h"
#include "mailbox.h"
#include "message.h"
#include "mutt_account.h"
#include "mutt_logging.h"
#include "mutt_socket.h"
#include "muttlib.h"
#include "mx.h"
#include "pattern.h"
#include "progress.h"
#include "protos.h"
#include "sort.h"
#ifdef USE_HCACHE
#include "hcache/hcache.h"
#endif
#ifdef ENABLE_NLS
#include <libintl.h>
#endif

/* These Config Variables are only used in imap/imap.c */
bool ImapIdle; ///< Config: (imap) Use the IMAP IDLE extension to check for new mail

/**
 * check_capabilities - Make sure we can log in to this server
 * @param adata Imap Account data
 * @retval  0 Success
 * @retval -1 Failure
 */
static int check_capabilities(struct ImapAccountData *adata)
{
  if (imap_exec(adata, "CAPABILITY", 0) != 0)
  {
    imap_error("check_capabilities", adata->buf);
    return -1;
  }

  if (!(mutt_bit_isset(adata->capabilities, IMAP4) ||
        mutt_bit_isset(adata->capabilities, IMAP4REV1)))
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
  const size_t plen = mutt_str_startswith(s, "FLAGS", CASE_IGNORE);
  if (plen == 0)
  {
    mutt_debug(1, "not a FLAGS response: %s\n", s);
    return NULL;
  }
  s += plen;
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
 * @param[in]  adata  Imap Account data
 * @param[in]  aclbit Permissions, e.g. #MUTT_ACL_WRITE
 * @param[in]  flag   Does the email have the flag set?
 * @param[in]  str    Server flag name
 * @param[out] flags  Buffer for server command
 * @param[in]  flsize Length of buffer
 */
static void set_flag(struct ImapAccountData *adata, int aclbit, int flag,
                     const char *str, char *flags, size_t flsize)
{
  if (mutt_bit_isset(adata->mailbox->rights, aclbit))
    if (flag && imap_has_flag(&adata->flags, str))
      mutt_str_strcat(flags, flsize, str);
}

/**
 * make_msg_set - Make a message set
 * @param[in]  adata   Imap Account data
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
static int make_msg_set(struct ImapAccountData *adata, struct Buffer *buf,
                        int flag, bool changed, bool invert, int *pos)
{
  int count = 0;             /* number of messages in message set */
  unsigned int setstart = 0; /* start of current message range */
  int n;
  bool started = false;
  struct Email **emails = adata->mailbox->hdrs;

  for (n = *pos; n < adata->mailbox->msg_count && buf->dptr - buf->data < IMAP_MAX_CMDLEN; n++)
  {
    bool match = false; /* whether current message matches flag condition */
    /* don't include pending expunged messages */
    if (emails[n]->active)
    {
      switch (flag)
      {
        case MUTT_DELETED:
          if (emails[n]->deleted != imap_edata_get(emails[n])->deleted)
            match = invert ^ emails[n]->deleted;
          break;
        case MUTT_FLAG:
          if (emails[n]->flagged != imap_edata_get(emails[n])->flagged)
            match = invert ^ emails[n]->flagged;
          break;
        case MUTT_OLD:
          if (emails[n]->old != imap_edata_get(emails[n])->old)
            match = invert ^ emails[n]->old;
          break;
        case MUTT_READ:
          if (emails[n]->read != imap_edata_get(emails[n])->read)
            match = invert ^ emails[n]->read;
          break;
        case MUTT_REPLIED:
          if (emails[n]->replied != imap_edata_get(emails[n])->replied)
            match = invert ^ emails[n]->replied;
          break;
        case MUTT_TAG:
          if (emails[n]->tagged)
            match = true;
          break;
        case MUTT_TRASH:
          if (emails[n]->deleted && !emails[n]->purge)
            match = true;
          break;
      }
    }

    if (match && (!changed || emails[n]->changed))
    {
      count++;
      if (setstart == 0)
      {
        setstart = imap_edata_get(emails[n])->uid;
        if (!started)
        {
          mutt_buffer_add_printf(buf, "%u", imap_edata_get(emails[n])->uid);
          started = true;
        }
        else
          mutt_buffer_add_printf(buf, ",%u", imap_edata_get(emails[n])->uid);
      }
      /* tie up if the last message also matches */
      else if (n == adata->mailbox->msg_count - 1)
        mutt_buffer_add_printf(buf, ":%u", imap_edata_get(emails[n])->uid);
    }
    /* End current set if message doesn't match or we've reached the end
     * of the mailbox via inactive messages following the last match. */
    else if (setstart && (emails[n]->active || n == adata->mailbox->msg_count - 1))
    {
      if (imap_edata_get(emails[n - 1])->uid > setstart)
        mutt_buffer_add_printf(buf, ":%u", imap_edata_get(emails[n - 1])->uid);
      setstart = 0;
    }
  }

  *pos = n;

  return count;
}

/**
 * compare_flags_for_copy - Compare local flags against the server
 * @param e Email
 * @retval true  Flags have changed
 * @retval false Flags match cached server flags
 *
 * The comparison of flags EXCLUDES the deleted flag.
 */
static bool compare_flags_for_copy(struct Email *e)
{
  struct ImapEmailData *edata = e->edata;

  if (e->read != edata->read)
    return true;
  if (e->old != edata->old)
    return true;
  if (e->flagged != edata->flagged)
    return true;
  if (e->replied != edata->replied)
    return true;

  return false;
}

/**
 * sync_helper - Sync flag changes to the server
 * @param adata Imap Account data
 * @param right ACL, e.g. #MUTT_ACL_DELETE
 * @param flag  Mutt flag, e.g. MUTT_DELETED
 * @param name  Name of server flag
 * @retval >=0 Success, number of messages
 * @retval  -1 Failure
 */
static int sync_helper(struct ImapAccountData *adata, int right, int flag, const char *name)
{
  int count = 0;
  int rc;
  char buf[LONG_STRING];

  if (!adata->mailbox)
    return -1;

  if (!mutt_bit_isset(adata->mailbox->rights, right))
    return 0;

  if (right == MUTT_ACL_WRITE && !imap_has_flag(&adata->flags, name))
    return 0;

  snprintf(buf, sizeof(buf), "+FLAGS.SILENT (%s)", name);
  rc = imap_exec_msgset(adata, "UID STORE", buf, flag, true, false);
  if (rc < 0)
    return rc;
  count += rc;

  buf[0] = '-';
  rc = imap_exec_msgset(adata, "UID STORE", buf, flag, true, true);
  if (rc < 0)
    return rc;
  count += rc;

  return count;
}

/**
 * imap_prepare_mailbox - Ensure we have a valid Mailbox
 * @param m                     Mailbox
 * @param mx                    Imap Mailbox
 * @param mailbox               Buffer for tidied mailbox path
 * @param mailboxlen            Length of buffer
 * @retval  0 Success
 * @retval -1 Failure
 *
 * This method ensure we have a valid Mailbox object with the ImapAccountData
 * structure setuped and ready to use.
 */
int imap_prepare_mailbox(struct Mailbox *m, struct ImapMbox *mx, char *mailbox, size_t mailboxlen)
{
  if (!m || !m->account)
    return -1;

  if ((imap_path_probe(m->path, NULL) != MUTT_IMAP) || imap_parse_path(m->path, mx))
  {
    mutt_debug(1, "Error parsing %s\n", m->path);
    mutt_error(_("Bad mailbox name"));
    return -1;
  }

  struct ImapAccountData *adata = m->account->adata;

  mutt_account_hook(m->realpath);

  if (imap_login(adata) < 0)
    return -1;

  imap_fix_path(adata, mx->mbox, mailbox, mailboxlen);
  if (!*mailbox)
    mutt_str_strfcpy(mailbox, "INBOX", mailboxlen);
  return 0;
}

/**
 * get_mailbox - Split mailbox URI
 * @param path   Mailbox URI
 * @param adata  Imap Account data
 * @param buf    Buffer to store mailbox name
 * @param buflen Length of buffer
 * @retval  0 Success
 * @retval -1 Failure
 *
 * Split up a mailbox URI.  The connection info is stored in the ImapAccountData and
 * the mailbox name is stored in buf.
 * TODO(sileht): We should drop this method and pass a Context or Mailbox
 * object everywhere instead.
 */
int get_mailbox(const char *path, struct ImapAccountData **adata, char *buf, size_t buflen)
{
  struct ImapMbox mx;

  *adata = imap_adata_find(path, &mx);
  if (!*adata)
  {
    FREE(&mx.mbox);
    return -1;
  }

  imap_fix_path(*adata, mx.mbox, buf, buflen);
  if (!*buf)
    mutt_str_strfcpy(buf, "INBOX", buflen);
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
 * @param m   Mailbox
 * @param pat Pattern to convert
 * @param buf Buffer for result
 * @retval  0 Success
 * @retval -1 Failure
 *
 * Convert neomutt Pattern to IMAP SEARCH command containing only elements
 * that require full-text search (neomutt already has what it needs for most
 * match types, and does a better job (eg server doesn't support regexes).
 */
static int compile_search(struct Mailbox *m, const struct Pattern *pat, struct Buffer *buf)
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

          if (compile_search(m, clause, buf) < 0)
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
        imap_quote_string(term, sizeof(term), pat->p.str, false);
        mutt_buffer_addstr(buf, term);
        mutt_buffer_addch(buf, ' ');

        /* and field */
        *delim = ':';
        delim++;
        SKIPWS(delim);
        imap_quote_string(term, sizeof(term), delim, false);
        mutt_buffer_addstr(buf, term);
        break;
      case MUTT_BODY:
        mutt_buffer_addstr(buf, "BODY ");
        imap_quote_string(term, sizeof(term), pat->p.str, false);
        mutt_buffer_addstr(buf, term);
        break;
      case MUTT_WHOLE_MSG:
        mutt_buffer_addstr(buf, "TEXT ");
        imap_quote_string(term, sizeof(term), pat->p.str, false);
        mutt_buffer_addstr(buf, term);
        break;
      case MUTT_SERVERSEARCH:
      {
        struct ImapAccountData *adata = imap_adata_get(m);
        if (!mutt_bit_isset(adata->capabilities, X_GM_EXT1))
        {
          mutt_error(_("Server-side custom search not supported: %s"), pat->p.str);
          return -1;
        }
      }
        mutt_buffer_addstr(buf, "X-GM-RAW ");
        imap_quote_string(term, sizeof(term), pat->p.str, false);
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
  // struct Connection *conn = NULL;
  int rc = -1;
  size_t matchlen;

  matchlen = mutt_str_strlen(buf);
  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &AllMailboxes, entries)
  {
    if (!mutt_str_startswith(np->m->path, buf, CASE_MATCH))
      continue;

    if (rc)
    {
      mutt_str_strfcpy(buf, np->m->path, buflen);
      rc = 0;
    }
    else
      longest_common_prefix(buf, np->m->path, matchlen, buflen);
  }

#if 0
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
#endif

  return rc;
}

/**
 * imap_access2 - Check permissions on an IMAP mailbox with an existing
 * connection
 * @retval  0 Success
 * @retval <0 Failure
 *
 * TODO: ACL checks. Right now we assume if it exists we can mess with it.
 */
static int imap_access2(struct ImapAccountData *adata, char *mailbox)
{
  char buf[LONG_STRING * 2];
  char mbox[LONG_STRING];
  int rc;

  /* we may already be in the folder we're checking */
  if (mutt_str_strcmp(adata->mbox_name, mailbox) == 0)
    return 0;

  if (imap_mboxcache_get(adata, mailbox, false))
  {
    mutt_debug(3, "found %s in cache\n", mailbox);
    return 0;
  }

  imap_munge_mbox_name(adata, mbox, sizeof(mbox), mailbox);

  if (mutt_bit_isset(adata->capabilities, IMAP4REV1))
    snprintf(buf, sizeof(buf), "STATUS %s (UIDVALIDITY)", mbox);
  else if (mutt_bit_isset(adata->capabilities, STATUS))
    snprintf(buf, sizeof(buf), "STATUS %s (UID-VALIDITY)", mbox);
  else
  {
    mutt_debug(2, "STATUS not supported?\n");
    return -1;
  }

  rc = imap_exec(adata, buf, IMAP_CMD_FAIL_OK);
  if (rc < 0)
  {
    mutt_debug(1, "Can't check STATUS of %s\n", mbox);
    return rc;
  }
  return 0;
}

/**
 * imap_create_mailbox - Create a new mailbox
 * @param adata Imap Account data
 * @param mailbox Mailbox to create
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_create_mailbox(struct ImapAccountData *adata, char *mailbox)
{
  char buf[LONG_STRING * 2], mbox[LONG_STRING];

  imap_munge_mbox_name(adata, mbox, sizeof(mbox), mailbox);
  snprintf(buf, sizeof(buf), "CREATE %s", mbox);

  if (imap_exec(adata, buf, 0) != 0)
  {
    mutt_error(_("CREATE failed: %s"), imap_cmd_trailer(adata));
    return -1;
  }

  return 0;
}

/**
 * imap_access - Check permissions on an IMAP mailbox with a new connection
 * @param path Mailbox path
 * @retval  0 Success
 * @retval <0 Failure
 *
 * TODO: ACL checks. Right now we assume if it exists we can mess with it.
 * TODO: This method should take a Context as parameter to be able to reuse the
 * existing connection.
 */
int imap_access(const char *path)
{
  struct ImapAccountData *adata = NULL;
  char mailbox[LONG_STRING];
  int rc;

  rc = get_mailbox(path, &adata, mailbox, sizeof(mailbox));
  if (rc < 0)
    return -1;

  rc = imap_access2(adata, mailbox);
  return rc;
}

/**
 * imap_rename_mailbox - Rename a mailbox
 * @param adata Imap Account data
 * @param mx      Existing mailbox
 * @param newname New name for mailbox
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_rename_mailbox(struct ImapAccountData *adata, struct ImapMbox *mx, const char *newname)
{
  char oldmbox[LONG_STRING];
  char newmbox[LONG_STRING];
  int rc = 0;

  imap_munge_mbox_name(adata, oldmbox, sizeof(oldmbox), mx->mbox);
  imap_munge_mbox_name(adata, newmbox, sizeof(newmbox), newname);

  struct Buffer *b = mutt_buffer_pool_get();
  mutt_buffer_printf(b, "RENAME %s %s", oldmbox, newmbox);

  if (imap_exec(adata, mutt_b2s(b), 0) != 0)
    rc = -1;

  mutt_buffer_pool_release(&b);

  return rc;
}

/**
 * imap_delete_mailbox - Delete a mailbox
 * @param m  Mailbox
 * @param path  name of the mailbox to delete
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_delete_mailbox(struct Mailbox *m, char *path)
{
  struct ImapMbox mx;
  char buf[PATH_MAX], mbox[PATH_MAX];

  if (imap_parse_path(path, &mx) < 0)
    return -1;

  struct ImapAccountData *adata = imap_adata_get(m);

  imap_munge_mbox_name(adata, mbox, sizeof(mbox), mx.mbox);
  snprintf(buf, sizeof(buf), "DELETE %s", mbox);
  FREE(&mx.mbox);

  if (imap_exec(m->account->adata, buf, 0) != 0)
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
  struct Account *np = NULL;
  TAILQ_FOREACH(np, &AllAccounts, entries)
  {
    if (np->magic != MUTT_IMAP)
      continue;

    struct ImapAccountData *adata = np->adata;
    struct Connection *conn = adata->conn;
    if (!conn || (conn->fd < 0))
      continue;

    mutt_message(_("Closing connection to %s..."), conn->account.host);
    imap_logout((struct ImapAccountData **) &np->adata);
    mutt_clear_error();
    //XXX? mutt_socket_close(conn);
    //not nec? FREE(&adata->conn);
  }
}

/**
 * imap_read_literal - Read bytes bytes from server into file
 * @param fp    File handle for email file
 * @param adata Imap Account data
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
int imap_read_literal(FILE *fp, struct ImapAccountData *adata,
                      unsigned long bytes, struct Progress *pbar)
{
  char c;
  bool r = false;
  struct Buffer *buf = NULL;

  if (DebugLevel >= IMAP_LOG_LTRL)
    buf = mutt_buffer_alloc(bytes + 10);

  mutt_debug(2, "reading %ld bytes\n", bytes);

  for (unsigned long pos = 0; pos < bytes; pos++)
  {
    if (mutt_socket_readchar(adata->conn, &c) != 1)
    {
      mutt_debug(1, "error during read, %ld bytes read\n", pos);
      adata->status = IMAP_FATAL;

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
 * @param adata Imap Account data
 *
 * Purge IMAP portion of expunged messages from the context. Must not be done
 * while something has a handle on any headers (eg inside pager or editor).
 * That is, check IMAP_REOPEN_ALLOW.
 */
void imap_expunge_mailbox(struct ImapAccountData *adata)
{
  struct Email *e = NULL;
  int cacheno;
  short old_sort;

#ifdef USE_HCACHE
  adata->hcache = imap_hcache_open(adata, NULL);
#endif

  old_sort = Sort;
  Sort = SORT_ORDER;
  mutt_sort_headers(adata->ctx, false);

  for (int i = 0; i < adata->mailbox->msg_count; i++)
  {
    e = adata->mailbox->hdrs[i];

    if (e->index == INT_MAX)
    {
      mutt_debug(2, "Expunging message UID %u.\n", imap_edata_get(e)->uid);

      e->active = false;
      adata->mailbox->size -= e->content->length;

      imap_cache_del(adata, e);
#ifdef USE_HCACHE
      imap_hcache_del(adata, imap_edata_get(e)->uid);
#endif

      /* free cached body from disk, if necessary */
      cacheno = imap_edata_get(e)->uid % IMAP_CACHE_LEN;
      if (adata->cache[cacheno].uid == imap_edata_get(e)->uid &&
          adata->cache[cacheno].path)
      {
        unlink(adata->cache[cacheno].path);
        FREE(&adata->cache[cacheno].path);
      }

      mutt_hash_int_delete(adata->uid_hash, imap_edata_get(e)->uid, e);

      imap_edata_free((void **) &e->edata);
    }
    else
    {
      e->index = i;
      /* NeoMutt has several places where it turns off e->active as a
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
      e->active = true;
    }
  }

#ifdef USE_HCACHE
  imap_hcache_close(adata);
#endif

  /* We may be called on to expunge at any time. We can't rely on the caller
   * to always know to rethread */
  mx_update_tables(adata->ctx, false);
  Sort = old_sort;
  mutt_sort_headers(adata->ctx, true);
}

/**
 * imap_open_connection - Open an IMAP connection
 * @param adata Imap Account data
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_open_connection(struct ImapAccountData *adata)
{
  char buf[LONG_STRING];

  if (mutt_socket_open(adata->conn) < 0)
    return -1;

  adata->state = IMAP_CONNECTED;

  if (imap_cmd_step(adata) != IMAP_CMD_OK)
  {
    imap_close_connection(adata);
    return -1;
  }

  if (mutt_str_startswith(adata->buf, "* OK", CASE_IGNORE))
  {
    if (!mutt_str_startswith(adata->buf, "* OK [CAPABILITY", CASE_IGNORE) &&
        check_capabilities(adata))
    {
      goto bail;
    }
#ifdef USE_SSL
    /* Attempt STARTTLS if available and desired. */
    if (!adata->conn->ssf && (SslForceTls || mutt_bit_isset(adata->capabilities, STARTTLS)))
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
        rc = imap_exec(adata, "STARTTLS", IMAP_CMD_FAIL_OK);
        if (rc == -1)
          goto bail;
        if (rc != -2)
        {
          if (mutt_ssl_starttls(adata->conn))
          {
            mutt_error(_("Could not negotiate TLS connection"));
            goto err_close_conn;
          }
          else
          {
            /* RFC2595 demands we recheck CAPABILITY after TLS completes. */
            if (imap_exec(adata, "CAPABILITY", 0))
              goto bail;
          }
        }
      }
    }

    if (SslForceTls && !adata->conn->ssf)
    {
      mutt_error(_("Encrypted connection unavailable"));
      goto err_close_conn;
    }
#endif
  }
  else if (mutt_str_startswith(adata->buf, "* PREAUTH", CASE_IGNORE))
  {
    adata->state = IMAP_AUTHENTICATED;
    if (check_capabilities(adata) != 0)
      goto bail;
    FREE(&adata->capstr);
  }
  else
  {
    imap_error("imap_open_connection()", buf);
    goto bail;
  }

  return 0;

#ifdef USE_SSL
err_close_conn:
  imap_close_connection(adata);
#endif
bail:
  FREE(&adata->capstr);
  return -1;
}

/**
 * imap_close_connection - Close an IMAP connection
 * @param adata Imap Account data
 */
void imap_close_connection(struct ImapAccountData *adata)
{
  if (adata->state != IMAP_DISCONNECTED)
  {
    mutt_socket_close(adata->conn);
    adata->state = IMAP_DISCONNECTED;
  }
  adata->seqno = false;
  adata->nextcmd = false;
  adata->lastcmd = false;
  adata->status = 0;
  memset(adata->cmds, 0, sizeof(struct ImapCommand) * adata->cmdslots);
}

/**
 * imap_logout - Gracefully log out of server
 * @param adata Imap Account data
 */
void imap_logout(struct ImapAccountData **adata)
{
  /* we set status here to let imap_handle_untagged know we _expect_ to
   * receive a bye response (so it doesn't freak out and close the conn) */
  if ((*adata)->state != IMAP_DISCONNECTED)
  {
    (*adata)->status = IMAP_BYE;
    imap_cmd_start(*adata, "LOGOUT");
    if (ImapPollTimeout <= 0 || mutt_socket_poll((*adata)->conn, ImapPollTimeout) != 0)
    {
      while (imap_cmd_step(*adata) == IMAP_CMD_CONTINUE)
        ;
    }
    mutt_socket_close((*adata)->conn);
  }

  imap_adata_free((void **) adata);
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

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, flag_list, entries)
  {
    if (mutt_str_strcasecmp(np->data, flag) == 0)
      return true;

    if (mutt_str_strcmp(np->data, "\\*") == 0)
      return true;
  }

  return false;
}

/**
 * imap_exec_msgset - Prepare commands for all messages matching conditions
 * @param adata   Imap Account data containing context containing header set
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
int imap_exec_msgset(struct ImapAccountData *adata, const char *pre,
                     const char *post, int flag, bool changed, bool invert)
{
  struct Email **emails = NULL;
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
    emails = adata->mailbox->hdrs;
    adata->mailbox->hdrs =
        mutt_mem_malloc(adata->mailbox->msg_count * sizeof(struct Email *));
    memcpy(adata->mailbox->hdrs, emails, adata->mailbox->msg_count * sizeof(struct Email *));

    Sort = SORT_ORDER;
    qsort(adata->mailbox->hdrs, adata->mailbox->msg_count,
          sizeof(struct Email *), mutt_get_sort_func(SORT_ORDER));
  }

  pos = 0;

  do
  {
    cmd->dptr = cmd->data;
    mutt_buffer_add_printf(cmd, "%s ", pre);
    rc = make_msg_set(adata, cmd, flag, changed, invert, &pos);
    if (rc > 0)
    {
      mutt_buffer_add_printf(cmd, " %s", post);
      if (imap_exec(adata, cmd->data, IMAP_CMD_QUEUE))
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
    FREE(&adata->mailbox->hdrs);
    adata->mailbox->hdrs = emails;
  }

  return rc;
}

/**
 * imap_sync_message_for_copy - Update server to reflect the flags of a single message
 * @param[in]  adata        Imap Account data
 * @param[in]  e            Email
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
int imap_sync_message_for_copy(struct ImapAccountData *adata, struct Email *e,
                               struct Buffer *cmd, int *err_continue)
{
  char flags[LONG_STRING];
  char *tags = NULL;
  char uid[11];

  if (!compare_flags_for_copy(e))
  {
    if (e->deleted == imap_edata_get(e)->deleted)
      e->changed = false;
    return 0;
  }

  snprintf(uid, sizeof(uid), "%u", imap_edata_get(e)->uid);
  cmd->dptr = cmd->data;
  mutt_buffer_addstr(cmd, "UID STORE ");
  mutt_buffer_addstr(cmd, uid);

  flags[0] = '\0';

  set_flag(adata, MUTT_ACL_SEEN, e->read, "\\Seen ", flags, sizeof(flags));
  set_flag(adata, MUTT_ACL_WRITE, e->old, "Old ", flags, sizeof(flags));
  set_flag(adata, MUTT_ACL_WRITE, e->flagged, "\\Flagged ", flags, sizeof(flags));
  set_flag(adata, MUTT_ACL_WRITE, e->replied, "\\Answered ", flags, sizeof(flags));
  set_flag(adata, MUTT_ACL_DELETE, imap_edata_get(e)->deleted, "\\Deleted ",
           flags, sizeof(flags));

  if (mutt_bit_isset(adata->mailbox->rights, MUTT_ACL_WRITE))
  {
    /* restore system flags */
    if (imap_edata_get(e)->flags_system)
      mutt_str_strcat(flags, sizeof(flags), imap_edata_get(e)->flags_system);
    /* set custom flags */
    tags = driver_tags_get_with_hidden(&e->tags);
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
    set_flag(adata, MUTT_ACL_SEEN, 1, "\\Seen ", flags, sizeof(flags));
    set_flag(adata, MUTT_ACL_WRITE, 1, "Old ", flags, sizeof(flags));
    set_flag(adata, MUTT_ACL_WRITE, 1, "\\Flagged ", flags, sizeof(flags));
    set_flag(adata, MUTT_ACL_WRITE, 1, "\\Answered ", flags, sizeof(flags));
    set_flag(adata, MUTT_ACL_DELETE, !imap_edata_get(e)->deleted, "\\Deleted ",
             flags, sizeof(flags));

    /* erase custom flags */
    if (mutt_bit_isset(adata->mailbox->rights, MUTT_ACL_WRITE) &&
        imap_edata_get(e)->flags_remote)
      mutt_str_strcat(flags, sizeof(flags), imap_edata_get(e)->flags_remote);

    mutt_str_remove_trailing_ws(flags);

    mutt_buffer_addstr(cmd, " -FLAGS.SILENT (");
  }
  else
    mutt_buffer_addstr(cmd, " FLAGS.SILENT (");

  mutt_buffer_addstr(cmd, flags);
  mutt_buffer_addstr(cmd, ")");

  /* dumb hack for bad UW-IMAP 4.7 servers spurious FLAGS updates */
  e->active = false;

  /* after all this it's still possible to have no flags, if you
   * have no ACL rights */
  if (*flags && (imap_exec(adata, cmd->data, 0) != 0) && err_continue &&
      (*err_continue != MUTT_YES))
  {
    *err_continue = imap_continue("imap_sync_message: STORE failed", adata->buf);
    if (*err_continue != MUTT_YES)
    {
      e->active = true;
      return -1;
    }
  }

  /* server have now the updated flags */
  FREE(&imap_edata_get(e)->flags_remote);
  imap_edata_get(e)->flags_remote = driver_tags_get_with_hidden(&e->tags);

  e->active = true;
  if (e->deleted == imap_edata_get(e)->deleted)
    e->changed = false;

  return 0;
}

/**
 * imap_check_mailbox - use the NOOP or IDLE command to poll for new mail
 * @param m     Mailbox
 * @param force Don't wait
 * @retval #MUTT_REOPENED  mailbox has been externally modified
 * @retval #MUTT_NEW_MAIL  new mail has arrived
 * @retval 0               no change
 * @retval -1              error
 */
int imap_check_mailbox(struct Mailbox *m, bool force)
{
  struct ImapAccountData *adata = imap_adata_get(m);
  return imap_check(adata, force);
}

/**
 * imap_check - Check for new mail
 * @param adata Imap Account data
 * @param force Force a refresh
 * @retval >0 Success, e.g. #MUTT_REOPENED
 * @retval -1 Failure
 */
int imap_check(struct ImapAccountData *adata, bool force)
{
  if (!adata || !adata->conn)
    return -1;

  /* overload keyboard timeout to avoid many mailbox checks in a row.
   * Most users don't like having to wait exactly when they press a key. */
  int result = 0;

  /* try IDLE first, unless force is set */
  if (!force && ImapIdle && mutt_bit_isset(adata->capabilities, IDLE) &&
      (adata->state != IMAP_IDLE || time(NULL) >= adata->lastread + ImapKeepalive))
  {
    if (imap_cmd_idle(adata) < 0)
      return -1;
  }
  if (adata->state == IMAP_IDLE)
  {
    while ((result = mutt_socket_poll(adata->conn, 0)) > 0)
    {
      if (imap_cmd_step(adata) != IMAP_CMD_CONTINUE)
      {
        mutt_debug(1, "Error reading IDLE response\n");
        return -1;
      }
    }
    if (result < 0)
    {
      mutt_debug(1, "Poll failed, disabling IDLE\n");
      mutt_bit_unset(adata->capabilities, IDLE);
    }
  }

  if ((force || (adata->state != IMAP_IDLE && time(NULL) >= adata->lastread + Timeout)) &&
      imap_exec(adata, "NOOP", IMAP_CMD_POLL) != 0)
  {
    return -1;
  }

  /* We call this even when we haven't run NOOP in case we have pending
   * changes to process, since we can reopen here. */
  imap_cmd_finish(adata);

  if (adata->check_status & IMAP_EXPUNGE_PENDING)
    result = MUTT_REOPENED;
  else if (adata->check_status & IMAP_NEWMAIL_PENDING)
    result = MUTT_NEW_MAIL;
  else if (adata->check_status & IMAP_FLAGS_PENDING)
    result = MUTT_FLAGS;

  adata->check_status = 0;

  return result;
}

/**
 * imap_mailbox_check - Check for new mail in subscribed folders
 * @param check_stats Check for message stats too
 * @retval num Number of mailboxes with new mail
 * @retval 0   Failure
 *
 * Given a list of mailboxes rather than called once for each so that it can
 * batch the commands and save on round trips.
 */
int imap_mailbox_check(bool check_stats)
{
  struct ImapAccountData *adata = NULL;
  struct ImapAccountData *lastdata = NULL;
  char name[LONG_STRING];
  char command[LONG_STRING * 2];
  char munged[LONG_STRING];
  int mbcount = 0;

  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &AllMailboxes, entries)
  {
    /* Init newly-added mailboxes */
    if (np->m->magic == MUTT_UNKNOWN)
    {
      if (imap_path_probe(np->m->path, NULL) == MUTT_IMAP)
        np->m->magic = MUTT_IMAP;
    }

    if (np->m->magic != MUTT_IMAP)
      continue;

    if (get_mailbox(np->m->path, &adata, name, sizeof(name)) < 0)
    {
      np->m->has_new = false;
      continue;
    }

    /* Don't issue STATUS on the selected mailbox, it will be NOOPed or
     * IDLEd elsewhere.
     * adata->mailbox may be NULL for connections other than the current
     * mailbox's, and shouldn't expand to INBOX in that case. #3216. */
    if (adata->mbox_name && (imap_mxcmp(name, adata->mbox_name) == 0))
    {
      np->m->has_new = false;
      continue;
    }

    if (!mutt_bit_isset(adata->capabilities, IMAP4REV1) &&
        !mutt_bit_isset(adata->capabilities, STATUS))
    {
      mutt_debug(2, "Server doesn't support STATUS\n");
      continue;
    }

    if (lastdata && (adata != lastdata))
    {
      /* Send commands to previous server. Sorting the mailbox list
       * may prevent some infelicitous interleavings */
      if (imap_exec(lastdata, NULL, IMAP_CMD_FAIL_OK | IMAP_CMD_POLL) == -1)
        mutt_debug(1, "#1 Error polling mailboxes\n");

      lastdata = NULL;
    }

    if (!lastdata)
      lastdata = adata;

    imap_munge_mbox_name(adata, munged, sizeof(munged), name);
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

    if (imap_exec(adata, command, IMAP_CMD_QUEUE | IMAP_CMD_POLL) < 0)
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
  STAILQ_FOREACH(np, &AllMailboxes, entries)
  {
    if ((np->m->magic == MUTT_IMAP) && np->m->has_new)
      mbcount++;
  }

  return mbcount;
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
int imap_status(const char *path, bool queue)
{
  static int queued = 0;

  struct ImapAccountData *adata = NULL;
  char buf[LONG_STRING * 2];
  char mbox[LONG_STRING];
  struct ImapStatus *status = NULL;

  if (get_mailbox(path, &adata, buf, sizeof(buf)) < 0)
    return -1;

  /* We are in the folder we're polling - just return the mailbox count.
   *
   * Note that imap_mxcmp() converts NULL to "INBOX", so we need to
   * make sure the adata really is open to a folder. */
  if (adata->mailbox && !imap_mxcmp(buf, adata->mbox_name))
    return adata->mailbox->msg_count;
  else if (mutt_bit_isset(adata->capabilities, IMAP4REV1) ||
           mutt_bit_isset(adata->capabilities, STATUS))
  {
    imap_munge_mbox_name(adata, mbox, sizeof(mbox), buf);
    snprintf(buf, sizeof(buf), "STATUS %s (%s)", mbox, "MESSAGES");
    imap_unmunge_mbox_name(adata, mbox);
  }
  else
  {
    /* Server does not support STATUS, and this is not the current mailbox.
     * There is no lightweight way to check recent arrivals */
    return -1;
  }

  if (queue)
  {
    imap_exec(adata, buf, IMAP_CMD_QUEUE);
    queued = 1;
    return 0;
  }
  else if (!queued)
    imap_exec(adata, buf, 0);

  queued = 0;
  status = imap_mboxcache_get(adata, mbox, false);
  if (status)
    return status->messages;

  return 0;
}

/**
 * imap_mboxcache_get - Open an hcache for a mailbox
 * @param adata  Imap Account data
 * @param mbox   Mailbox to cache
 * @param create Should it be created if it doesn't exist?
 * @retval ptr  Stats of cached mailbox
 * @retval ptr  Stats of new cache entry
 * @retval NULL Not in cache and create is false
 *
 * return cached mailbox stats or NULL if create is 0
 */
struct ImapStatus *imap_mboxcache_get(struct ImapAccountData *adata,
                                      const char *mbox, bool create)
{
  struct ImapStatus *status = NULL;
  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, &adata->mboxcache, entries)
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
    mutt_list_insert_tail(&adata->mboxcache, (char *) scache);
    status = imap_mboxcache_get(adata, mbox, false);
    status->name = mutt_str_strdup(mbox);
  }

#ifdef USE_HCACHE
  header_cache_t *hc = imap_hcache_open(adata, mbox);
  if (hc)
  {
    void *uidvalidity = mutt_hcache_fetch_raw(hc, "/UIDVALIDITY", 12);
    void *uidnext = mutt_hcache_fetch_raw(hc, "/UIDNEXT", 8);
    unsigned long long *modseq = mutt_hcache_fetch_raw(hc, "/MODSEQ", 7);
    if (uidvalidity)
    {
      if (!status)
      {
        mutt_hcache_free(hc, &uidvalidity);
        mutt_hcache_free(hc, &uidnext);
        mutt_hcache_free(hc, (void **) &modseq);
        mutt_hcache_close(hc);
        return imap_mboxcache_get(adata, mbox, true);
      }
      status->uidvalidity = *(unsigned int *) uidvalidity;
      status->uidnext = uidnext ? *(unsigned int *) uidnext : 0;
      status->modseq = modseq ? *modseq : 0;
      mutt_debug(3, "hcache uidvalidity %u, uidnext %u, modseq %llu\n",
                 status->uidvalidity, status->uidnext, status->modseq);
    }
    mutt_hcache_free(hc, &uidvalidity);
    mutt_hcache_free(hc, &uidnext);
    mutt_hcache_free(hc, (void **) &modseq);
    mutt_hcache_close(hc);
  }
#endif

  return status;
}

/**
 * imap_mboxcache_free - Free the cached ImapStatus
 * @param adata Imap Account data
 */
void imap_mboxcache_free(struct ImapAccountData *adata)
{
  struct ImapStatus *status = NULL;

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, &adata->mboxcache, entries)
  {
    status = (struct ImapStatus *) np->data;
    FREE(&status->name);
  }

  mutt_list_free(&adata->mboxcache);
}

/**
 * imap_search - Find a matching mailbox
 * @param m   Mailbox
 * @param pat Pattern to match
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_search(struct Mailbox *m, const struct Pattern *pat)
{
  struct Buffer buf;
  struct ImapAccountData *adata = imap_adata_get(m);
  for (int i = 0; i < m->msg_count; i++)
    m->hdrs[i]->matched = false;

  if (do_search(pat, 1) == 0)
    return 0;

  mutt_buffer_init(&buf);
  mutt_buffer_addstr(&buf, "UID SEARCH ");
  if (compile_search(m, pat, &buf) < 0)
  {
    FREE(&buf.data);
    return -1;
  }
  if (imap_exec(adata, buf.data, 0) < 0)
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
  struct ImapAccountData *adata = NULL;
  char buf[LONG_STRING * 2];
  char mbox[LONG_STRING];
  char errstr[STRING];
  struct Buffer err, token;
  size_t len = 0;
  int rc;

  rc = get_mailbox(path, &adata, buf, sizeof(buf));
  if (rc < 0)
    return -1;

  if (ImapCheckSubscribed)
  {
    mutt_buffer_init(&token);
    mutt_buffer_init(&err);
    err.data = errstr;
    err.dsize = sizeof(errstr);
    len = snprintf(mbox, sizeof(mbox), "%smailboxes ", subscribe ? "" : "un");
    imap_quote_string(mbox + len, sizeof(mbox) - len, path, true);
    if (mutt_parse_rc_line(mbox, &token, &err))
      mutt_debug(1, "Error adding subscribed mailbox: %s\n", errstr);
    FREE(&token.data);
  }

  if (subscribe)
    mutt_message(_("Subscribing to %s..."), buf);
  else
    mutt_message(_("Unsubscribing from %s..."), buf);
  imap_munge_mbox_name(adata, mbox, sizeof(mbox), buf);

  snprintf(buf, sizeof(buf), "%sSUBSCRIBE %s", subscribe ? "" : "UN", mbox);

  if (imap_exec(adata, buf, 0) < 0)
    return -1;

  imap_unmunge_mbox_name(adata, mbox);
  if (subscribe)
    mutt_message(_("Subscribed to %s"), mbox);
  else
    mutt_message(_("Unsubscribed from %s"), mbox);
  return 0;
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
  struct ImapAccountData *adata = NULL;
  char list[LONG_STRING];
  char tmp[LONG_STRING * 2];
  struct ImapList listresp;
  char completion[LONG_STRING];
  int clen;
  size_t matchlen = 0;
  int completions = 0;
  struct ImapMbox mx;
  int rc;

  adata = imap_adata_find(path, &mx);
  if (!adata)
  {
    FREE(&mx.mbox);
    mutt_str_strfcpy(buf, path, buflen);
    return complete_hosts(buf, buflen);
  }

  /* reformat path for IMAP list, and append wildcard */
  /* don't use INBOX in place of "" */
  if (mx.mbox && mx.mbox[0])
    imap_fix_path(adata, mx.mbox, list, sizeof(list));
  else
    list[0] = '\0';

  /* fire off command */
  snprintf(tmp, sizeof(tmp), "%s \"\" \"%s%%\"", ImapListSubscribed ? "LSUB" : "LIST", list);

  imap_cmd_start(adata, tmp);

  /* and see what the results are */
  mutt_str_strfcpy(completion, mx.mbox, sizeof(completion));
  adata->cmdtype = IMAP_CT_LIST;
  adata->cmddata = &listresp;
  do
  {
    listresp.name = NULL;
    rc = imap_cmd_step(adata);

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
  adata->cmddata = NULL;

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
 * @param m    Mailbox
 * @param dest Mailbox to move to
 * @retval -1 Error
 * @retval  0 Success
 * @retval  1 Non-fatal error - try fetch/append
 */
int imap_fast_trash(struct Mailbox *m, char *dest)
{
  char mbox[LONG_STRING];
  char mmbox[LONG_STRING];
  char prompt[LONG_STRING];
  int rc;
  struct ImapMbox mx;
  bool triedcreate = false;
  struct Buffer *sync_cmd = NULL;
  int err_continue = MUTT_NO;

  struct ImapAccountData *adata = imap_adata_get(m);

  if (imap_parse_path(dest, &mx))
  {
    mutt_debug(1, "bad destination %s\n", dest);
    return -1;
  }

  /* check that the save-to folder is in the same account */
  if (!mutt_account_match(&(adata->conn->account), &(mx.account)))
  {
    mutt_debug(3, "%s not same server as %s\n", dest, m->path);
    return 1;
  }

  imap_fix_path(adata, mx.mbox, mbox, sizeof(mbox));
  if (!*mbox)
    mutt_str_strfcpy(mbox, "INBOX", sizeof(mbox));
  imap_munge_mbox_name(adata, mmbox, sizeof(mmbox), mbox);

  sync_cmd = mutt_buffer_new();
  for (int i = 0; i < m->msg_count; i++)
  {
    if (m->hdrs[i]->active && m->hdrs[i]->changed && m->hdrs[i]->deleted &&
        !m->hdrs[i]->purge)
    {
      rc = imap_sync_message_for_copy(adata, m->hdrs[i], sync_cmd, &err_continue);
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
    rc = imap_exec_msgset(adata, "UID COPY", mmbox, MUTT_TRASH, false, false);
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
    rc = imap_exec(adata, NULL, IMAP_CMD_FAIL_OK);
    if (rc == -2)
    {
      if (triedcreate)
      {
        mutt_debug(1, "Already tried to create mailbox %s\n", mbox);
        break;
      }
      /* bail out if command failed for reasons other than nonexistent target */
      if (!mutt_str_startswith(imap_get_qualifier(adata->buf), "[TRYCREATE]", CASE_IGNORE))
        break;
      mutt_debug(3, "server suggests TRYCREATE\n");
      snprintf(prompt, sizeof(prompt), _("Create %s?"), mbox);
      if (Confirmcreate && mutt_yesorno(prompt, 1) != MUTT_YES)
      {
        mutt_clear_error();
        goto out;
      }
      if (imap_create_mailbox(adata, mbox) < 0)
        break;
      triedcreate = true;
    }
  } while (rc == -2);

  if (rc != 0)
  {
    imap_error("imap_fast_trash", adata->buf);
    goto out;
  }

  rc = 0;

out:
  mutt_buffer_free(&sync_cmd);
  FREE(&mx.mbox);

  return (rc < 0) ? -1 : rc;
}

/**
 * imap_sync_mailbox - Sync all the changes to the server
 * @param ctx     Mailbox
 * @param expunge if true do expunge
 * @param close   if true we move imap state to CLOSE
 * @retval  0 Success
 * @retval -1 Error
 */
int imap_sync_mailbox(struct Context *ctx, bool expunge, bool close)
{
  if (!ctx || !ctx->mailbox)
    return -1;

  struct Email *e = NULL;
  struct Email **emails = NULL;
  int oldsort;
  int rc;

  struct Mailbox *m = ctx->mailbox;

  struct ImapAccountData *adata = imap_adata_get(m);

  if (adata->state < IMAP_SELECTED)
  {
    mutt_debug(2, "no mailbox selected\n");
    return -1;
  }

  /* This function is only called when the calling code expects the context
   * to be changed. */
  imap_allow_reopen(m);

  rc = imap_check(adata, false);
  if (rc != 0)
    return rc;

  /* if we are expunging anyway, we can do deleted messages very quickly... */
  if (expunge && mutt_bit_isset(m->rights, MUTT_ACL_DELETE))
  {
    rc = imap_exec_msgset(adata, "UID STORE", "+FLAGS.SILENT (\\Deleted)",
                          MUTT_DELETED, true, false);
    if (rc < 0)
    {
      mutt_error(_("Expunge failed"));
      return rc;
    }

    if (rc > 0)
    {
      /* mark these messages as unchanged so second pass ignores them. Done
       * here so BOGUS UW-IMAP 4.7 SILENT FLAGS updates are ignored. */
      for (int i = 0; i < m->msg_count; i++)
        if (m->hdrs[i]->deleted && m->hdrs[i]->changed)
          m->hdrs[i]->active = false;
      mutt_message(ngettext("Marking %d message deleted...",
                            "Marking %d messages deleted...", rc),
                   rc);
    }
  }

#ifdef USE_HCACHE
  adata->hcache = imap_hcache_open(adata, NULL);
#endif

  /* save messages with real (non-flag) changes */
  for (int i = 0; i < m->msg_count; i++)
  {
    e = m->hdrs[i];

    if (e->deleted)
    {
      imap_cache_del(adata, e);
#ifdef USE_HCACHE
      imap_hcache_del(adata, imap_edata_get(e)->uid);
#endif
    }

    if (e->active && e->changed)
    {
#ifdef USE_HCACHE
      imap_hcache_put(adata, e);
#endif
      /* if the message has been rethreaded or attachments have been deleted
       * we delete the message and reupload it.
       * This works better if we're expunging, of course. */
      if ((e->env && (e->env->refs_changed || e->env->irt_changed)) ||
          e->attach_del || e->xlabel_changed)
      {
        /* L10N: The plural is choosen by the last %d, i.e. the total number */
        mutt_message(ngettext("Saving changed message... [%d/%d]",
                              "Saving changed messages... [%d/%d]", m->msg_count),
                     i + 1, m->msg_count);
        mutt_save_message_ctx(e, true, false, false, ctx);
        e->xlabel_changed = false;
      }
    }
  }

#ifdef USE_HCACHE
  imap_hcache_close(adata);
#endif

  /* presort here to avoid doing 10 resorts in imap_exec_msgset */
  oldsort = Sort;
  if (Sort != SORT_ORDER)
  {
    emails = m->hdrs;
    m->hdrs = mutt_mem_malloc(m->msg_count * sizeof(struct Email *));
    memcpy(m->hdrs, emails, m->msg_count * sizeof(struct Email *));

    Sort = SORT_ORDER;
    qsort(m->hdrs, m->msg_count, sizeof(struct Email *), mutt_get_sort_func(SORT_ORDER));
  }

  rc = sync_helper(adata, MUTT_ACL_DELETE, MUTT_DELETED, "\\Deleted");
  if (rc >= 0)
    rc |= sync_helper(adata, MUTT_ACL_WRITE, MUTT_FLAG, "\\Flagged");
  if (rc >= 0)
    rc |= sync_helper(adata, MUTT_ACL_WRITE, MUTT_OLD, "Old");
  if (rc >= 0)
    rc |= sync_helper(adata, MUTT_ACL_SEEN, MUTT_READ, "\\Seen");
  if (rc >= 0)
    rc |= sync_helper(adata, MUTT_ACL_WRITE, MUTT_REPLIED, "\\Answered");

  if (oldsort != Sort)
  {
    Sort = oldsort;
    FREE(&m->hdrs);
    m->hdrs = emails;
  }

  /* Flush the queued flags if any were changed in sync_helper. */
  if (rc > 0)
    if (imap_exec(adata, NULL, 0) != IMAP_CMD_OK)
      rc = -1;

  if (rc < 0)
  {
    if (close)
    {
      if (mutt_yesorno(_("Error saving flags. Close anyway?"), 0) == MUTT_YES)
      {
        adata->state = IMAP_AUTHENTICATED;
        return 0;
      }
    }
    else
      mutt_error(_("Error saving flags"));
    return -1;
  }

  /* Update local record of server state to reflect the synchronization just
   * completed.  imap_read_headers always overwrites hcache-origin flags, so
   * there is no need to mutate the hcache after flag-only changes. */
  for (int i = 0; i < m->msg_count; i++)
  {
    imap_edata_get(m->hdrs[i])->deleted = m->hdrs[i]->deleted;
    imap_edata_get(m->hdrs[i])->flagged = m->hdrs[i]->flagged;
    imap_edata_get(m->hdrs[i])->old = m->hdrs[i]->old;
    imap_edata_get(m->hdrs[i])->read = m->hdrs[i]->read;
    imap_edata_get(m->hdrs[i])->replied = m->hdrs[i]->replied;
    m->hdrs[i]->changed = false;
  }
  m->changed = false;

  /* We must send an EXPUNGE command if we're not closing. */
  if (expunge && !close && mutt_bit_isset(m->rights, MUTT_ACL_DELETE))
  {
    mutt_message(_("Expunging messages from server..."));
    /* Set expunge bit so we don't get spurious reopened messages */
    adata->reopen |= IMAP_EXPUNGE_EXPECTED;
    if (imap_exec(adata, "EXPUNGE", 0) != 0)
    {
      adata->reopen &= ~IMAP_EXPUNGE_EXPECTED;
      imap_error(_("imap_sync_mailbox: EXPUNGE failed"), adata->buf);
      return -1;
    }
    adata->reopen &= ~IMAP_EXPUNGE_EXPECTED;
  }

  if (expunge && close)
  {
    adata->closing = true;
    imap_exec(adata, "CLOSE", IMAP_CMD_QUEUE);
    adata->state = IMAP_AUTHENTICATED;
  }

  if (MessageCacheClean)
    imap_cache_clean(adata);

  return 0;
}

/**
 * imap_ac_find - Find a Account that matches a Mailbox path
 */
struct Account *imap_ac_find(struct Account *a, const char *path)
{
  if (!a || (a->magic != MUTT_IMAP) || !path)
    return NULL;

  struct Url url;
  char tmp[PATH_MAX];
  mutt_str_strfcpy(tmp, path, sizeof(tmp));
  url_parse(&url, tmp);

  struct ImapAccountData *adata = a->adata;
  struct ConnAccount *ac = &adata->conn_account;

  if (mutt_str_strcasecmp(url.host, ac->host) != 0)
    return NULL;

  if (mutt_str_strcasecmp(url.user, ac->user) != 0)
    return NULL;

  // if (mutt_str_strcmp(path, a->mailbox->realpath) == 0)
  //   return a;

  return a;
}

/**
 * imap_ac_add - Add a Mailbox to a Account
 */
int imap_ac_add(struct Account *a, struct Mailbox *m)
{
  if (!a || !m)
    return -1;

  if (m->magic != MUTT_IMAP)
    return -1;

  // if (a->type == MUTT_UNKNOWN)
  if (!a->adata)
  {
    struct ImapAccountData *adata = imap_adata_new();
    struct ImapMbox mx;
    a->magic = MUTT_IMAP;
    a->adata = adata;
    a->free_adata = imap_adata_free;

    if (imap_parse_path(m->path, &mx) < 0)
      return -1;
    FREE(&mx.mbox);
    adata->conn_account = mx.account;
    adata->conn = mutt_conn_new(&adata->conn_account);
    if (!adata->conn)
      return -1;
  }

  m->account = a;

  struct MailboxNode *np = mutt_mem_calloc(1, sizeof(*np));
  np->m = m;
  STAILQ_INSERT_TAIL(&a->mailboxes, np, entries);
  return 0;
}

/**
 * imap_login -  Open an IMAP connection
 * @param adata Imap Account data
 * @retval  0 Success
 * @retval -1 Failure
 *
 * This method ensure ImapAccountData is connected and logged to
 * the imap server.
 */
int imap_login(struct ImapAccountData *adata)
{
  if (!adata)
    return -1;

  if (adata->state == IMAP_DISCONNECTED)
    imap_open_connection(adata);
  if (adata->state == IMAP_CONNECTED)
  {
    if (imap_authenticate(adata) == IMAP_AUTH_SUCCESS)
    {
      adata->state = IMAP_AUTHENTICATED;
      FREE(&adata->capstr);
      if (adata->conn->ssf)
        mutt_debug(2, "Communication encrypted at %d bits\n", adata->conn->ssf);
    }
    else
      mutt_account_unsetpass(&adata->conn->account);
  }
  if (adata->state == IMAP_AUTHENTICATED)
  {
    /* capabilities may have changed */
    imap_exec(adata, "CAPABILITY", IMAP_CMD_QUEUE);

    /* enable RFC6855, if the server supports that */
    if (mutt_bit_isset(adata->capabilities, ENABLE))
      imap_exec(adata, "ENABLE UTF8=ACCEPT", IMAP_CMD_QUEUE);

    /* enable QRESYNC.  Advertising QRESYNC also means CONDSTORE
     * is supported (even if not advertised), so flip that bit. */
    if (mutt_bit_isset(adata->capabilities, QRESYNC))
    {
      mutt_bit_set(adata->capabilities, CONDSTORE);
      if (ImapQResync)
        imap_exec(adata, "ENABLE QRESYNC", IMAP_CMD_QUEUE);
    }

    /* get root delimiter, '/' as default */
    adata->delim = '/';
    imap_exec(adata, "LIST \"\" \"\"", IMAP_CMD_QUEUE);

    /* we may need the root delimiter before we open a mailbox */
    imap_exec(adata, NULL, IMAP_CMD_FAIL_OK);
  }

  if (adata->state < IMAP_AUTHENTICATED)
    return -1;

  return 0;
}

/**
 * imap_mbox_open - Implements MxOps::mbox_open()
 */
static int imap_mbox_open(struct Context *ctx)
{
  if (!ctx || !ctx->mailbox)
    return -1;

  struct Mailbox *m = ctx->mailbox;
  if (!m->account)
    return -1;

  struct ImapStatus *status = NULL;
  char buf[PATH_MAX];
  char bufout[PATH_MAX];
  int count = 0;
  struct ImapMbox mx, pmx;
  int rc;
  const char *condstore = NULL;

  rc = imap_prepare_mailbox(m, &mx, buf, sizeof(buf));
  if (rc < 0)
  {
    FREE(&mx.mbox);
    return -1;
  }

  struct ImapAccountData *adata = m->account->adata;

  FREE(&(adata->mbox_name));
  adata->mbox_name = mutt_str_strdup(buf);
  imap_qualify_path(buf, sizeof(buf), &mx, adata->mbox_name);

  mutt_str_strfcpy(m->path, buf, sizeof(m->path));
  mutt_str_strfcpy(m->realpath, m->path, sizeof(m->realpath));

  // NOTE(sileht): looks like we have two not obvious loop here
  // ctx->mailbox->account->adata->ctx
  // mailbox->account->adata->mailbox
  // this is used only by imap_mbox_close() to detect if the
  // adata/mailbox is a normal or append one, looks a bit dirty
  adata->ctx = ctx;
  adata->mailbox = m;

  /* clear mailbox status */
  adata->status = 0;
  memset(adata->mailbox->rights, 0, sizeof(adata->mailbox->rights));
  adata->new_mail_count = 0;
  adata->max_msn = 0;

  mutt_message(_("Selecting %s..."), adata->mbox_name);
  imap_munge_mbox_name(adata, buf, sizeof(buf), adata->mbox_name);

  /* pipeline ACL test */
  if (mutt_bit_isset(adata->capabilities, ACL))
  {
    snprintf(bufout, sizeof(bufout), "MYRIGHTS %s", buf);
    imap_exec(adata, bufout, IMAP_CMD_QUEUE);
  }
  /* assume we have all rights if ACL is unavailable */
  else
  {
    mutt_bit_set(adata->mailbox->rights, MUTT_ACL_LOOKUP);
    mutt_bit_set(adata->mailbox->rights, MUTT_ACL_READ);
    mutt_bit_set(adata->mailbox->rights, MUTT_ACL_SEEN);
    mutt_bit_set(adata->mailbox->rights, MUTT_ACL_WRITE);
    mutt_bit_set(adata->mailbox->rights, MUTT_ACL_INSERT);
    mutt_bit_set(adata->mailbox->rights, MUTT_ACL_POST);
    mutt_bit_set(adata->mailbox->rights, MUTT_ACL_CREATE);
    mutt_bit_set(adata->mailbox->rights, MUTT_ACL_DELETE);
  }
  /* pipeline the postponed count if possible */
  pmx.mbox = NULL;
  if ((imap_path_probe(Postponed, NULL) == MUTT_IMAP) &&
      !imap_parse_path(Postponed, &pmx) && mutt_account_match(&pmx.account, &mx.account))
  {
    imap_status(Postponed, true);
  }
  FREE(&pmx.mbox);

  if (ImapCheckSubscribed)
    imap_exec(adata, "LSUB \"\" \"*\"", IMAP_CMD_QUEUE);

#ifdef USE_HCACHE
  if (mutt_bit_isset(adata->capabilities, CONDSTORE) && ImapCondStore)
    condstore = " (CONDSTORE)";
  else
#endif
    condstore = "";

  snprintf(bufout, sizeof(bufout), "%s %s%s",
           m->readonly ? "EXAMINE" : "SELECT", buf, condstore);

  adata->state = IMAP_SELECTED;

  imap_cmd_start(adata, bufout);

  status = imap_mboxcache_get(adata, adata->mbox_name, true);

  do
  {
    char *pc = NULL;

    rc = imap_cmd_step(adata);
    if (rc != IMAP_CMD_CONTINUE)
      break;

    pc = adata->buf + 2;

    /* Obtain list of available flags here, may be overridden by a
     * PERMANENTFLAGS tag in the OK response */
    if (mutt_str_startswith(pc, "FLAGS", CASE_IGNORE))
    {
      /* don't override PERMANENTFLAGS */
      if (STAILQ_EMPTY(&adata->flags))
      {
        mutt_debug(3, "Getting mailbox FLAGS\n");
        pc = get_flags(&adata->flags, pc);
        if (!pc)
          goto fail;
      }
    }
    /* PERMANENTFLAGS are massaged to look like FLAGS, then override FLAGS */
    else if (mutt_str_startswith(pc, "OK [PERMANENTFLAGS", CASE_IGNORE))
    {
      mutt_debug(3, "Getting mailbox PERMANENTFLAGS\n");
      /* safe to call on NULL */
      mutt_list_free(&adata->flags);
      /* skip "OK [PERMANENT" so syntax is the same as FLAGS */
      pc += 13;
      pc = get_flags(&(adata->flags), pc);
      if (!pc)
        goto fail;
    }
    /* save UIDVALIDITY for the header cache */
    else if (mutt_str_startswith(pc, "OK [UIDVALIDITY", CASE_IGNORE))
    {
      mutt_debug(3, "Getting mailbox UIDVALIDITY\n");
      pc += 3;
      pc = imap_next_word(pc);
      if (mutt_str_atoui(pc, &adata->uid_validity) < 0)
        goto fail;
      status->uidvalidity = adata->uid_validity;
    }
    else if (mutt_str_startswith(pc, "OK [UIDNEXT", CASE_IGNORE))
    {
      mutt_debug(3, "Getting mailbox UIDNEXT\n");
      pc += 3;
      pc = imap_next_word(pc);
      if (mutt_str_atoui(pc, &adata->uidnext) < 0)
        goto fail;
      status->uidnext = adata->uidnext;
    }
    else if (mutt_str_startswith(pc, "OK [HIGHESTMODSEQ", CASE_IGNORE))
    {
      mutt_debug(3, "Getting mailbox HIGHESTMODSEQ\n");
      pc += 3;
      pc = imap_next_word(pc);
      if (mutt_str_atoull(pc, &adata->modseq) < 0)
        goto fail;
      status->modseq = adata->modseq;
    }
    else if (mutt_str_startswith(pc, "OK [NOMODSEQ", CASE_IGNORE))
    {
      mutt_debug(3, "Mailbox has NOMODSEQ set\n");
      status->modseq = adata->modseq = 0;
    }
    else
    {
      pc = imap_next_word(pc);
      if (mutt_str_startswith(pc, "EXISTS", CASE_IGNORE))
      {
        count = adata->new_mail_count;
        adata->new_mail_count = 0;
      }
    }
  } while (rc == IMAP_CMD_CONTINUE);

  if (rc == IMAP_CMD_NO)
  {
    char *s = imap_next_word(adata->buf); /* skip seq */
    s = imap_next_word(s);                /* Skip response */
    mutt_error("%s", s);
    goto fail;
  }

  if (rc != IMAP_CMD_OK)
    goto fail;

  /* check for READ-ONLY notification */
  if (mutt_str_startswith(imap_get_qualifier(adata->buf), "[READ-ONLY]", CASE_IGNORE) &&
      !mutt_bit_isset(adata->capabilities, ACL))
  {
    mutt_debug(2, "Mailbox is read-only.\n");
    m->readonly = true;
  }

  /* dump the mailbox flags we've found */
  if (DebugLevel > 2)
  {
    if (STAILQ_EMPTY(&adata->flags))
      mutt_debug(3, "No folder flags found\n");
    else
    {
      struct ListNode *np = NULL;
      struct Buffer flag_buffer;
      mutt_buffer_init(&flag_buffer);
      mutt_buffer_printf(&flag_buffer, "Mailbox flags: ");
      STAILQ_FOREACH(np, &adata->flags, entries)
      {
        mutt_buffer_add_printf(&flag_buffer, "[%s] ", np->data);
      }
      mutt_debug(3, "%s\n", flag_buffer.data);
      FREE(&flag_buffer.data);
    }
  }

  if (!(mutt_bit_isset(adata->mailbox->rights, MUTT_ACL_DELETE) ||
        mutt_bit_isset(adata->mailbox->rights, MUTT_ACL_SEEN) ||
        mutt_bit_isset(adata->mailbox->rights, MUTT_ACL_WRITE) ||
        mutt_bit_isset(adata->mailbox->rights, MUTT_ACL_INSERT)))
  {
    m->readonly = true;
  }

  m->hdrmax = count;
  m->hdrs = mutt_mem_calloc(count, sizeof(struct Email *));
  m->v2r = mutt_mem_calloc(count, sizeof(int));
  m->msg_count = 0;
  m->msg_unread = 0;
  m->msg_flagged = 0;
  m->msg_new = 0;
  m->msg_deleted = 0;
  m->size = 0;
  m->vcount = 0;

  if (count && (imap_read_headers(adata, 1, count, true) < 0))
  {
    mutt_error(_("Error opening mailbox"));
    goto fail;
  }

  mutt_debug(2, "msg_count is %d\n", m->msg_count);
  FREE(&mx.mbox);
  return 0;

fail:
  if (adata->state == IMAP_SELECTED)
    adata->state = IMAP_AUTHENTICATED;
  FREE(&mx.mbox);
  return -1;
}

/**
 * imap_mbox_open_append - Implements MxOps::mbox_open_append()
 */
static int imap_mbox_open_append(struct Mailbox *m, int flags)
{
  if (!m)
    return -1;

  char mailbox[PATH_MAX];
  struct ImapMbox mx;
  int rc;

  /* in APPEND mode, we appear to hijack an existing IMAP connection -
   * ctx is brand new and mostly empty */
  rc = imap_prepare_mailbox(m, &mx, mailbox, sizeof(mailbox));
  FREE(&mx.mbox);
  if (rc < 0)
    return -1;

  struct ImapAccountData *adata = m->account->adata;

  rc = imap_access2(adata, mailbox);
  if (rc == 0)
    return 0;
  if (rc == -1)
    return -1;

  char buf[PATH_MAX + 64];
  snprintf(buf, sizeof(buf), _("Create %s?"), mailbox);
  if (Confirmcreate && mutt_yesorno(buf, 1) != MUTT_YES)
    return -1;

  if (imap_create_mailbox(adata, mailbox) < 0)
    return -1;

  return 0;
}

/**
 * imap_mbox_check - Implements MxOps::mbox_check()
 * @param ctx        Mailbox
 * @param index_hint Remember our place in the index
 * @retval >0 Success, e.g. #MUTT_REOPENED
 * @retval -1 Failure
 */
static int imap_mbox_check(struct Context *ctx, int *index_hint)
{
  if (!ctx || !ctx->mailbox)
    return -1;

  struct Mailbox *m = ctx->mailbox;

  imap_allow_reopen(m);
  struct ImapAccountData *adata = imap_adata_get(m);
  int rc = imap_check(adata, false);
  /* NOTE - ctx might have been changed at this point. In particular,
   * m could be NULL. Beware. */
  imap_disallow_reopen(m);

  return rc;
}

/**
 * imap_mbox_close - Implements MxOps::mbox_close()
 */
static int imap_mbox_close(struct Context *ctx)
{
  if (!ctx || !ctx->mailbox)
    return -1;

  struct Mailbox *m = ctx->mailbox;

  struct ImapAccountData *adata = imap_adata_get(m);
  /* Check to see if the mailbox is actually open */
  if (!adata)
    return 0;

  /* imap_mbox_open_append() borrows the struct ImapAccountData temporarily,
   * just for the connection, but does not set adata->ctx to the
   * open-append ctx.
   *
   * So when these are equal, it means we are actually closing the
   * mailbox and should clean up adata.  Otherwise, we don't want to
   * touch adata - it's still being used.
   */
  if (ctx == adata->ctx)
  {
    if (adata->status != IMAP_FATAL && adata->state >= IMAP_SELECTED)
    {
      /* mx_mbox_close won't sync if there are no deleted messages
       * and the mailbox is unchanged, so we may have to close here */
      if (!m->msg_deleted)
      {
        adata->closing = true;
        imap_exec(adata, "CLOSE", IMAP_CMD_QUEUE);
      }
      adata->state = IMAP_AUTHENTICATED;
    }

    adata->reopen &= IMAP_REOPEN_ALLOW;
    FREE(&(adata->mbox_name));
    mutt_list_free(&adata->flags);
    adata->mailbox = NULL;
    adata->ctx = NULL;

    mutt_hash_destroy(&adata->uid_hash);
    FREE(&adata->msn_index);
    adata->msn_index_size = 0;
    adata->max_msn = 0;

    for (int i = 0; i < IMAP_CACHE_LEN; i++)
    {
      if (adata->cache[i].path)
      {
        unlink(adata->cache[i].path);
        FREE(&adata->cache[i].path);
      }
    }

    mutt_bcache_close(&adata->bcache);
  }

  return 0;
}

/**
 * imap_msg_open_new - Implements MxOps::msg_open_new()
 */
static int imap_msg_open_new(struct Context *ctx, struct Message *msg, struct Email *e)
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
 * imap_tags_edit - Implements MxOps::tags_edit()
 */
static int imap_tags_edit(struct Mailbox *m, const char *tags, char *buf, size_t buflen)
{
  if (!m)
    return -1;

  char *new = NULL;
  char *checker = NULL;
  struct ImapAccountData *adata = imap_adata_get(m);

  /* Check for \* flags capability */
  if (!imap_has_flag(&adata->flags, NULL))
  {
    mutt_error(_("IMAP server doesn't support custom flags"));
    return -1;
  }

  *buf = '\0';
  if (tags)
    mutt_str_strfcpy(buf, tags, buflen);

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
static int imap_tags_commit(struct Context *ctx, struct Email *e, char *buf)
{
  if (!ctx || !ctx->mailbox)
    return -1;

  struct Mailbox *m = ctx->mailbox;

  struct Buffer *cmd = NULL;
  char uid[11];

  struct ImapAccountData *adata = imap_adata_get(m);

  if (*buf == '\0')
    buf = NULL;

  if (!mutt_bit_isset(adata->mailbox->rights, MUTT_ACL_WRITE))
    return 0;

  snprintf(uid, sizeof(uid), "%u", imap_edata_get(e)->uid);

  /* Remove old custom flags */
  if (imap_edata_get(e)->flags_remote)
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
    mutt_buffer_addstr(cmd, imap_edata_get(e)->flags_remote);
    mutt_buffer_addstr(cmd, ")");

    /* Should we return here, or we are fine and we could
     * continue to add new flags */
    if (imap_exec(adata, cmd->data, 0) != 0)
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

    if (imap_exec(adata, cmd->data, 0) != 0)
    {
      mutt_debug(1, "fail to add new flags\n");
      mutt_buffer_free(&cmd);
      return -1;
    }

    mutt_buffer_free(&cmd);
  }

  /* We are good sync them */
  mutt_debug(1, "NEW TAGS: %d\n", buf);
  driver_tags_replace(&e->tags, buf);
  FREE(&imap_edata_get(e)->flags_remote);
  imap_edata_get(e)->flags_remote = driver_tags_get_with_hidden(&e->tags);
  return 0;
}

/**
 * imap_path_probe - Is this an IMAP mailbox? - Implements MxOps::path_probe()
 */
enum MailboxType imap_path_probe(const char *path, const struct stat *st)
{
  if (!path)
    return MUTT_UNKNOWN;

  if (mutt_str_startswith(path, "imap://", CASE_IGNORE))
    return MUTT_IMAP;

  if (mutt_str_startswith(path, "imaps://", CASE_IGNORE))
    return MUTT_IMAP;

  return MUTT_UNKNOWN;
}

/**
 * imap_path_canon - Canonicalise a mailbox path - Implements MxOps::path_canon()
 */
int imap_path_canon(char *buf, size_t buflen)
{
  if (!buf)
    return -1;

  struct Url url;
  char tmp[PATH_MAX];
  char tmp2[PATH_MAX];
  url_parse(&url, buf);
  imap_fix_path(NULL, url.path, tmp, sizeof(tmp));
  url.path = tmp;
  url_tostring(&url, tmp2, sizeof(tmp2), 0);
  mutt_str_strfcpy(buf, tmp2, buflen);

  return 0;
}

/**
 * imap_path_pretty - Implements MxOps::path_pretty()
 */
int imap_path_pretty(char *buf, size_t buflen, const char *folder)
{
  if (!buf || !folder)
    return -1;

  imap_pretty_mailbox(buf, folder);
  return 0;
}

/**
 * imap_path_parent - Implements MxOps::path_parent()
 */
int imap_path_parent(char *buf, size_t buflen)
{
  char tmp[PATH_MAX] = { 0 };

  imap_get_parent_path(buf, tmp, sizeof(tmp));
  mutt_str_strfcpy(buf, tmp, buflen);
  return 0;
}

// clang-format off
/**
 * struct mx_imap_ops - IMAP mailbox - Implements ::MxOps
 */
struct MxOps mx_imap_ops = {
  .magic            = MUTT_IMAP,
  .name             = "imap",
  .ac_find          = imap_ac_find,
  .ac_add           = imap_ac_add,
  .mbox_open        = imap_mbox_open,
  .mbox_open_append = imap_mbox_open_append,
  .mbox_check       = imap_mbox_check,
  .mbox_sync        = NULL, /* imap syncing is handled by imap_sync_mailbox */
  .mbox_close       = imap_mbox_close,
  .msg_open         = imap_msg_open,
  .msg_open_new     = imap_msg_open_new,
  .msg_commit       = imap_msg_commit,
  .msg_close        = imap_msg_close,
  .msg_padding_size = NULL,
  .tags_edit        = imap_tags_edit,
  .tags_commit      = imap_tags_commit,
  .path_probe       = imap_path_probe,
  .path_canon       = imap_path_canon,
  .path_pretty      = imap_path_pretty,
  .path_parent      = imap_path_parent,
};
// clang-format on
