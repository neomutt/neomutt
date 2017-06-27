/**
 * Copyright (C) 1996-1998,2012 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1996-1999 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2009,2012 Brendan Cully <brendan@kublai.com>
 *
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

/* Support for IMAP4rev1, with the occasional nod to IMAP 4. */

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
#include "mutt.h"
#include "imap.h"
#include "account.h"
#include "ascii.h"
#include "bcache.h"
#include "body.h"
#include "buffer.h"
#include "buffy.h"
#include "context.h"
#include "envelope.h"
#include "globals.h"
#include "hash.h"
#include "header.h"
#include "imap/imap.h"
#include "lib.h"
#include "list.h"
#include "mailbox.h"
#include "message.h"
#include "mutt_curses.h"
#include "mutt_socket.h"
#include "mx.h"
#include "options.h"
#include "pattern.h"
#include "protos.h"
#include "sort.h"
#include "url.h"
#ifdef USE_HCACHE
#include "hcache/hcache.h"
#endif
#ifdef USE_SSL
#include "mutt_ssl.h"
#endif

/* imap forward declarations */
static char *imap_get_flags(struct List **hflags, char *s);
static int imap_check_capabilities(struct ImapData *idata);
static void imap_set_flag(struct ImapData *idata, int aclbit, int flag,
                          const char *str, char *flags, size_t flsize);

/* imap_access: Check permissions on an IMAP mailbox.
 * TODO: ACL checks. Right now we assume if it exists we can
 *       mess with it. */
int imap_access(const char *path, int flags)
{
  struct ImapData *idata = NULL;
  struct ImapMbox mx;
  char buf[LONG_STRING];
  char mailbox[LONG_STRING];
  char mbox[LONG_STRING];
  int rc;

  if (imap_parse_path(path, &mx))
    return -1;

  if (!(idata = imap_conn_find(&mx.account, option(OPTIMAPPASSIVE) ? MUTT_IMAP_CONN_NONEW : 0)))
  {
    FREE(&mx.mbox);
    return -1;
  }

  imap_fix_path(idata, mx.mbox, mailbox, sizeof(mailbox));
  if (!*mailbox)
    strfcpy(mailbox, "INBOX", sizeof(mailbox));

  /* we may already be in the folder we're checking */
  if (ascii_strcmp(idata->mailbox, mx.mbox) == 0)
  {
    FREE(&mx.mbox);
    return 0;
  }
  FREE(&mx.mbox);

  if (imap_mboxcache_get(idata, mailbox, 0))
  {
    mutt_debug(3, "imap_access: found %s in cache\n", mailbox);
    return 0;
  }

  imap_munge_mbox_name(idata, mbox, sizeof(mbox), mailbox);

  if (mutt_bit_isset(idata->capabilities, IMAP4REV1))
    snprintf(buf, sizeof(buf), "STATUS %s (UIDVALIDITY)", mbox);
  else if (mutt_bit_isset(idata->capabilities, STATUS))
    snprintf(buf, sizeof(buf), "STATUS %s (UID-VALIDITY)", mbox);
  else
  {
    mutt_debug(2, "imap_access: STATUS not supported?\n");
    return -1;
  }

  if ((rc = imap_exec(idata, buf, IMAP_CMD_FAIL_OK)) < 0)
  {
    mutt_debug(1, "imap_access: Can't check STATUS of %s\n", mbox);
    return rc;
  }

  return 0;
}

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

int imap_delete_mailbox(struct Context *ctx, struct ImapMbox *mx)
{
  char buf[LONG_STRING], mbox[LONG_STRING];
  struct ImapData *idata = NULL;

  if (!ctx || !ctx->data)
  {
    if (!(idata = imap_conn_find(&mx->account, option(OPTIMAPPASSIVE) ? MUTT_IMAP_CONN_NONEW : 0)))
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

/* imap_logout_all: close all open connections. Quick and dirty until we can
 *   make sure we've got all the context we need. */
void imap_logout_all(void)
{
  struct Connection *conn = NULL;
  struct Connection *tmp = NULL;

  conn = mutt_socket_head();

  while (conn)
  {
    tmp = conn->next;

    if (conn->account.type == MUTT_ACCT_TYPE_IMAP && conn->fd >= 0)
    {
      mutt_message(_("Closing connection to %s..."), conn->account.host);
      imap_logout((struct ImapData **) (void *) &conn->data);
      mutt_clear_error();
      mutt_socket_free(conn);
    }

    conn = tmp;
  }
}

/* imap_read_literal: read bytes bytes from server into file. Not explicitly
 *   buffered, relies on FILE buffering. NOTE: strips \r from \r\n.
 *   Apparently even literals use \r\n-terminated strings ?! */
int imap_read_literal(FILE *fp, struct ImapData *idata, long bytes, struct Progress *pbar)
{
  char c;
  bool r = false;

  mutt_debug(2, "imap_read_literal: reading %ld bytes\n", bytes);

  for (long pos = 0; pos < bytes; pos++)
  {
    if (mutt_socket_readchar(idata->conn, &c) != 1)
    {
      mutt_debug(1, "imap_read_literal: error during read, %ld bytes read\n", pos);
      idata->status = IMAP_FATAL;

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
#ifdef DEBUG
    if (debuglevel >= IMAP_LOG_LTRL)
      fputc(c, debugfile);
#endif
  }

  return 0;
}

/* imap_expunge_mailbox: Purge IMAP portion of expunged messages from the
 *   context. Must not be done while something has a handle on any headers
 *   (eg inside pager or editor). That is, check IMAP_REOPEN_ALLOW. */
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
      mutt_debug(2, "Expunging message UID %d.\n", HEADER_DATA(h)->uid);

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

      int_hash_delete(idata->uid_hash, HEADER_DATA(h)->uid, h, NULL);

      imap_free_header_data((struct ImapHeaderData **) &h->data);
    }
    else
      h->index = i;
  }

#ifdef USE_HCACHE
  imap_hcache_close(idata);
#endif

  /* We may be called on to expunge at any time. We can't rely on the caller
   * to always know to rethread */
  mx_update_tables(idata->ctx, 0);
  Sort = old_sort;
  mutt_sort_headers(idata->ctx, 1);
}

/* imap_check_capabilities: make sure we can log in to this server. */
static int imap_check_capabilities(struct ImapData *idata)
{
  if (imap_exec(idata, "CAPABILITY", 0) != 0)
  {
    imap_error("imap_check_capabilities", idata->buf);
    return -1;
  }

  if (!(mutt_bit_isset(idata->capabilities, IMAP4) ||
        mutt_bit_isset(idata->capabilities, IMAP4REV1)))
  {
    mutt_error(
        _("This IMAP server is ancient. NeoMutt does not work with it."));
    mutt_sleep(2); /* pause a moment to let the user see the error */

    return -1;
  }

  return 0;
}

/* imap_conn_find: Find an open IMAP connection matching account, or open
 *   a new one if none can be found. */
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
    if (!(idata = imap_new_idata()))
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
    if (!imap_authenticate(idata))
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
    if (option(OPTIMAPCHECKSUBSCRIBED))
      imap_exec(idata, "LSUB \"\" \"*\"", IMAP_CMD_QUEUE);
    /* we may need the root delimiter before we open a mailbox */
    imap_exec(idata, NULL, IMAP_CMD_FAIL_OK);
  }

  return idata;
}

int imap_open_connection(struct ImapData *idata)
{
  char buf[LONG_STRING];

  if (mutt_socket_open(idata->conn) < 0)
    return -1;

  idata->state = IMAP_CONNECTED;

  if (imap_cmd_step(idata) != IMAP_CMD_CONTINUE)
  {
    imap_close_connection(idata);
    return -1;
  }

  if (ascii_strncasecmp("* OK", idata->buf, 4) == 0)
  {
    if ((ascii_strncasecmp("* OK [CAPABILITY", idata->buf, 16) != 0) &&
        imap_check_capabilities(idata))
      goto bail;
#ifdef USE_SSL
    /* Attempt STARTTLS if available and desired. */
    if (!idata->conn->ssf &&
        (option(OPTSSLFORCETLS) || mutt_bit_isset(idata->capabilities, STARTTLS)))
    {
      int rc;

      if (option(OPTSSLFORCETLS))
        rc = MUTT_YES;
      else if ((rc = query_quadoption(OPT_SSLSTARTTLS,
                                      _("Secure connection with TLS?"))) == MUTT_ABORT)
        goto err_close_conn;
      if (rc == MUTT_YES)
      {
        if ((rc = imap_exec(idata, "STARTTLS", IMAP_CMD_FAIL_OK)) == -1)
          goto bail;
        if (rc != -2)
        {
          if (mutt_ssl_starttls(idata->conn))
          {
            mutt_error(_("Could not negotiate TLS connection"));
            mutt_sleep(1);
            goto err_close_conn;
          }
          else
          {
            /* RFC 2595 demands we recheck CAPABILITY after TLS completes. */
            if (imap_exec(idata, "CAPABILITY", 0))
              goto bail;
          }
        }
      }
    }

    if (option(OPTSSLFORCETLS) && !idata->conn->ssf)
    {
      mutt_error(_("Encrypted connection unavailable"));
      mutt_sleep(1);
      goto err_close_conn;
    }
#endif
  }
  else if (ascii_strncasecmp("* PREAUTH", idata->buf, 9) == 0)
  {
    idata->state = IMAP_AUTHENTICATED;
    if (imap_check_capabilities(idata) != 0)
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

/* imap_get_flags: Make a simple list out of a FLAGS response.
 *   return stream following FLAGS response */
static char *imap_get_flags(struct List **hflags, char *s)
{
  struct List *flags = NULL;
  char *flag_word = NULL;
  char ctmp;

  /* sanity-check string */
  if (ascii_strncasecmp("FLAGS", s, 5) != 0)
  {
    mutt_debug(1, "imap_get_flags: not a FLAGS response: %s\n", s);
    return NULL;
  }
  s += 5;
  SKIPWS(s);
  if (*s != '(')
  {
    mutt_debug(1, "imap_get_flags: bogus FLAGS response: %s\n", s);
    return NULL;
  }

  /* create list, update caller's flags handle */
  flags = mutt_new_list();
  *hflags = flags;

  while (*s && *s != ')')
  {
    s++;
    SKIPWS(s);
    flag_word = s;
    while (*s && (*s != ')') && !ISSPACE(*s))
      s++;
    ctmp = *s;
    *s = '\0';
    if (*flag_word)
      mutt_add_list(flags, flag_word);
    *s = ctmp;
  }

  /* note bad flags response */
  if (*s != ')')
  {
    mutt_debug(1, "imap_get_flags: Unterminated FLAGS response: %s\n", s);
    mutt_free_list(hflags);

    return NULL;
  }

  s++;

  return s;
}

static int imap_open_mailbox(struct Context *ctx)
{
  struct ImapData *idata = NULL;
  struct ImapStatus *status = NULL;
  char buf[LONG_STRING];
  char bufout[LONG_STRING];
  int count = 0;
  struct ImapMbox mx, pmx;
  int rc;

  if (imap_parse_path(ctx->path, &mx))
  {
    mutt_error(_("%s is an invalid IMAP path"), ctx->path);
    return -1;
  }

  /* we require a connection which isn't currently in IMAP_SELECTED state */
  if (!(idata = imap_conn_find(&(mx.account), MUTT_IMAP_CONN_NOSELECT)))
    goto fail_noidata;
  if (idata->state < IMAP_AUTHENTICATED)
    goto fail;

  /* once again the context is new */
  ctx->data = idata;

  /* Clean up path and replace the one in the ctx */
  imap_fix_path(idata, mx.mbox, buf, sizeof(buf));
  if (!*buf)
    strfcpy(buf, "INBOX", sizeof(buf));
  FREE(&(idata->mailbox));
  idata->mailbox = safe_strdup(buf);
  imap_qualify_path(buf, sizeof(buf), &mx, idata->mailbox);

  FREE(&(ctx->path));
  FREE(&(ctx->realpath));
  ctx->path = safe_strdup(buf);
  ctx->realpath = safe_strdup(ctx->path);

  idata->ctx = ctx;

  /* clear mailbox status */
  idata->status = false;
  memset(idata->ctx->rights, 0, sizeof(idata->ctx->rights));
  idata->newMailCount = 0;
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
    imap_status(Postponed, 1);
  FREE(&pmx.mbox);

  snprintf(bufout, sizeof(bufout), "%s %s",
           ctx->readonly ? "EXAMINE" : "SELECT", buf);

  idata->state = IMAP_SELECTED;

  imap_cmd_start(idata, bufout);

  status = imap_mboxcache_get(idata, idata->mailbox, 1);

  do
  {
    char *pc = NULL;

    if ((rc = imap_cmd_step(idata)) != IMAP_CMD_CONTINUE)
      break;

    pc = idata->buf + 2;

    /* Obtain list of available flags here, may be overridden by a
     * PERMANENTFLAGS tag in the OK response */
    if (ascii_strncasecmp("FLAGS", pc, 5) == 0)
    {
      /* don't override PERMANENTFLAGS */
      if (!idata->flags)
      {
        mutt_debug(3, "Getting mailbox FLAGS\n");
        if ((pc = imap_get_flags(&(idata->flags), pc)) == NULL)
          goto fail;
      }
    }
    /* PERMANENTFLAGS are massaged to look like FLAGS, then override FLAGS */
    else if (ascii_strncasecmp("OK [PERMANENTFLAGS", pc, 18) == 0)
    {
      mutt_debug(3, "Getting mailbox PERMANENTFLAGS\n");
      /* safe to call on NULL */
      mutt_free_list(&(idata->flags));
      /* skip "OK [PERMANENT" so syntax is the same as FLAGS */
      pc += 13;
      if ((pc = imap_get_flags(&(idata->flags), pc)) == NULL)
        goto fail;
    }
    /* save UIDVALIDITY for the header cache */
    else if (ascii_strncasecmp("OK [UIDVALIDITY", pc, 14) == 0)
    {
      mutt_debug(3, "Getting mailbox UIDVALIDITY\n");
      pc += 3;
      pc = imap_next_word(pc);
      idata->uid_validity = strtol(pc, NULL, 10);
      status->uidvalidity = idata->uid_validity;
    }
    else if (ascii_strncasecmp("OK [UIDNEXT", pc, 11) == 0)
    {
      mutt_debug(3, "Getting mailbox UIDNEXT\n");
      pc += 3;
      pc = imap_next_word(pc);
      idata->uidnext = strtol(pc, NULL, 10);
      status->uidnext = idata->uidnext;
    }
    else
    {
      pc = imap_next_word(pc);
      if (ascii_strncasecmp("EXISTS", pc, 6) == 0)
      {
        count = idata->newMailCount;
        idata->newMailCount = 0;
      }
    }
  } while (rc == IMAP_CMD_CONTINUE);

  if (rc == IMAP_CMD_NO)
  {
    char *s = NULL;
    s = imap_next_word(idata->buf); /* skip seq */
    s = imap_next_word(s);          /* Skip response */
    mutt_error("%s", s);
    mutt_sleep(2);
    goto fail;
  }

  if (rc != IMAP_CMD_OK)
    goto fail;

  /* check for READ-ONLY notification */
  if ((ascii_strncasecmp(imap_get_qualifier(idata->buf), "[READ-ONLY]", 11) == 0) &&
      !mutt_bit_isset(idata->capabilities, ACL))
  {
    mutt_debug(2, "Mailbox is read-only.\n");
    ctx->readonly = true;
  }

#ifdef DEBUG
  /* dump the mailbox flags we've found */
  if (debuglevel > 2)
  {
    if (!idata->flags)
      mutt_debug(3, "No folder flags found\n");
    else
    {
      struct List *t = idata->flags;

      mutt_debug(3, "Mailbox flags: ");

      t = t->next;
      while (t)
      {
        mutt_debug(3, "[%s] ", t->data);
        t = t->next;
      }
      mutt_debug(3, "\n");
    }
  }
#endif

  if (!(mutt_bit_isset(idata->ctx->rights, MUTT_ACL_DELETE) ||
        mutt_bit_isset(idata->ctx->rights, MUTT_ACL_SEEN) ||
        mutt_bit_isset(idata->ctx->rights, MUTT_ACL_WRITE) ||
        mutt_bit_isset(idata->ctx->rights, MUTT_ACL_INSERT)))
    ctx->readonly = true;

  ctx->hdrmax = count;
  ctx->hdrs = safe_calloc(count, sizeof(struct Header *));
  ctx->v2r = safe_calloc(count, sizeof(int));
  ctx->msgcount = 0;

  if (count && (imap_read_headers(idata, 1, count) < 0))
  {
    mutt_error(_("Error opening mailbox"));
    mutt_sleep(1);
    goto fail;
  }

  mutt_debug(2, "imap_open_mailbox: msgcount is %d\n", ctx->msgcount);
  FREE(&mx.mbox);
  return 0;

fail:
  if (idata->state == IMAP_SELECTED)
    idata->state = IMAP_AUTHENTICATED;
fail_noidata:
  FREE(&mx.mbox);
  return -1;
}

static int imap_open_mailbox_append(struct Context *ctx, int flags)
{
  struct ImapData *idata = NULL;
  char buf[LONG_STRING];
  char mailbox[LONG_STRING];
  struct ImapMbox mx;
  int rc;

  if (imap_parse_path(ctx->path, &mx))
    return -1;

  /* in APPEND mode, we appear to hijack an existing IMAP connection -
   * ctx is brand new and mostly empty */

  if (!(idata = imap_conn_find(&(mx.account), 0)))
  {
    FREE(&mx.mbox);
    return -1;
  }

  ctx->data = idata;

  imap_fix_path(idata, mx.mbox, mailbox, sizeof(mailbox));
  if (!*mailbox)
    strfcpy(mailbox, "INBOX", sizeof(mailbox));
  FREE(&mx.mbox);

  /* really we should also check for W_OK */
  if ((rc = imap_access(ctx->path, F_OK)) == 0)
    return 0;

  if (rc == -1)
    return -1;

  snprintf(buf, sizeof(buf), _("Create %s?"), mailbox);
  if (option(OPTCONFIRMCREATE) && mutt_yesorno(buf, 1) != MUTT_YES)
    return -1;

  if (imap_create_mailbox(idata, mailbox) < 0)
    return -1;

  return 0;
}

/* imap_logout: Gracefully log out of server. */
void imap_logout(struct ImapData **idata)
{
  /* we set status here to let imap_handle_untagged know we _expect_ to
   * receive a bye response (so it doesn't freak out and close the conn) */
  (*idata)->status = IMAP_BYE;
  imap_cmd_start(*idata, "LOGOUT");
  while (imap_cmd_step(*idata) == IMAP_CMD_CONTINUE)
    ;

  mutt_socket_close((*idata)->conn);
  imap_free_idata(idata);
}

static int imap_open_new_message(struct Message *msg, struct Context *dest, struct Header *hdr)
{
  char tmp[_POSIX_PATH_MAX];

  mutt_mktemp(tmp, sizeof(tmp));
  if ((msg->fp = safe_fopen(tmp, "w")) == NULL)
  {
    mutt_perror(tmp);
    return -1;
  }
  msg->path = safe_strdup(tmp);
  return 0;
}

/* imap_set_flag: append str to flags if we currently have permission
 *   according to aclbit */
static void imap_set_flag(struct ImapData *idata, int aclbit, int flag,
                          const char *str, char *flags, size_t flsize)
{
  if (mutt_bit_isset(idata->ctx->rights, aclbit))
    if (flag && imap_has_flag(idata->flags, str))
      safe_strcat(flags, flsize, str);
}

/* imap_has_flag: do a caseless comparison of the flag against a flag list,
*   return true if found or flag list has '\*', false otherwise */
bool imap_has_flag(struct List *flag_list, const char *flag)
{
  if (!flag_list)
    return false;

  flag_list = flag_list->next;
  while (flag_list)
  {
    if (ascii_strncasecmp(flag_list->data, flag, strlen(flag_list->data)) == 0)
      return true;

    if (ascii_strncmp(flag_list->data, "\\*", strlen(flag_list->data)) == 0)
      return true;

    flag_list = flag_list->next;
  }

  return false;
}

/* Note: headers must be in SORT_ORDER. See imap_exec_msgset for args.
 * Pos is an opaque pointer a la strtok. It should be 0 at first call. */
static int imap_make_msg_set(struct ImapData *idata, struct Buffer *buf,
                             int flag, bool changed, bool invert, int *pos)
{
  struct Header **hdrs = idata->ctx->hdrs;
  int count = 0;      /* number of messages in message set */
  bool match = false; /* whether current message matches flag condition */
  unsigned int setstart = 0; /* start of current message range */
  int n;
  bool started = false;

  hdrs = idata->ctx->hdrs;

  for (n = *pos; n < idata->ctx->msgcount && buf->dptr - buf->data < IMAP_MAX_CMDLEN; n++)
  {
    match = false;
    /* don't include pending expunged messages */
    if (hdrs[n]->active)
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

/* Prepares commands for all messages matching conditions (must be flushed
 * with imap_exec)
 * Params:
 *   idata: ImapData containing context containing header set
 *   pre, post: commands are of the form "%s %s %s %s", tag,
 *     pre, message set, post
 *   flag: enum of flag type on which to filter
 *   changed: include only changed messages in message set
 *   invert: invert sense of flag, eg MUTT_READ matches unread messages
 * Returns: number of matched messages, or -1 on failure */
int imap_exec_msgset(struct ImapData *idata, const char *pre, const char *post,
                     int flag, int changed, int invert)
{
  struct Header **hdrs = NULL;
  short oldsort;
  struct Buffer *cmd = NULL;
  int pos;
  int rc;
  int count = 0;

  if (!(cmd = mutt_buffer_new()))
  {
    mutt_debug(1, "imap_exec_msgset: unable to allocate buffer\n");
    return -1;
  }

  /* We make a copy of the headers just in case resorting doesn't give
   exactly the original order (duplicate messages?), because other parts of
   the ctx are tied to the header order. This may be overkill. */
  oldsort = Sort;
  if (Sort != SORT_ORDER)
  {
    hdrs = idata->ctx->hdrs;
    idata->ctx->hdrs = safe_malloc(idata->ctx->msgcount * sizeof(struct Header *));
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
    rc = imap_make_msg_set(idata, cmd, flag, changed, invert, &pos);
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

/* returns 0 if mutt's flags match cached server flags */
static bool compare_flags(struct Header *h)
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
  if (h->deleted != hd->deleted)
    return true;

  return false;
}

/* Update the IMAP server to reflect the flags a single message.  */
int imap_sync_message(struct ImapData *idata, struct Header *hdr,
                      struct Buffer *cmd, int *err_continue)
{
  char flags[LONG_STRING];
  char uid[11];

  hdr->changed = false;

  if (!compare_flags(hdr))
  {
    idata->ctx->changed = false;
    return 0;
  }

  snprintf(uid, sizeof(uid), "%u", HEADER_DATA(hdr)->uid);
  cmd->dptr = cmd->data;
  mutt_buffer_addstr(cmd, "UID STORE ");
  mutt_buffer_addstr(cmd, uid);

  flags[0] = '\0';

  imap_set_flag(idata, MUTT_ACL_SEEN, hdr->read, "\\Seen ", flags, sizeof(flags));
  imap_set_flag(idata, MUTT_ACL_WRITE, hdr->old, "Old ", flags, sizeof(flags));
  imap_set_flag(idata, MUTT_ACL_WRITE, hdr->flagged, "\\Flagged ", flags, sizeof(flags));
  imap_set_flag(idata, MUTT_ACL_WRITE, hdr->replied, "\\Answered ", flags, sizeof(flags));
  imap_set_flag(idata, MUTT_ACL_DELETE, hdr->deleted, "\\Deleted ", flags, sizeof(flags));

  /* now make sure we don't lose custom tags */
  if (mutt_bit_isset(idata->ctx->rights, MUTT_ACL_WRITE))
    imap_add_keywords(flags, hdr, idata->flags, sizeof(flags));

  mutt_remove_trailing_ws(flags);

  /* UW-IMAP is OK with null flags, Cyrus isn't. The only solution is to
   * explicitly revoke all system flags (if we have permission) */
  if (!*flags)
  {
    imap_set_flag(idata, MUTT_ACL_SEEN, 1, "\\Seen ", flags, sizeof(flags));
    imap_set_flag(idata, MUTT_ACL_WRITE, 1, "Old ", flags, sizeof(flags));
    imap_set_flag(idata, MUTT_ACL_WRITE, 1, "\\Flagged ", flags, sizeof(flags));
    imap_set_flag(idata, MUTT_ACL_WRITE, 1, "\\Answered ", flags, sizeof(flags));
    imap_set_flag(idata, MUTT_ACL_DELETE, 1, "\\Deleted ", flags, sizeof(flags));

    mutt_remove_trailing_ws(flags);

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
      return -1;
  }

  hdr->active = true;
  idata->ctx->changed = false;

  return 0;
}

static int sync_helper(struct ImapData *idata, int right, int flag, const char *name)
{
  int count = 0;
  int rc;
  char buf[LONG_STRING];

  if (!idata->ctx)
    return -1;

  if (!mutt_bit_isset(idata->ctx->rights, right))
    return 0;

  if (right == MUTT_ACL_WRITE && !imap_has_flag(idata->flags, name))
    return 0;

  snprintf(buf, sizeof(buf), "+FLAGS.SILENT (%s)", name);
  if ((rc = imap_exec_msgset(idata, "UID STORE", buf, flag, 1, 0)) < 0)
    return rc;
  count += rc;

  buf[0] = '-';
  if ((rc = imap_exec_msgset(idata, "UID STORE", buf, flag, 1, 1)) < 0)
    return rc;
  count += rc;

  return count;
}

/* update the IMAP server to reflect message changes done within mutt.
 * Arguments
 *   ctx: the current context
 *   expunge: 0 or 1 - do expunge?
 */
int imap_sync_mailbox(struct Context *ctx, int expunge)
{
  struct ImapData *idata = NULL;
  struct Context *appendctx = NULL;
  struct Header *h = NULL;
  struct Header **hdrs = NULL;
  int oldsort;
  int n;
  int rc;

  idata = ctx->data;

  if (idata->state < IMAP_SELECTED)
  {
    mutt_debug(2, "imap_sync_mailbox: no mailbox selected\n");
    return -1;
  }

  /* This function is only called when the calling code expects the context
   * to be changed. */
  imap_allow_reopen(ctx);

  if ((rc = imap_check(idata, 0)) != 0)
    return rc;

  /* if we are expunging anyway, we can do deleted messages very quickly... */
  if (expunge && mutt_bit_isset(ctx->rights, MUTT_ACL_DELETE))
  {
    if ((rc = imap_exec_msgset(idata, "UID STORE", "+FLAGS.SILENT (\\Deleted)",
                               MUTT_DELETED, 1, 0)) < 0)
    {
      mutt_error(_("Expunge failed"));
      mutt_sleep(1);
      goto out;
    }

    if (rc > 0)
    {
      /* mark these messages as unchanged so second pass ignores them. Done
       * here so BOGUS UW-IMAP 4.7 SILENT FLAGS updates are ignored. */
      for (n = 0; n < ctx->msgcount; n++)
        if (ctx->hdrs[n]->deleted && ctx->hdrs[n]->changed)
          ctx->hdrs[n]->active = false;
      mutt_message(_("Marking %d messages deleted..."), rc);
    }
  }

#ifdef USE_HCACHE
  idata->hcache = imap_hcache_open(idata, NULL);
#endif

  /* save messages with real (non-flag) changes */
  for (n = 0; n < ctx->msgcount; n++)
  {
    h = ctx->hdrs[n];

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
        mutt_message(_("Saving changed messages... [%d/%d]"), n + 1, ctx->msgcount);
        if (!appendctx)
          appendctx = mx_open_mailbox(ctx->path, MUTT_APPEND | MUTT_QUIET, NULL);
        if (!appendctx)
          mutt_debug(
              1, "imap_sync_mailbox: Error opening mailbox in append mode\n");
        else
          _mutt_save_message(h, appendctx, 1, 0, 0);
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
    ctx->hdrs = safe_malloc(ctx->msgcount * sizeof(struct Header *));
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
  for (n = 0; n < ctx->msgcount; n++)
  {
    HEADER_DATA(ctx->hdrs[n])->deleted = ctx->hdrs[n]->deleted;
    HEADER_DATA(ctx->hdrs[n])->flagged = ctx->hdrs[n]->flagged;
    HEADER_DATA(ctx->hdrs[n])->old = ctx->hdrs[n]->old;
    HEADER_DATA(ctx->hdrs[n])->read = ctx->hdrs[n]->read;
    HEADER_DATA(ctx->hdrs[n])->replied = ctx->hdrs[n]->replied;
    ctx->hdrs[n]->changed = false;
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

  if (option(OPTMESSAGECACHECLEAN))
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

/* imap_close_mailbox: clean up IMAP data in Context */
int imap_close_mailbox(struct Context *ctx)
{
  struct ImapData *idata = NULL;
  int i;

  idata = ctx->data;
  /* Check to see if the mailbox is actually open */
  if (!idata)
    return 0;

  /* imap_open_mailbox_append() borrows the struct ImapData temporarily,
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
      /* mx_close_mailbox won't sync if there are no deleted messages
       * and the mailbox is unchanged, so we may have to close here */
      if (!ctx->deleted)
        imap_exec(idata, "CLOSE", IMAP_CMD_QUEUE);
      idata->state = IMAP_AUTHENTICATED;
    }

    idata->reopen &= IMAP_REOPEN_ALLOW;
    FREE(&(idata->mailbox));
    mutt_free_list(&idata->flags);
    idata->ctx = NULL;

    hash_destroy(&idata->uid_hash, NULL);
    FREE(&idata->msn_index);
    idata->msn_index_size = 0;
    idata->max_msn = 0;

    for (i = 0; i < IMAP_CACHE_LEN; i++)
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
  for (i = 0; i < ctx->msgcount; i++)
    /* mailbox may not have fully loaded */
    if (ctx->hdrs[i] && ctx->hdrs[i]->data)
      imap_free_header_data((struct ImapHeaderData **) &(ctx->hdrs[i]->data));

  return 0;
}

/* use the NOOP or IDLE command to poll for new mail
 *
 * return values:
 *      MUTT_REOPENED   mailbox has been externally modified
 *      MUTT_NEW_MAIL   new mail has arrived!
 *      0               no change
 *      -1              error
 */
int imap_check_mailbox(struct Context *ctx, int force)
{
  return imap_check(ctx->data, force);
}

int imap_check(struct ImapData *idata, int force)
{
  /* overload keyboard timeout to avoid many mailbox checks in a row.
   * Most users don't like having to wait exactly when they press a key. */
  int result = 0;

  /* try IDLE first, unless force is set */
  if (!force && option(OPTIMAPIDLE) && mutt_bit_isset(idata->capabilities, IDLE) &&
      (idata->state != IMAP_IDLE || time(NULL) >= idata->lastread + ImapKeepalive))
  {
    if (imap_cmd_idle(idata) < 0)
      return -1;
  }
  if (idata->state == IMAP_IDLE)
  {
    while ((result = mutt_socket_poll(idata->conn)) > 0)
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
      imap_exec(idata, "NOOP", 0) != 0)
    return -1;

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

static int imap_check_mailbox_reopen(struct Context *ctx, int *index_hint)
{
  int rc;
  (void) index_hint;

  imap_allow_reopen(ctx);
  rc = imap_check(ctx->data, 0);
  imap_disallow_reopen(ctx);

  return rc;
}

/* split path into (idata,mailbox name) */
static int imap_get_mailbox(const char *path, struct ImapData **hidata, char *buf, size_t blen)
{
  struct ImapMbox mx;

  if (imap_parse_path(path, &mx))
  {
    mutt_debug(1, "imap_get_mailbox: Error parsing %s\n", path);
    return -1;
  }
  if (!(*hidata = imap_conn_find(&(mx.account), option(OPTIMAPPASSIVE) ? MUTT_IMAP_CONN_NONEW : 0)) ||
      (*hidata)->state < IMAP_AUTHENTICATED)
  {
    FREE(&mx.mbox);
    return -1;
  }

  imap_fix_path(*hidata, mx.mbox, buf, blen);
  if (!*buf)
    strfcpy(buf, "INBOX", blen);
  FREE(&mx.mbox);

  return 0;
}

/* check for new mail in any subscribed mailboxes. Given a list of mailboxes
 * rather than called once for each so that it can batch the commands and
 * save on round trips. Returns number of mailboxes with new mail. */
int imap_buffy_check(int force, int check_stats)
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

    if (imap_get_mailbox(mailbox->path, &idata, name, sizeof(name)) < 0)
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
      if (imap_exec(lastdata, NULL, IMAP_CMD_FAIL_OK) == -1)
        mutt_debug(1, "Error polling mailboxes\n");

      lastdata = NULL;
    }

    if (!lastdata)
      lastdata = idata;

    imap_munge_mbox_name(idata, munged, sizeof(munged), name);
    if (check_stats)
      snprintf(command, sizeof(command),
               "STATUS %s (UIDNEXT UIDVALIDITY UNSEEN RECENT MESSAGES)", munged);
    else
      snprintf(command, sizeof(command),
               "STATUS %s (UIDNEXT UIDVALIDITY UNSEEN RECENT)", munged);

    if (imap_exec(idata, command, IMAP_CMD_QUEUE) < 0)
    {
      mutt_debug(1, "Error queueing command\n");
      return 0;
    }
  }

  if (lastdata && (imap_exec(lastdata, NULL, IMAP_CMD_FAIL_OK) == -1))
  {
    mutt_debug(1, "Error polling mailboxes\n");
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

/* imap_status: returns count of messages in mailbox, or -1 on error.
 * if queue != 0, queue the command and expect it to have been run
 * on the next call (for pipelining the postponed count) */
int imap_status(char *path, int queue)
{
  static int queued = 0;

  struct ImapData *idata = NULL;
  char buf[LONG_STRING];
  char mbox[LONG_STRING];
  struct ImapStatus *status = NULL;

  if (imap_get_mailbox(path, &idata, buf, sizeof(buf)) < 0)
    return -1;

  if (imap_mxcmp(buf, idata->mailbox) == 0)
    /* We are in the folder we're polling - just return the mailbox count */
    return idata->ctx->msgcount;
  else if (mutt_bit_isset(idata->capabilities, IMAP4REV1) ||
           mutt_bit_isset(idata->capabilities, STATUS))
  {
    imap_munge_mbox_name(idata, mbox, sizeof(mbox), buf);
    snprintf(buf, sizeof(buf), "STATUS %s (%s)", mbox, "MESSAGES");
    imap_unmunge_mbox_name(idata, mbox);
  }
  else
    /* Server does not support STATUS, and this is not the current mailbox.
     * There is no lightweight way to check recent arrivals */
    return -1;

  if (queue)
  {
    imap_exec(idata, buf, IMAP_CMD_QUEUE);
    queued = 1;
    return 0;
  }
  else if (!queued)
    imap_exec(idata, buf, 0);

  queued = 0;
  if ((status = imap_mboxcache_get(idata, mbox, 0)))
    return status->messages;

  return 0;
}

/* return cached mailbox stats or NULL if create is 0 */
struct ImapStatus *imap_mboxcache_get(struct ImapData *idata, const char *mbox, int create)
{
  struct List *cur = NULL;
  struct ImapStatus *status = NULL;
  struct ImapStatus scache;
#ifdef USE_HCACHE
  header_cache_t *hc = NULL;
  void *uidvalidity = NULL;
  void *uidnext = NULL;
#endif

  for (cur = idata->mboxcache; cur; cur = cur->next)
  {
    status = (struct ImapStatus *) cur->data;

    if (imap_mxcmp(mbox, status->name) == 0)
      return status;
  }
  status = NULL;

  /* lame */
  if (create)
  {
    memset(&scache, 0, sizeof(scache));
    scache.name = (char *) mbox;
    idata->mboxcache = mutt_add_list_n(idata->mboxcache, &scache, sizeof(scache));
    status = imap_mboxcache_get(idata, mbox, 0);
    status->name = safe_strdup(mbox);
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
      mutt_debug(3, "mboxcache: hcache uidvalidity %d, uidnext %d\n",
                 status->uidvalidity, status->uidnext);
    }
    mutt_hcache_free(hc, &uidvalidity);
    mutt_hcache_free(hc, &uidnext);
    mutt_hcache_close(hc);
  }
#endif

  return status;
}

void imap_mboxcache_free(struct ImapData *idata)
{
  struct List *cur = NULL;
  struct ImapStatus *status = NULL;

  for (cur = idata->mboxcache; cur; cur = cur->next)
  {
    status = (struct ImapStatus *) cur->data;

    FREE(&status->name);
  }

  mutt_free_list(&idata->mboxcache);
}

/* returns number of patterns in the search that should be done server-side
 * (eg are full-text) */
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

/* convert mutt Pattern to IMAP SEARCH command containing only elements
 * that require full-text search (mutt already has what it needs for most
 * match types, and does a better job (eg server doesn't support regexps). */
static int imap_compile_search(struct Context *ctx, const struct Pattern *pat,
                               struct Buffer *buf)
{
  if (!do_search(pat, 0))
    return 0;

  if (pat->not)
    mutt_buffer_addstr(buf, "NOT ");

  if (pat->child)
  {
    int clauses;

    if ((clauses = do_search(pat->child, 1)) > 0)
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

          if (imap_compile_search(ctx, clause, buf) < 0)
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
        if (!(delim = strchr(pat->p.str, ':')))
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

int imap_search(struct Context *ctx, const struct Pattern *pat)
{
  struct Buffer buf;
  struct ImapData *idata = ctx->data;
  for (int i = 0; i < ctx->msgcount; i++)
    ctx->hdrs[i]->matched = false;

  if (!do_search(pat, 1))
    return 0;

  mutt_buffer_init(&buf);
  mutt_buffer_addstr(&buf, "UID SEARCH ");
  if (imap_compile_search(ctx, pat, &buf) < 0)
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

int imap_subscribe(char *path, int subscribe)
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
  if (!(idata = imap_conn_find(&(mx.account), 0)))
    goto fail;

  imap_fix_path(idata, mx.mbox, buf, sizeof(buf));
  if (!*buf)
    strfcpy(buf, "INBOX", sizeof(buf));

  if (option(OPTIMAPCHECKSUBSCRIBED))
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

/* trim dest to the length of the longest prefix it shares with src,
 * returning the length of the trimmed string */
static int longest_common_prefix(char *dest, const char *src, int start, size_t dlen)
{
  int pos = start;

  while (pos < dlen && dest[pos] && dest[pos] == src[pos])
    pos++;
  dest[pos] = '\0';

  return pos;
}

/* look for IMAP URLs to complete from defined mailboxes. Could be extended
 * to complete over open connections and account/folder hooks too. */
static int imap_complete_hosts(char *dest, size_t len)
{
  struct Buffy *mailbox = NULL;
  struct Connection *conn = NULL;
  int rc = -1;
  int matchlen;

  matchlen = mutt_strlen(dest);
  for (mailbox = Incoming; mailbox; mailbox = mailbox->next)
  {
    if (mutt_strncmp(dest, mailbox->path, matchlen) == 0)
    {
      if (rc)
      {
        strfcpy(dest, mailbox->path, len);
        rc = 0;
      }
      else
        longest_common_prefix(dest, mailbox->path, matchlen, len);
    }
  }

  for (conn = mutt_socket_head(); conn; conn = conn->next)
  {
    struct CissUrl url;
    char urlstr[LONG_STRING];

    if (conn->account.type != MUTT_ACCT_TYPE_IMAP)
      continue;

    mutt_account_tourl(&conn->account, &url);
    /* FIXME: how to handle multiple users on the same host? */
    url.user = NULL;
    url.path = NULL;
    url_ciss_tostring(&url, urlstr, sizeof(urlstr), 0);
    if (mutt_strncmp(dest, urlstr, matchlen) == 0)
    {
      if (rc)
      {
        strfcpy(dest, urlstr, len);
        rc = 0;
      }
      else
        longest_common_prefix(dest, urlstr, matchlen, len);
    }
  }

  return rc;
}

/* imap_complete: given a partial IMAP folder path, return a string which
 *   adds as much to the path as is unique */
int imap_complete(char *dest, size_t dlen, char *path)
{
  struct ImapData *idata = NULL;
  char list[LONG_STRING];
  char buf[LONG_STRING];
  struct ImapList listresp;
  char completion[LONG_STRING];
  int clen, matchlen = 0;
  int completions = 0;
  struct ImapMbox mx;
  int rc;

  if (imap_parse_path(path, &mx))
  {
    strfcpy(dest, path, dlen);
    return imap_complete_hosts(dest, dlen);
  }

  /* don't open a new socket just for completion. Instead complete over
   * known mailboxes/hooks/etc */
  if (!(idata = imap_conn_find(&(mx.account), MUTT_IMAP_CONN_NONEW)))
  {
    FREE(&mx.mbox);
    strfcpy(dest, path, dlen);
    return imap_complete_hosts(dest, dlen);
  }

  /* reformat path for IMAP list, and append wildcard */
  /* don't use INBOX in place of "" */
  if (mx.mbox && mx.mbox[0])
    imap_fix_path(idata, mx.mbox, list, sizeof(list));
  else
    list[0] = '\0';

  /* fire off command */
  snprintf(buf, sizeof(buf), "%s \"\" \"%s%%\"", option(OPTIMAPLSUB) ? "LSUB" : "LIST", list);

  imap_cmd_start(idata, buf);

  /* and see what the results are */
  strfcpy(completion, NONULL(mx.mbox), sizeof(completion));
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
        strfcpy(completion, listresp.name, sizeof(completion));
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
    imap_qualify_path(dest, dlen, &mx, completion);
    mutt_pretty_mailbox(dest, dlen);

    FREE(&mx.mbox);
    return 0;
  }

  return -1;
}

/* imap_fast_trash: use server COPY command to copy deleted
 * messages to the trash folder.
 *   Return codes:
 *      -1: error
 *       0: success
 *       1: non-fatal error - try fetch/append */
int imap_fast_trash(struct Context *ctx, char *dest)
{
  struct ImapData *idata = NULL;
  char mbox[LONG_STRING];
  char mmbox[LONG_STRING];
  char prompt[LONG_STRING];
  int rc;
  struct ImapMbox mx;
  bool triedcreate = false;

  idata = ctx->data;

  if (imap_parse_path(dest, &mx))
  {
    mutt_debug(1, "imap_fast_trash: bad destination %s\n", dest);
    return -1;
  }

  /* check that the save-to folder is in the same account */
  if (!mutt_account_match(&(idata->conn->account), &(mx.account)))
  {
    mutt_debug(3, "imap_fast_trash: %s not same server as %s\n", dest, ctx->path);
    return 1;
  }

  imap_fix_path(idata, mx.mbox, mbox, sizeof(mbox));
  if (!*mbox)
    strfcpy(mbox, "INBOX", sizeof(mbox));
  imap_munge_mbox_name(idata, mmbox, sizeof(mmbox), mbox);

  /* loop in case of TRYCREATE */
  do
  {
    rc = imap_exec_msgset(idata, "UID STORE", "+FLAGS.SILENT (\\Seen)", MUTT_TRASH, 0, 0);
    if (rc < 0)
    {
      mutt_debug(1, "imap_fast_trash: Unable to mark messages as seen\n");
      goto out;
    }

    rc = imap_exec_msgset(idata, "UID COPY", mmbox, MUTT_TRASH, 0, 0);
    if (!rc)
    {
      mutt_debug(1, "imap_fast_trash: No messages to trash\n");
      rc = -1;
      goto out;
    }
    else if (rc < 0)
    {
      mutt_debug(1, "could not queue copy\n");
      goto out;
    }
    else
      mutt_message(_("Copying %d messages to %s..."), rc, mbox);

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
      if (ascii_strncasecmp(imap_get_qualifier(idata->buf), "[TRYCREATE]", 11) != 0)
        break;
      mutt_debug(3, "imap_fast_trash: server suggests TRYCREATE\n");
      snprintf(prompt, sizeof(prompt), _("Create %s?"), mbox);
      if (option(OPTCONFIRMCREATE) && mutt_yesorno(prompt, 1) != MUTT_YES)
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
  FREE(&mx.mbox);

  return rc < 0 ? -1 : rc;
}

struct MxOps mx_imap_ops = {
  .open = imap_open_mailbox,
  .open_append = imap_open_mailbox_append,
  .close = imap_close_mailbox,
  .open_msg = imap_fetch_message,
  .close_msg = imap_close_message,
  .commit_msg = imap_commit_message,
  .open_new_msg = imap_open_new_message,
  .check = imap_check_mailbox_reopen,
  .sync = NULL, /* imap syncing is handled by imap_sync_mailbox */
};
