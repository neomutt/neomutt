/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1996-9 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2009 Brendan Cully <brendan@kublai.com>
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/* Support for IMAP4rev1, with the occasional nod to IMAP 4. */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mx.h"
#include "mailbox.h"
#include "globals.h"
#include "sort.h"
#include "browser.h"
#include "message.h"
#include "imap_private.h"
#if defined(USE_SSL)
# include "mutt_ssl.h"
#endif
#include "buffy.h"
#if USE_HCACHE
#include "hcache.h"
#endif

#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

/* imap forward declarations */
static char* imap_get_flags (LIST** hflags, char* s);
static int imap_check_capabilities (IMAP_DATA* idata);
static void imap_set_flag (IMAP_DATA* idata, int aclbit, int flag,
			   const char* str, char* flags, size_t flsize);

/* imap_access: Check permissions on an IMAP mailbox.
 * TODO: ACL checks. Right now we assume if it exists we can
 *       mess with it. */
int imap_access (const char* path, int flags)
{
  IMAP_DATA* idata;
  IMAP_MBOX mx;
  char buf[LONG_STRING];
  char mailbox[LONG_STRING];
  char mbox[LONG_STRING];
  int rc;

  if (imap_parse_path (path, &mx))
    return -1;

  if (!(idata = imap_conn_find (&mx.account,
    option (OPTIMAPPASSIVE) ? M_IMAP_CONN_NONEW : 0)))
  {
    FREE (&mx.mbox);
    return -1;
  }

  imap_fix_path (idata, mx.mbox, mailbox, sizeof (mailbox));
  if (!*mailbox)
    strfcpy (mailbox, "INBOX", sizeof (mailbox));

  /* we may already be in the folder we're checking */
  if (!ascii_strcmp(idata->mailbox, mx.mbox))
  {
    FREE (&mx.mbox);
    return 0;
  }
  FREE (&mx.mbox);

  if (imap_mboxcache_get (idata, mailbox, 0))
  {
    dprint (3, (debugfile, "imap_access: found %s in cache\n", mailbox));
    return 0;
  }

  imap_munge_mbox_name (mbox, sizeof (mbox), mailbox);

  if (mutt_bit_isset (idata->capabilities, IMAP4REV1))
    snprintf (buf, sizeof (buf), "STATUS %s (UIDVALIDITY)", mbox);
  else if (mutt_bit_isset (idata->capabilities, STATUS))
    snprintf (buf, sizeof (buf), "STATUS %s (UID-VALIDITY)", mbox);
  else
  {
    dprint (2, (debugfile, "imap_access: STATUS not supported?\n"));
    return -1;
  }

  if ((rc = imap_exec (idata, buf, IMAP_CMD_FAIL_OK)) < 0)
  {
    dprint (1, (debugfile, "imap_access: Can't check STATUS of %s\n", mbox));
    return rc;
  }

  return 0;
}

int imap_create_mailbox (IMAP_DATA* idata, char* mailbox)
{
  char buf[LONG_STRING], mbox[LONG_STRING];

  imap_munge_mbox_name (mbox, sizeof (mbox), mailbox);
  snprintf (buf, sizeof (buf), "CREATE %s", mbox);

  if (imap_exec (idata, buf, 0) != 0)
  {
    mutt_error (_("CREATE failed: %s"), imap_cmd_trailer (idata));
    return -1;
  }

  return 0;
}

int imap_rename_mailbox (IMAP_DATA* idata, IMAP_MBOX* mx, const char* newname)
{
  char oldmbox[LONG_STRING];
  char newmbox[LONG_STRING];
  char buf[LONG_STRING];

  imap_munge_mbox_name (oldmbox, sizeof (oldmbox), mx->mbox);
  imap_munge_mbox_name (newmbox, sizeof (newmbox), newname);

  snprintf (buf, sizeof (buf), "RENAME %s %s", oldmbox, newmbox);

  if (imap_exec (idata, buf, 0) != 0)
    return -1;

  return 0;
}

int imap_delete_mailbox (CONTEXT* ctx, IMAP_MBOX mx)
{
  char buf[LONG_STRING], mbox[LONG_STRING];
  IMAP_DATA *idata;

  if (!ctx || !ctx->data) {
    if (!(idata = imap_conn_find (&mx.account,
          option (OPTIMAPPASSIVE) ? M_IMAP_CONN_NONEW : 0)))
    {
      FREE (&mx.mbox);
      return -1;
    }
  } else {
    idata = ctx->data;
  }

  imap_munge_mbox_name (mbox, sizeof (mbox), mx.mbox);
  snprintf (buf, sizeof (buf), "DELETE %s", mbox);

  if (imap_exec ((IMAP_DATA*) idata, buf, 0) != 0)
    return -1;

  return 0;
}

/* imap_logout_all: close all open connections. Quick and dirty until we can
 *   make sure we've got all the context we need. */
void imap_logout_all (void)
{
  CONNECTION* conn;
  CONNECTION* tmp;

  conn = mutt_socket_head ();

  while (conn)
  {
    tmp = conn->next;

    if (conn->account.type == M_ACCT_TYPE_IMAP && conn->fd >= 0)
    {
      mutt_message (_("Closing connection to %s..."), conn->account.host);
      imap_logout ((IMAP_DATA**) (void*) &conn->data);
      mutt_clear_error ();
      mutt_socket_free (conn);
    }

    conn = tmp;
  }
}

/* imap_read_literal: read bytes bytes from server into file. Not explicitly
 *   buffered, relies on FILE buffering. NOTE: strips \r from \r\n.
 *   Apparently even literals use \r\n-terminated strings ?! */
int imap_read_literal (FILE* fp, IMAP_DATA* idata, long bytes, progress_t* pbar)
{
  long pos;
  char c;

  int r = 0;

  dprint (2, (debugfile, "imap_read_literal: reading %ld bytes\n", bytes));

  for (pos = 0; pos < bytes; pos++)
  {
    if (mutt_socket_readchar (idata->conn, &c) != 1)
    {
      dprint (1, (debugfile, "imap_read_literal: error during read, %ld bytes read\n", pos));
      idata->status = IMAP_FATAL;

      return -1;
    }

#if 1
    if (r == 1 && c != '\n')
      fputc ('\r', fp);

    if (c == '\r')
    {
      r = 1;
      continue;
    }
    else
      r = 0;
#endif
    fputc (c, fp);

    if (pbar && !(pos % 1024))
      mutt_progress_update (pbar, pos, -1);
#ifdef DEBUG
    if (debuglevel >= IMAP_LOG_LTRL)
      fputc (c, debugfile);
#endif
  }

  return 0;
}

/* imap_expunge_mailbox: Purge IMAP portion of expunged messages from the
 *   context. Must not be done while something has a handle on any headers
 *   (eg inside pager or editor). That is, check IMAP_REOPEN_ALLOW. */
void imap_expunge_mailbox (IMAP_DATA* idata)
{
  HEADER* h;
  int i, cacheno;

#ifdef USE_HCACHE
  idata->hcache = imap_hcache_open (idata, NULL);
#endif

  for (i = 0; i < idata->ctx->msgcount; i++)
  {
    h = idata->ctx->hdrs[i];

    if (h->index == -1)
    {
      dprint (2, (debugfile, "Expunging message UID %d.\n", HEADER_DATA (h)->uid));

      h->active = 0;
      idata->ctx->size -= h->content->length;

      imap_cache_del (idata, h);
#if USE_HCACHE
      imap_hcache_del (idata, HEADER_DATA(h)->uid);
#endif

      /* free cached body from disk, if necessary */
      cacheno = HEADER_DATA(h)->uid % IMAP_CACHE_LEN;
      if (idata->cache[cacheno].uid == HEADER_DATA(h)->uid &&
	  idata->cache[cacheno].path)
      {
	unlink (idata->cache[cacheno].path);
	FREE (&idata->cache[cacheno].path);
      }

      imap_free_header_data (&h->data);
    }
  }

#if USE_HCACHE
  imap_hcache_close (idata);
#endif

  /* We may be called on to expunge at any time. We can't rely on the caller
   * to always know to rethread */
  mx_update_tables (idata->ctx, 0);
  mutt_sort_headers (idata->ctx, 1);
}

/* imap_check_capabilities: make sure we can log in to this server. */
static int imap_check_capabilities (IMAP_DATA* idata)
{
  if (imap_exec (idata, "CAPABILITY", 0) != 0)
  {
    imap_error ("imap_check_capabilities", idata->buf);
    return -1;
  }

  if (!(mutt_bit_isset(idata->capabilities,IMAP4)
      ||mutt_bit_isset(idata->capabilities,IMAP4REV1)))
  {
    mutt_error _("This IMAP server is ancient. Mutt does not work with it.");
    mutt_sleep (2);	/* pause a moment to let the user see the error */

    return -1;
  }

  return 0;
}

/* imap_conn_find: Find an open IMAP connection matching account, or open
 *   a new one if none can be found. */
IMAP_DATA* imap_conn_find (const ACCOUNT* account, int flags)
{
  CONNECTION* conn = NULL;
  ACCOUNT* creds = NULL;
  IMAP_DATA* idata = NULL;
  int new = 0;

  while ((conn = mutt_conn_find (conn, account)))
  {
    if (!creds)
      creds = &conn->account;
    else
      memcpy (&conn->account, creds, sizeof (ACCOUNT));

    idata = (IMAP_DATA*)conn->data;
    if (flags & M_IMAP_CONN_NONEW)
    {
      if (!idata)
      {
        /* This should only happen if we've come to the end of the list */
        mutt_socket_free (conn);
        return NULL;
      }
      else if (idata->state < IMAP_AUTHENTICATED)
        continue;
    }
    if (flags & M_IMAP_CONN_NOSELECT && idata && idata->state >= IMAP_SELECTED)
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
    if (! (idata = imap_new_idata ()))
    {
      mutt_socket_free (conn);
      return NULL;
    }

    conn->data = idata;
    idata->conn = conn;
    new = 1;
  }

  if (idata->state == IMAP_DISCONNECTED)
    imap_open_connection (idata);
  if (idata->state == IMAP_CONNECTED)
  {
    if (!imap_authenticate (idata))
    {
      idata->state = IMAP_AUTHENTICATED;
      new = 1;
      if (idata->conn->ssf)
	dprint (2, (debugfile, "Communication encrypted at %d bits\n",
		    idata->conn->ssf));
    }
    else
      mutt_account_unsetpass (&idata->conn->account);

    FREE (&idata->capstr);
  }
  if (new && idata->state == IMAP_AUTHENTICATED)
  {
    /* capabilities may have changed */
    imap_exec (idata, "CAPABILITY", IMAP_CMD_QUEUE);
    /* get root delimiter, '/' as default */
    idata->delim = '/';
    imap_exec (idata, "LIST \"\" \"\"", IMAP_CMD_QUEUE);
    if (option (OPTIMAPCHECKSUBSCRIBED))
      imap_exec (idata, "LSUB \"\" \"*\"", IMAP_CMD_QUEUE);
    /* we may need the root delimiter before we open a mailbox */
    imap_exec (idata, NULL, IMAP_CMD_FAIL_OK);
  }

  return idata;
}

int imap_open_connection (IMAP_DATA* idata)
{
  char buf[LONG_STRING];

  if (mutt_socket_open (idata->conn) < 0)
    return -1;

  idata->state = IMAP_CONNECTED;

  if (imap_cmd_step (idata) != IMAP_CMD_CONTINUE)
  {
    imap_close_connection (idata);
    return -1;
  }

  if (ascii_strncasecmp ("* OK", idata->buf, 4) == 0)
  {
    if (ascii_strncasecmp ("* OK [CAPABILITY", idata->buf, 16)
        && imap_check_capabilities (idata))
      goto bail;
#if defined(USE_SSL)
    /* Attempt STARTTLS if available and desired. */
    if (!idata->conn->ssf && (option(OPTSSLFORCETLS) ||
                              mutt_bit_isset (idata->capabilities, STARTTLS)))
    {
      int rc;

      if (option(OPTSSLFORCETLS))
        rc = M_YES;
      else if ((rc = query_quadoption (OPT_SSLSTARTTLS,
        _("Secure connection with TLS?"))) == -1)
	goto err_close_conn;
      if (rc == M_YES) {
	if ((rc = imap_exec (idata, "STARTTLS", IMAP_CMD_FAIL_OK)) == -1)
	  goto bail;
	if (rc != -2)
	{
	  if (mutt_ssl_starttls (idata->conn))
	  {
	    mutt_error (_("Could not negotiate TLS connection"));
	    mutt_sleep (1);
	    goto err_close_conn;
	  }
	  else
	  {
	    /* RFC 2595 demands we recheck CAPABILITY after TLS completes. */
	    if (imap_exec (idata, "CAPABILITY", 0))
	      goto bail;
	  }
	}
      }
    }

    if (option(OPTSSLFORCETLS) && ! idata->conn->ssf)
    {
      mutt_error _("Encrypted connection unavailable");
      mutt_sleep (1);
      goto err_close_conn;
    }
#endif
  }
  else if (ascii_strncasecmp ("* PREAUTH", idata->buf, 9) == 0)
  {
    idata->state = IMAP_AUTHENTICATED;
    if (imap_check_capabilities (idata) != 0)
      goto bail;
    FREE (&idata->capstr);
  }
  else
  {
    imap_error ("imap_open_connection()", buf);
    goto bail;
  }

  return 0;

#if defined(USE_SSL)
 err_close_conn:
  imap_close_connection (idata);
#endif
 bail:
  FREE (&idata->capstr);
  return -1;
}

void imap_close_connection(IMAP_DATA* idata)
{
  if (idata->state != IMAP_DISCONNECTED)
  {
    mutt_socket_close (idata->conn);
    idata->state = IMAP_DISCONNECTED;
  }
  idata->seqno = idata->nextcmd = idata->lastcmd = idata->status = 0;
  memset (idata->cmds, 0, sizeof (IMAP_COMMAND) * idata->cmdslots);
}

/* imap_get_flags: Make a simple list out of a FLAGS response.
 *   return stream following FLAGS response */
static char* imap_get_flags (LIST** hflags, char* s)
{
  LIST* flags;
  char* flag_word;
  char ctmp;

  /* sanity-check string */
  if (ascii_strncasecmp ("FLAGS", s, 5) != 0)
  {
    dprint (1, (debugfile, "imap_get_flags: not a FLAGS response: %s\n",
      s));
    return NULL;
  }
  s += 5;
  SKIPWS(s);
  if (*s != '(')
  {
    dprint (1, (debugfile, "imap_get_flags: bogus FLAGS response: %s\n",
      s));
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
    while (*s && (*s != ')') && !ISSPACE (*s))
      s++;
    ctmp = *s;
    *s = '\0';
    if (*flag_word)
      mutt_add_list (flags, flag_word);
    *s = ctmp;
  }

  /* note bad flags response */
  if (*s != ')')
  {
    dprint (1, (debugfile,
      "imap_get_flags: Unterminated FLAGS response: %s\n", s));
    mutt_free_list (hflags);

    return NULL;
  }

  s++;

  return s;
}

int imap_open_mailbox (CONTEXT* ctx)
{
  CONNECTION *conn;
  IMAP_DATA *idata;
  IMAP_STATUS* status;
  char buf[LONG_STRING];
  char bufout[LONG_STRING];
  int count = 0;
  IMAP_MBOX mx, pmx;
  int rc;

  if (imap_parse_path (ctx->path, &mx))
  {
    mutt_error (_("%s is an invalid IMAP path"), ctx->path);
    return -1;
  }

  /* we require a connection which isn't currently in IMAP_SELECTED state */
  if (!(idata = imap_conn_find (&(mx.account), M_IMAP_CONN_NOSELECT)))
    goto fail_noidata;
  if (idata->state < IMAP_AUTHENTICATED)
    goto fail;

  conn = idata->conn;

  /* once again the context is new */
  ctx->data = idata;
  ctx->mx_close = imap_close_mailbox;

  /* Clean up path and replace the one in the ctx */
  imap_fix_path (idata, mx.mbox, buf, sizeof (buf));
  if (!*buf)
    strfcpy (buf, "INBOX", sizeof (buf));
  FREE(&(idata->mailbox));
  idata->mailbox = safe_strdup (buf);
  imap_qualify_path (buf, sizeof (buf), &mx, idata->mailbox);

  FREE (&(ctx->path));
  ctx->path = safe_strdup (buf);

  idata->ctx = ctx;

  /* clear mailbox status */
  idata->status = 0;
  memset (idata->ctx->rights, 0, sizeof (idata->ctx->rights));
  idata->newMailCount = 0;

  mutt_message (_("Selecting %s..."), idata->mailbox);
  imap_munge_mbox_name (buf, sizeof(buf), idata->mailbox);

  /* pipeline ACL test */
  if (mutt_bit_isset (idata->capabilities, ACL))
  {
    snprintf (bufout, sizeof (bufout), "MYRIGHTS %s", buf);
    imap_exec (idata, bufout, IMAP_CMD_QUEUE);
  }
  /* assume we have all rights if ACL is unavailable */
  else
  {
    mutt_bit_set (idata->ctx->rights, M_ACL_LOOKUP);
    mutt_bit_set (idata->ctx->rights, M_ACL_READ);
    mutt_bit_set (idata->ctx->rights, M_ACL_SEEN);
    mutt_bit_set (idata->ctx->rights, M_ACL_WRITE);
    mutt_bit_set (idata->ctx->rights, M_ACL_INSERT);
    mutt_bit_set (idata->ctx->rights, M_ACL_POST);
    mutt_bit_set (idata->ctx->rights, M_ACL_CREATE);
    mutt_bit_set (idata->ctx->rights, M_ACL_DELETE);
  }
  /* pipeline the postponed count if possible */
  pmx.mbox = NULL;
  if (mx_is_imap (Postponed) && !imap_parse_path (Postponed, &pmx)
      && mutt_account_match (&pmx.account, &mx.account))
    imap_status (Postponed, 1);
  FREE (&pmx.mbox);

  snprintf (bufout, sizeof (bufout), "%s %s",
    ctx->readonly ? "EXAMINE" : "SELECT", buf);

  idata->state = IMAP_SELECTED;

  imap_cmd_start (idata, bufout);

  status = imap_mboxcache_get (idata, idata->mailbox, 1);

  do
  {
    char *pc;

    if ((rc = imap_cmd_step (idata)) != IMAP_CMD_CONTINUE)
      break;

    pc = idata->buf + 2;

    /* Obtain list of available flags here, may be overridden by a
     * PERMANENTFLAGS tag in the OK response */
    if (ascii_strncasecmp ("FLAGS", pc, 5) == 0)
    {
      /* don't override PERMANENTFLAGS */
      if (!idata->flags)
      {
	dprint (3, (debugfile, "Getting mailbox FLAGS\n"));
	if ((pc = imap_get_flags (&(idata->flags), pc)) == NULL)
	  goto fail;
      }
    }
    /* PERMANENTFLAGS are massaged to look like FLAGS, then override FLAGS */
    else if (ascii_strncasecmp ("OK [PERMANENTFLAGS", pc, 18) == 0)
    {
      dprint (3, (debugfile, "Getting mailbox PERMANENTFLAGS\n"));
      /* safe to call on NULL */
      mutt_free_list (&(idata->flags));
      /* skip "OK [PERMANENT" so syntax is the same as FLAGS */
      pc += 13;
      if ((pc = imap_get_flags (&(idata->flags), pc)) == NULL)
	goto fail;
    }
    /* save UIDVALIDITY for the header cache */
    else if (ascii_strncasecmp ("OK [UIDVALIDITY", pc, 14) == 0)
    {
      dprint (3, (debugfile, "Getting mailbox UIDVALIDITY\n"));
      pc += 3;
      pc = imap_next_word (pc);
      idata->uid_validity = strtol (pc, NULL, 10);
      status->uidvalidity = idata->uid_validity;
    }
    else if (ascii_strncasecmp ("OK [UIDNEXT", pc, 11) == 0)
    {
      dprint (3, (debugfile, "Getting mailbox UIDNEXT\n"));
      pc += 3;
      pc = imap_next_word (pc);
      idata->uidnext = strtol (pc, NULL, 10);
      status->uidnext = idata->uidnext;
    }
    else
    {
      pc = imap_next_word (pc);
      if (!ascii_strncasecmp ("EXISTS", pc, 6))
      {
	count = idata->newMailCount;
	idata->newMailCount = 0;
      }
    }
  }
  while (rc == IMAP_CMD_CONTINUE);

  if (rc == IMAP_CMD_NO)
  {
    char *s;
    s = imap_next_word (idata->buf); /* skip seq */
    s = imap_next_word (s); /* Skip response */
    mutt_error ("%s", s);
    mutt_sleep (2);
    goto fail;
  }

  if (rc != IMAP_CMD_OK)
    goto fail;

  /* check for READ-ONLY notification */
  if (!ascii_strncasecmp (imap_get_qualifier (idata->buf), "[READ-ONLY]", 11)  \
  && !mutt_bit_isset (idata->capabilities, ACL))
  {
    dprint (2, (debugfile, "Mailbox is read-only.\n"));
    ctx->readonly = 1;
  }

#ifdef DEBUG
  /* dump the mailbox flags we've found */
  if (debuglevel > 2)
  {
    if (!idata->flags)
      dprint (3, (debugfile, "No folder flags found\n"));
    else
    {
      LIST* t = idata->flags;

      dprint (3, (debugfile, "Mailbox flags: "));

      t = t->next;
      while (t)
      {
        dprint (3, (debugfile, "[%s] ", t->data));
        t = t->next;
      }
      dprint (3, (debugfile, "\n"));
    }
  }
#endif

  if (!(mutt_bit_isset(idata->ctx->rights, M_ACL_DELETE) ||
        mutt_bit_isset(idata->ctx->rights, M_ACL_SEEN) ||
        mutt_bit_isset(idata->ctx->rights, M_ACL_WRITE) ||
        mutt_bit_isset(idata->ctx->rights, M_ACL_INSERT)))
     ctx->readonly = 1;

  ctx->hdrmax = count;
  ctx->hdrs = safe_calloc (count, sizeof (HEADER *));
  ctx->v2r = safe_calloc (count, sizeof (int));
  ctx->msgcount = 0;

  if (count && (imap_read_headers (idata, 0, count-1) < 0))
  {
    mutt_error _("Error opening mailbox");
    mutt_sleep (1);
    goto fail;
  }

  dprint (2, (debugfile, "imap_open_mailbox: msgcount is %d\n", ctx->msgcount));
  FREE (&mx.mbox);
  return 0;

 fail:
  if (idata->state == IMAP_SELECTED)
    idata->state = IMAP_AUTHENTICATED;
 fail_noidata:
  FREE (&mx.mbox);
  return -1;
}

int imap_open_mailbox_append (CONTEXT *ctx)
{
  CONNECTION *conn;
  IMAP_DATA *idata;
  char buf[LONG_STRING];
  char mailbox[LONG_STRING];
  IMAP_MBOX mx;
  int rc;

  if (imap_parse_path (ctx->path, &mx))
    return -1;

  /* in APPEND mode, we appear to hijack an existing IMAP connection -
   * ctx is brand new and mostly empty */

  if (!(idata = imap_conn_find (&(mx.account), 0)))
  {
    FREE (&mx.mbox);
    return -1;
  }

  conn = idata->conn;

  ctx->magic = M_IMAP;
  ctx->data = idata;

  imap_fix_path (idata, mx.mbox, mailbox, sizeof (mailbox));
  if (!*mailbox)
    strfcpy (mailbox, "INBOX", sizeof (mailbox));
  FREE (&mx.mbox);

  /* really we should also check for W_OK */
  if ((rc = imap_access (ctx->path, F_OK)) == 0)
    return 0;

  if (rc == -1)
    return -1;

  snprintf (buf, sizeof (buf), _("Create %s?"), mailbox);
  if (option (OPTCONFIRMCREATE) && mutt_yesorno (buf, 1) < 1)
    return -1;

  if (imap_create_mailbox (idata, mailbox) < 0)
    return -1;

  return 0;
}

/* imap_logout: Gracefully log out of server. */
void imap_logout (IMAP_DATA** idata)
{
  /* we set status here to let imap_handle_untagged know we _expect_ to
   * receive a bye response (so it doesn't freak out and close the conn) */
  (*idata)->status = IMAP_BYE;
  imap_cmd_start (*idata, "LOGOUT");
  while (imap_cmd_step (*idata) == IMAP_CMD_CONTINUE)
    ;

  mutt_socket_close ((*idata)->conn);
  imap_free_idata (idata);
}

/* imap_set_flag: append str to flags if we currently have permission
 *   according to aclbit */
static void imap_set_flag (IMAP_DATA* idata, int aclbit, int flag,
  const char *str, char *flags, size_t flsize)
{
  if (mutt_bit_isset (idata->ctx->rights, aclbit))
    if (flag && imap_has_flag (idata->flags, str))
      safe_strcat (flags, flsize, str);
}

/* imap_has_flag: do a caseless comparison of the flag against a flag list,
*   return 1 if found or flag list has '\*', 0 otherwise */
int imap_has_flag (LIST* flag_list, const char* flag)
{
  if (!flag_list)
    return 0;

  flag_list = flag_list->next;
  while (flag_list)
  {
    if (!ascii_strncasecmp (flag_list->data, flag, strlen (flag_list->data)))
      return 1;

    if (!ascii_strncmp (flag_list->data, "\\*", strlen (flag_list->data)))
      return 1;

    flag_list = flag_list->next;
  }

  return 0;
}

/* Note: headers must be in SORT_ORDER. See imap_exec_msgset for args.
 * Pos is an opaque pointer a la strtok. It should be 0 at first call. */
static int imap_make_msg_set (IMAP_DATA* idata, BUFFER* buf, int flag,
                              int changed, int invert, int* pos)
{
  HEADER** hdrs = idata->ctx->hdrs;
  int count = 0;	/* number of messages in message set */
  int match = 0;	/* whether current message matches flag condition */
  unsigned int setstart = 0;	/* start of current message range */
  int n;
  int started = 0;

  hdrs = idata->ctx->hdrs;

  for (n = *pos;
       n < idata->ctx->msgcount && buf->dptr - buf->data < IMAP_MAX_CMDLEN;
       n++)
  {
    match = 0;
    /* don't include pending expunged messages */
    if (hdrs[n]->active)
      switch (flag)
      {
        case M_DELETED:
          if (hdrs[n]->deleted != HEADER_DATA(hdrs[n])->deleted)
            match = invert ^ hdrs[n]->deleted;
	  break;
        case M_FLAG:
          if (hdrs[n]->flagged != HEADER_DATA(hdrs[n])->flagged)
            match = invert ^ hdrs[n]->flagged;
	  break;
        case M_OLD:
          if (hdrs[n]->old != HEADER_DATA(hdrs[n])->old)
            match = invert ^ hdrs[n]->old;
	  break;
        case M_READ:
          if (hdrs[n]->read != HEADER_DATA(hdrs[n])->read)
            match = invert ^ hdrs[n]->read;
	  break;
        case M_REPLIED:
          if (hdrs[n]->replied != HEADER_DATA(hdrs[n])->replied)
            match = invert ^ hdrs[n]->replied;
	  break;

        case M_TAG:
	  if (hdrs[n]->tagged)
	    match = 1;
	  break;
      }

    if (match && (!changed || hdrs[n]->changed))
    {
      count++;
      if (setstart == 0)
      {
        setstart = HEADER_DATA (hdrs[n])->uid;
        if (started == 0)
	{
	  mutt_buffer_printf (buf, "%u", HEADER_DATA (hdrs[n])->uid);
	  started = 1;
	}
        else
	  mutt_buffer_printf (buf, ",%u", HEADER_DATA (hdrs[n])->uid);
      }
      /* tie up if the last message also matches */
      else if (n == idata->ctx->msgcount-1)
	mutt_buffer_printf (buf, ":%u", HEADER_DATA (hdrs[n])->uid);
    }
    /* End current set if message doesn't match or we've reached the end
     * of the mailbox via inactive messages following the last match. */
    else if (setstart && (hdrs[n]->active || n == idata->ctx->msgcount-1))
    {
      if (HEADER_DATA (hdrs[n-1])->uid > setstart)
	mutt_buffer_printf (buf, ":%u", HEADER_DATA (hdrs[n-1])->uid);
      setstart = 0;
    }
  }

  *pos = n;

  return count;
}

/* Prepares commands for all messages matching conditions (must be flushed
 * with imap_exec)
 * Params:
 *   idata: IMAP_DATA containing context containing header set
 *   pre, post: commands are of the form "%s %s %s %s", tag,
 *     pre, message set, post
 *   flag: enum of flag type on which to filter
 *   changed: include only changed messages in message set
 *   invert: invert sense of flag, eg M_READ matches unread messages
 * Returns: number of matched messages, or -1 on failure */
int imap_exec_msgset (IMAP_DATA* idata, const char* pre, const char* post,
                      int flag, int changed, int invert)
{
  HEADER** hdrs = NULL;
  short oldsort;
  BUFFER* cmd;
  int pos;
  int rc;
  int count = 0;

  if (! (cmd = mutt_buffer_init (NULL)))
  {
    dprint (1, (debugfile, "imap_exec_msgset: unable to allocate buffer\n"));
    return -1;
  }

  /* We make a copy of the headers just in case resorting doesn't give
   exactly the original order (duplicate messages?), because other parts of
   the ctx are tied to the header order. This may be overkill. */
  oldsort = Sort;
  if (Sort != SORT_ORDER)
  {
    hdrs = idata->ctx->hdrs;
    idata->ctx->hdrs = safe_malloc (idata->ctx->msgcount * sizeof (HEADER*));
    memcpy (idata->ctx->hdrs, hdrs, idata->ctx->msgcount * sizeof (HEADER*));

    Sort = SORT_ORDER;
    qsort (idata->ctx->hdrs, idata->ctx->msgcount, sizeof (HEADER*),
           mutt_get_sort_func (SORT_ORDER));
  }

  pos = 0;

  do
  {
    cmd->dptr = cmd->data;
    mutt_buffer_printf (cmd, "%s ", pre);
    rc = imap_make_msg_set (idata, cmd, flag, changed, invert, &pos);
    if (rc > 0)
    {
      mutt_buffer_printf (cmd, " %s", post);
      if (imap_exec (idata, cmd->data, IMAP_CMD_QUEUE))
      {
        rc = -1;
        goto out;
      }
      count += rc;
    }
  }
  while (rc > 0);

  rc = count;

out:
  mutt_buffer_free (&cmd);
  if (oldsort != Sort)
  {
    Sort = oldsort;
    FREE (&idata->ctx->hdrs);
    idata->ctx->hdrs = hdrs;
  }

  return rc;
}

/* returns 0 if mutt's flags match cached server flags */
static int compare_flags (HEADER* h)
{
  IMAP_HEADER_DATA* hd = (IMAP_HEADER_DATA*)h->data;

  if (h->read != hd->read)
    return 1;
  if (h->old != hd->old)
    return 1;
  if (h->flagged != hd->flagged)
    return 1;
  if (h->replied != hd->replied)
    return 1;
  if (h->deleted != hd->deleted)
    return 1;

  return 0;
}

/* Update the IMAP server to reflect the flags a single message.  */
int imap_sync_message (IMAP_DATA *idata, HEADER *hdr, BUFFER *cmd,
		       int *err_continue)
{
  char flags[LONG_STRING];
  char uid[11];

  hdr->changed = 0;

  if (!compare_flags (hdr))
  {
    idata->ctx->changed--;
    return 0;
  }

  snprintf (uid, sizeof (uid), "%u", HEADER_DATA(hdr)->uid);
  cmd->dptr = cmd->data;
  mutt_buffer_addstr (cmd, "UID STORE ");
  mutt_buffer_addstr (cmd, uid);

  flags[0] = '\0';

  imap_set_flag (idata, M_ACL_SEEN, hdr->read, "\\Seen ",
		 flags, sizeof (flags));
  imap_set_flag (idata, M_ACL_WRITE, hdr->old,
                 "Old ", flags, sizeof (flags));
  imap_set_flag (idata, M_ACL_WRITE, hdr->flagged,
		 "\\Flagged ", flags, sizeof (flags));
  imap_set_flag (idata, M_ACL_WRITE, hdr->replied,
		 "\\Answered ", flags, sizeof (flags));
  imap_set_flag (idata, M_ACL_DELETE, hdr->deleted,
		 "\\Deleted ", flags, sizeof (flags));

  /* now make sure we don't lose custom tags */
  if (mutt_bit_isset (idata->ctx->rights, M_ACL_WRITE))
    imap_add_keywords (flags, hdr, idata->flags, sizeof (flags));

  mutt_remove_trailing_ws (flags);

  /* UW-IMAP is OK with null flags, Cyrus isn't. The only solution is to
   * explicitly revoke all system flags (if we have permission) */
  if (!*flags)
  {
    imap_set_flag (idata, M_ACL_SEEN, 1, "\\Seen ", flags, sizeof (flags));
    imap_set_flag (idata, M_ACL_WRITE, 1, "Old ", flags, sizeof (flags));
    imap_set_flag (idata, M_ACL_WRITE, 1, "\\Flagged ", flags, sizeof (flags));
    imap_set_flag (idata, M_ACL_WRITE, 1, "\\Answered ", flags, sizeof (flags));
    imap_set_flag (idata, M_ACL_DELETE, 1, "\\Deleted ", flags, sizeof (flags));

    mutt_remove_trailing_ws (flags);

    mutt_buffer_addstr (cmd, " -FLAGS.SILENT (");
  } else
    mutt_buffer_addstr (cmd, " FLAGS.SILENT (");

  mutt_buffer_addstr (cmd, flags);
  mutt_buffer_addstr (cmd, ")");

  /* dumb hack for bad UW-IMAP 4.7 servers spurious FLAGS updates */
  hdr->active = 0;

  /* after all this it's still possible to have no flags, if you
   * have no ACL rights */
  if (*flags && (imap_exec (idata, cmd->data, 0) != 0) &&
      err_continue && (*err_continue != M_YES))
  {
    *err_continue = imap_continue ("imap_sync_message: STORE failed",
				   idata->buf);
    if (*err_continue != M_YES)
      return -1;
  }

  hdr->active = 1;
  idata->ctx->changed--;

  return 0;
}

static int sync_helper (IMAP_DATA* idata, int right, int flag, const char* name)
{
  int count = 0;
  int rc;

  char buf[LONG_STRING];

  if (!mutt_bit_isset (idata->ctx->rights, right))
    return 0;

  if (right == M_ACL_WRITE && !imap_has_flag (idata->flags, name))
    return 0;

  snprintf (buf, sizeof(buf), "+FLAGS.SILENT (%s)", name);
  if ((rc = imap_exec_msgset (idata, "UID STORE", buf, flag, 1, 0)) < 0)
    return rc;
  count += rc;

  buf[0] = '-';
  if ((rc = imap_exec_msgset (idata, "UID STORE", buf, flag, 1, 1)) < 0)
    return rc;
  count += rc;

  return count;
}

/* update the IMAP server to reflect message changes done within mutt.
 * Arguments
 *   ctx: the current context
 *   expunge: 0 or 1 - do expunge?
 */
int imap_sync_mailbox (CONTEXT* ctx, int expunge, int* index_hint)
{
  IMAP_DATA* idata;
  CONTEXT* appendctx = NULL;
  HEADER* h;
  HEADER** hdrs = NULL;
  int oldsort;
  int n;
  int rc;

  idata = (IMAP_DATA*) ctx->data;

  if (idata->state < IMAP_SELECTED)
  {
    dprint (2, (debugfile, "imap_sync_mailbox: no mailbox selected\n"));
    return -1;
  }

  /* This function is only called when the calling code expects the context
   * to be changed. */
  imap_allow_reopen (ctx);

  if ((rc = imap_check_mailbox (ctx, index_hint, 0)) != 0)
    return rc;

  /* if we are expunging anyway, we can do deleted messages very quickly... */
  if (expunge && mutt_bit_isset (ctx->rights, M_ACL_DELETE))
  {
    if ((rc = imap_exec_msgset (idata, "UID STORE", "+FLAGS.SILENT (\\Deleted)",
                                M_DELETED, 1, 0)) < 0)
    {
      mutt_error (_("Expunge failed"));
      mutt_sleep (1);
      goto out;
    }

    if (rc > 0)
    {
      /* mark these messages as unchanged so second pass ignores them. Done
       * here so BOGUS UW-IMAP 4.7 SILENT FLAGS updates are ignored. */
      for (n = 0; n < ctx->msgcount; n++)
        if (ctx->hdrs[n]->deleted && ctx->hdrs[n]->changed)
          ctx->hdrs[n]->active = 0;
      mutt_message (_("Marking %d messages deleted..."), rc);
    }
  }

#if USE_HCACHE
  idata->hcache = imap_hcache_open (idata, NULL);
#endif

  /* save messages with real (non-flag) changes */
  for (n = 0; n < ctx->msgcount; n++)
  {
    h = ctx->hdrs[n];

    if (h->deleted)
    {
      imap_cache_del (idata, h);
#if USE_HCACHE
      imap_hcache_del (idata, HEADER_DATA(h)->uid);
#endif
    }

    if (h->active && h->changed)
    {
#if USE_HCACHE
      imap_hcache_put (idata, h);
#endif
      /* if the message has been rethreaded or attachments have been deleted
       * we delete the message and reupload it.
       * This works better if we're expunging, of course. */
      if ((h->env && (h->env->refs_changed || h->env->irt_changed)) ||
	  h->attach_del)
      {
        mutt_message (_("Saving changed messages... [%d/%d]"), n+1,
                      ctx->msgcount);
	if (!appendctx)
	  appendctx = mx_open_mailbox (ctx->path, M_APPEND | M_QUIET, NULL);
	if (!appendctx)
	  dprint (1, (debugfile, "imap_sync_mailbox: Error opening mailbox in append mode\n"));
	else
	  _mutt_save_message (h, appendctx, 1, 0, 0);
      }
    }
  }

#if USE_HCACHE
  imap_hcache_close (idata);
#endif

  /* sync +/- flags for the five flags mutt cares about */
  rc = 0;

  /* presort here to avoid doing 10 resorts in imap_exec_msgset */
  oldsort = Sort;
  if (Sort != SORT_ORDER)
  {
    hdrs = ctx->hdrs;
    ctx->hdrs = safe_malloc (ctx->msgcount * sizeof (HEADER*));
    memcpy (ctx->hdrs, hdrs, ctx->msgcount * sizeof (HEADER*));

    Sort = SORT_ORDER;
    qsort (ctx->hdrs, ctx->msgcount, sizeof (HEADER*),
           mutt_get_sort_func (SORT_ORDER));
  }

  rc += sync_helper (idata, M_ACL_DELETE, M_DELETED, "\\Deleted");
  rc += sync_helper (idata, M_ACL_WRITE, M_FLAG, "\\Flagged");
  rc += sync_helper (idata, M_ACL_WRITE, M_OLD, "Old");
  rc += sync_helper (idata, M_ACL_SEEN, M_READ, "\\Seen");
  rc += sync_helper (idata, M_ACL_WRITE, M_REPLIED, "\\Answered");

  if (oldsort != Sort)
  {
    Sort = oldsort;
    FREE (&ctx->hdrs);
    ctx->hdrs = hdrs;
  }

  if (rc && (imap_exec (idata, NULL, 0) != IMAP_CMD_OK))
  {
    if (ctx->closing)
    {
      if (mutt_yesorno (_("Error saving flags. Close anyway?"), 0) == M_YES)
      {
        rc = 0;
        idata->state = IMAP_AUTHENTICATED;
        goto out;
      }
    }
    else
      mutt_error _("Error saving flags");
    goto out;
  }

  for (n = 0; n < ctx->msgcount; n++)
    ctx->hdrs[n]->changed = 0;
  ctx->changed = 0;

  /* We must send an EXPUNGE command if we're not closing. */
  if (expunge && !(ctx->closing) &&
      mutt_bit_isset(ctx->rights, M_ACL_DELETE))
  {
    mutt_message _("Expunging messages from server...");
    /* Set expunge bit so we don't get spurious reopened messages */
    idata->reopen |= IMAP_EXPUNGE_EXPECTED;
    if (imap_exec (idata, "EXPUNGE", 0) != 0)
    {
      imap_error (_("imap_sync_mailbox: EXPUNGE failed"), idata->buf);
      rc = -1;
      goto out;
    }
  }

  if (expunge && ctx->closing)
  {
    imap_exec (idata, "CLOSE", IMAP_CMD_QUEUE);
    idata->state = IMAP_AUTHENTICATED;
  }

  if (option (OPTMESSAGECACHECLEAN))
    imap_cache_clean (idata);

  rc = 0;

 out:
  if (appendctx)
  {
    mx_fastclose_mailbox (appendctx);
    FREE (&appendctx);
  }
  return rc;
}

/* imap_close_mailbox: clean up IMAP data in CONTEXT */
int imap_close_mailbox (CONTEXT* ctx)
{
  IMAP_DATA* idata;
  int i;

  idata = (IMAP_DATA*) ctx->data;
  /* Check to see if the mailbox is actually open */
  if (!idata)
    return 0;

  if (ctx == idata->ctx)
  {
    if (idata->status != IMAP_FATAL && idata->state >= IMAP_SELECTED)
    {
      /* mx_close_mailbox won't sync if there are no deleted messages
       * and the mailbox is unchanged, so we may have to close here */
      if (!ctx->deleted)
        imap_exec (idata, "CLOSE", IMAP_CMD_QUEUE);
      idata->state = IMAP_AUTHENTICATED;
    }

    idata->reopen &= IMAP_REOPEN_ALLOW;
    FREE (&(idata->mailbox));
    mutt_free_list (&idata->flags);
    idata->ctx = NULL;
  }

  /* free IMAP part of headers */
  for (i = 0; i < ctx->msgcount; i++)
    /* mailbox may not have fully loaded */
    if (ctx->hdrs[i] && ctx->hdrs[i]->data)
      imap_free_header_data (&(ctx->hdrs[i]->data));

  for (i = 0; i < IMAP_CACHE_LEN; i++)
  {
    if (idata->cache[i].path)
    {
      unlink (idata->cache[i].path);
      FREE (&idata->cache[i].path);
    }
  }

  mutt_bcache_close (&idata->bcache);

  return 0;
}

/* use the NOOP or IDLE command to poll for new mail
 *
 * return values:
 *	M_REOPENED	mailbox has been externally modified
 *	M_NEW_MAIL	new mail has arrived!
 *	0		no change
 *	-1		error
 */
int imap_check_mailbox (CONTEXT *ctx, int *index_hint, int force)
{
  /* overload keyboard timeout to avoid many mailbox checks in a row.
   * Most users don't like having to wait exactly when they press a key. */
  IMAP_DATA* idata;
  int result = 0;

  idata = (IMAP_DATA*) ctx->data;

  /* try IDLE first, unless force is set */
  if (!force && option (OPTIMAPIDLE) && mutt_bit_isset (idata->capabilities, IDLE)
      && (idata->state != IMAP_IDLE || time(NULL) >= idata->lastread + ImapKeepalive))
  {
    if (imap_cmd_idle (idata) < 0)
      return -1;
  }
  if (idata->state == IMAP_IDLE)
  {
    while ((result = mutt_socket_poll (idata->conn)) > 0)
    {
      if (imap_cmd_step (idata) != IMAP_CMD_CONTINUE)
      {
        dprint (1, (debugfile, "Error reading IDLE response\n"));
        return -1;
      }
    }
    if (result < 0)
    {
      dprint (1, (debugfile, "Poll failed, disabling IDLE\n"));
      mutt_bit_unset (idata->capabilities, IDLE);
    }
  }

  if ((force ||
       (idata->state != IMAP_IDLE && time(NULL) >= idata->lastread + Timeout))
      && imap_exec (idata, "NOOP", 0) != 0)
    return -1;

  /* We call this even when we haven't run NOOP in case we have pending
   * changes to process, since we can reopen here. */
  imap_cmd_finish (idata);

  if (idata->check_status & IMAP_EXPUNGE_PENDING)
    result = M_REOPENED;
  else if (idata->check_status & IMAP_NEWMAIL_PENDING)
    result = M_NEW_MAIL;
  else if (idata->check_status & IMAP_FLAGS_PENDING)
    result = M_FLAGS;

  idata->check_status = 0;

  return result;
}

/* split path into (idata,mailbox name) */
static int imap_get_mailbox (const char* path, IMAP_DATA** hidata, char* buf, size_t blen)
{
  IMAP_MBOX mx;

  if (imap_parse_path (path, &mx))
  {
    dprint (1, (debugfile, "imap_get_mailbox: Error parsing %s\n", path));
    return -1;
  }
  if (!(*hidata = imap_conn_find (&(mx.account), option (OPTIMAPPASSIVE) ? M_IMAP_CONN_NONEW : 0))
      || (*hidata)->state < IMAP_AUTHENTICATED)
  {
    FREE (&mx.mbox);
    return -1;
  }

  imap_fix_path (*hidata, mx.mbox, buf, blen);
  if (!*buf)
    strfcpy (buf, "INBOX", blen);
  FREE (&mx.mbox);

  return 0;
}

/* check for new mail in any subscribed mailboxes. Given a list of mailboxes
 * rather than called once for each so that it can batch the commands and
 * save on round trips. Returns number of mailboxes with new mail. */
int imap_buffy_check (int force)
{
  IMAP_DATA* idata;
  IMAP_DATA* lastdata = NULL;
  BUFFY* mailbox;
  char name[LONG_STRING];
  char command[LONG_STRING];
  char munged[LONG_STRING];
  int buffies = 0;

  for (mailbox = Incoming; mailbox; mailbox = mailbox->next)
  {
    /* Init newly-added mailboxes */
    if (! mailbox->magic)
    {
      if (mx_is_imap (mailbox->path))
        mailbox->magic = M_IMAP;
    }

    if (mailbox->magic != M_IMAP)
      continue;

    mailbox->new = 0;

    if (imap_get_mailbox (mailbox->path, &idata, name, sizeof (name)) < 0)
      continue;

    /* Don't issue STATUS on the selected mailbox, it will be NOOPed or
     * IDLEd elsewhere.
     * idata->mailbox may be NULL for connections other than the current
     * mailbox's, and shouldn't expand to INBOX in that case. #3216. */
    if (idata->mailbox && !imap_mxcmp (name, idata->mailbox))
      continue;

    if (!mutt_bit_isset (idata->capabilities, IMAP4REV1) &&
        !mutt_bit_isset (idata->capabilities, STATUS))
    {
      dprint (2, (debugfile, "Server doesn't support STATUS\n"));
      continue;
    }

    if (lastdata && idata != lastdata)
    {
      /* Send commands to previous server. Sorting the buffy list
       * may prevent some infelicitous interleavings */
      if (imap_exec (lastdata, NULL, IMAP_CMD_FAIL_OK) == -1)
        dprint (1, (debugfile, "Error polling mailboxes\n"));

      lastdata = NULL;
    }

    if (!lastdata)
      lastdata = idata;

    imap_munge_mbox_name (munged, sizeof (munged), name);
    snprintf (command, sizeof (command),
	      "STATUS %s (UIDNEXT UIDVALIDITY UNSEEN RECENT)", munged);

    if (imap_exec (idata, command, IMAP_CMD_QUEUE) < 0)
    {
      dprint (1, (debugfile, "Error queueing command\n"));
      return 0;
    }
  }

  if (lastdata && (imap_exec (lastdata, NULL, IMAP_CMD_FAIL_OK) == -1))
  {
    dprint (1, (debugfile, "Error polling mailboxes\n"));
    return 0;
  }

  /* collect results */
  for (mailbox = Incoming; mailbox; mailbox = mailbox->next)
  {
    if (mailbox->magic == M_IMAP && mailbox->new)
      buffies++;
  }

  return buffies;
}

/* imap_status: returns count of messages in mailbox, or -1 on error.
 * if queue != 0, queue the command and expect it to have been run
 * on the next call (for pipelining the postponed count) */
int imap_status (char* path, int queue)
{
  static int queued = 0;

  IMAP_DATA *idata;
  char buf[LONG_STRING];
  char mbox[LONG_STRING];
  IMAP_STATUS* status;

  if (imap_get_mailbox (path, &idata, buf, sizeof (buf)) < 0)
    return -1;

  if (!imap_mxcmp (buf, idata->mailbox))
    /* We are in the folder we're polling - just return the mailbox count */
    return idata->ctx->msgcount;
  else if (mutt_bit_isset(idata->capabilities,IMAP4REV1) ||
	   mutt_bit_isset(idata->capabilities,STATUS))
  {
    imap_munge_mbox_name (mbox, sizeof(mbox), buf);
    snprintf (buf, sizeof (buf), "STATUS %s (%s)", mbox, "MESSAGES");
    imap_unmunge_mbox_name (mbox);
  }
  else
    /* Server does not support STATUS, and this is not the current mailbox.
     * There is no lightweight way to check recent arrivals */
    return -1;

  if (queue)
  {
    imap_exec (idata, buf, IMAP_CMD_QUEUE);
    queued = 1;
    return 0;
  }
  else if (!queued)
    imap_exec (idata, buf, 0);

  queued = 0;
  if ((status = imap_mboxcache_get (idata, mbox, 0)))
    return status->messages;

  return 0;
}

/* return cached mailbox stats or NULL if create is 0 */
IMAP_STATUS* imap_mboxcache_get (IMAP_DATA* idata, const char* mbox, int create)
{
  LIST* cur;
  IMAP_STATUS* status;
  IMAP_STATUS scache;
#ifdef USE_HCACHE
  header_cache_t *hc = NULL;
  unsigned int *uidvalidity = NULL;
  unsigned int *uidnext = NULL;
#endif

  for (cur = idata->mboxcache; cur; cur = cur->next)
  {
    status = (IMAP_STATUS*)cur->data;

    if (!imap_mxcmp (mbox, status->name))
      return status;
  }
  status = NULL;

  /* lame */
  if (create)
  {
    memset (&scache, 0, sizeof (scache));
    scache.name = (char*)mbox;
    idata->mboxcache = mutt_add_list_n (idata->mboxcache, &scache,
                                        sizeof (scache));
    status = imap_mboxcache_get (idata, mbox, 0);
    status->name = safe_strdup (mbox);
  }

#ifdef USE_HCACHE
  hc = imap_hcache_open (idata, mbox);
  if (hc)
  {
    uidvalidity = mutt_hcache_fetch_raw (hc, "/UIDVALIDITY", imap_hcache_keylen);
    uidnext = mutt_hcache_fetch_raw (hc, "/UIDNEXT", imap_hcache_keylen);
    mutt_hcache_close (hc);
    if (uidvalidity)
    {
      if (!status)
      {
        FREE (&uidvalidity);
        FREE (&uidnext);
        return imap_mboxcache_get (idata, mbox, 1);
      }
      status->uidvalidity = *uidvalidity;
      status->uidnext = uidnext ? *uidnext: 0;
      dprint (3, (debugfile, "mboxcache: hcache uidvalidity %d, uidnext %d\n",
                  status->uidvalidity, status->uidnext));
    }
    FREE (&uidvalidity);
    FREE (&uidnext);
  }
#endif

  return status;
}

void imap_mboxcache_free (IMAP_DATA* idata)
{
  LIST* cur;
  IMAP_STATUS* status;

  for (cur = idata->mboxcache; cur; cur = cur->next)
  {
    status = (IMAP_STATUS*)cur->data;

    FREE (&status->name);
  }

  mutt_free_list (&idata->mboxcache);
}

/* returns number of patterns in the search that should be done server-side
 * (eg are full-text) */
static int do_search (const pattern_t* search, int allpats)
{
  int rc = 0;
  const pattern_t* pat;

  for (pat = search; pat; pat = pat->next)
  {
    switch (pat->op)
    {
      case M_BODY:
      case M_HEADER:
      case M_WHOLE_MSG:
        if (pat->stringmatch)
          rc++;
        break;
      default:
        if (pat->child && do_search (pat->child, 1))
          rc++;
    }

    if (!allpats)
      break;
  }

  return rc;
}

/* convert mutt pattern_t to IMAP SEARCH command containing only elements
 * that require full-text search (mutt already has what it needs for most
 * match types, and does a better job (eg server doesn't support regexps). */
static int imap_compile_search (const pattern_t* pat, BUFFER* buf)
{
  if (! do_search (pat, 0))
    return 0;

  if (pat->not)
    mutt_buffer_addstr (buf, "NOT ");

  if (pat->child)
  {
    int clauses;

    if ((clauses = do_search (pat->child, 1)) > 0)
    {
      const pattern_t* clause = pat->child;

      mutt_buffer_addch (buf, '(');

      while (clauses)
      {
        if (do_search (clause, 0))
        {
          if (pat->op == M_OR && clauses > 1)
            mutt_buffer_addstr (buf, "OR ");
          clauses--;

          if (imap_compile_search (clause, buf) < 0)
            return -1;

          if (clauses)
            mutt_buffer_addch (buf, ' ');

        }
        clause = clause->next;
      }

      mutt_buffer_addch (buf, ')');
    }
  }
  else
  {
    char term[STRING];
    char *delim;

    switch (pat->op)
    {
      case M_HEADER:
        mutt_buffer_addstr (buf, "HEADER ");

        /* extract header name */
        if (! (delim = strchr (pat->p.str, ':')))
        {
          mutt_error (_("Header search without header name: %s"), pat->p.str);
          return -1;
        }
        *delim = '\0';
        imap_quote_string (term, sizeof (term), pat->p.str);
        mutt_buffer_addstr (buf, term);
        mutt_buffer_addch (buf, ' ');

        /* and field */
        *delim = ':';
        delim++;
        SKIPWS(delim);
        imap_quote_string (term, sizeof (term), delim);
        mutt_buffer_addstr (buf, term);
        break;
      case M_BODY:
        mutt_buffer_addstr (buf, "BODY ");
        imap_quote_string (term, sizeof (term), pat->p.str);
        mutt_buffer_addstr (buf, term);
        break;
      case M_WHOLE_MSG:
        mutt_buffer_addstr (buf, "TEXT ");
        imap_quote_string (term, sizeof (term), pat->p.str);
        mutt_buffer_addstr (buf, term);
        break;
    }
  }

  return 0;
}

int imap_search (CONTEXT* ctx, const pattern_t* pat)
{
  BUFFER buf;
  IMAP_DATA* idata = (IMAP_DATA*)ctx->data;
  int i;

  for (i = 0; i < ctx->msgcount; i++)
    ctx->hdrs[i]->matched = 0;

  if (!do_search (pat, 1))
    return 0;

  memset (&buf, 0, sizeof (buf));
  mutt_buffer_addstr (&buf, "UID SEARCH ");
  if (imap_compile_search (pat, &buf) < 0)
  {
    FREE (&buf.data);
    return -1;
  }
  if (imap_exec (idata, buf.data, 0) < 0)
  {
    FREE (&buf.data);
    return -1;
  }

  FREE (&buf.data);
  return 0;
}

int imap_subscribe (char *path, int subscribe)
{
  CONNECTION *conn;
  IMAP_DATA *idata;
  char buf[LONG_STRING];
  char mbox[LONG_STRING];
  char errstr[STRING];
  BUFFER err, token;
  IMAP_MBOX mx;

  if (!mx_is_imap (path) || imap_parse_path (path, &mx) || !mx.mbox)
  {
    mutt_error (_("Bad mailbox name"));
    return -1;
  }
  if (!(idata = imap_conn_find (&(mx.account), 0)))
    goto fail;

  conn = idata->conn;

  imap_fix_path (idata, mx.mbox, buf, sizeof (buf));
  if (!*buf)
    strfcpy (buf, "INBOX", sizeof (buf));

  if (option (OPTIMAPCHECKSUBSCRIBED))
  {
    memset (&token, 0, sizeof (token));
    err.data = errstr;
    err.dsize = sizeof (errstr);
    snprintf (mbox, sizeof (mbox), "%smailboxes \"%s\"",
              subscribe ? "" : "un", path);
    if (mutt_parse_rc_line (mbox, &token, &err))
      dprint (1, (debugfile, "Error adding subscribed mailbox: %s\n", errstr));
    FREE (&token.data);
  }

  if (subscribe)
    mutt_message (_("Subscribing to %s..."), buf);
  else
    mutt_message (_("Unsubscribing from %s..."), buf);
  imap_munge_mbox_name (mbox, sizeof(mbox), buf);

  snprintf (buf, sizeof (buf), "%sSUBSCRIBE %s", subscribe ? "" : "UN", mbox);

  if (imap_exec (idata, buf, 0) < 0)
    goto fail;

  imap_unmunge_mbox_name(mx.mbox);
  if (subscribe)
    mutt_message (_("Subscribed to %s"), mx.mbox);
  else
    mutt_message (_("Unsubscribed from %s"), mx.mbox);
  FREE (&mx.mbox);
  return 0;

 fail:
  FREE (&mx.mbox);
  return -1;
}

/* trim dest to the length of the longest prefix it shares with src,
 * returning the length of the trimmed string */
static int
longest_common_prefix (char *dest, const char* src, int start, size_t dlen)
{
  int pos = start;

  while (pos < dlen && dest[pos] && dest[pos] == src[pos])
    pos++;
  dest[pos] = '\0';

  return pos;
}

/* look for IMAP URLs to complete from defined mailboxes. Could be extended
 * to complete over open connections and account/folder hooks too. */
static int
imap_complete_hosts (char *dest, size_t len)
{
  BUFFY* mailbox;
  CONNECTION* conn;
  int rc = -1;
  int matchlen;

  matchlen = mutt_strlen (dest);
  for (mailbox = Incoming; mailbox; mailbox = mailbox->next)
  {
    if (!mutt_strncmp (dest, mailbox->path, matchlen))
    {
      if (rc)
      {
        strfcpy (dest, mailbox->path, len);
        rc = 0;
      }
      else
        longest_common_prefix (dest, mailbox->path, matchlen, len);
    }
  }

  for (conn = mutt_socket_head (); conn; conn = conn->next)
  {
    ciss_url_t url;
    char urlstr[LONG_STRING];

    if (conn->account.type != M_ACCT_TYPE_IMAP)
      continue;

    mutt_account_tourl (&conn->account, &url);
    /* FIXME: how to handle multiple users on the same host? */
    url.user = NULL;
    url.path = NULL;
    url_ciss_tostring (&url, urlstr, sizeof (urlstr), 0);
    if (!mutt_strncmp (dest, urlstr, matchlen))
    {
      if (rc)
      {
        strfcpy (dest, urlstr, len);
        rc = 0;
      }
      else
        longest_common_prefix (dest, urlstr, matchlen, len);
    }
  }

  return rc;
}

/* imap_complete: given a partial IMAP folder path, return a string which
 *   adds as much to the path as is unique */
int imap_complete(char* dest, size_t dlen, char* path) {
  CONNECTION* conn;
  IMAP_DATA* idata;
  char list[LONG_STRING];
  char buf[LONG_STRING];
  IMAP_LIST listresp;
  char completion[LONG_STRING];
  int clen, matchlen = 0;
  int completions = 0;
  IMAP_MBOX mx;
  int rc;

  if (imap_parse_path (path, &mx))
  {
    strfcpy (dest, path, dlen);
    return imap_complete_hosts (dest, dlen);
  }

  /* don't open a new socket just for completion. Instead complete over
   * known mailboxes/hooks/etc */
  if (!(idata = imap_conn_find (&(mx.account), M_IMAP_CONN_NONEW)))
  {
    FREE (&mx.mbox);
    strfcpy (dest, path, dlen);
    return imap_complete_hosts (dest, dlen);
  }
  conn = idata->conn;

  /* reformat path for IMAP list, and append wildcard */
  /* don't use INBOX in place of "" */
  if (mx.mbox && mx.mbox[0])
    imap_fix_path (idata, mx.mbox, list, sizeof(list));
  else
    list[0] = '\0';

  /* fire off command */
  snprintf (buf, sizeof(buf), "%s \"\" \"%s%%\"",
    option (OPTIMAPLSUB) ? "LSUB" : "LIST", list);

  imap_cmd_start (idata, buf);

  /* and see what the results are */
  strfcpy (completion, NONULL(mx.mbox), sizeof(completion));
  idata->cmdtype = IMAP_CT_LIST;
  idata->cmddata = &listresp;
  do
  {
    listresp.name = NULL;
    rc = imap_cmd_step (idata);

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
        strfcpy (completion, listresp.name, sizeof(completion));
        matchlen = strlen (completion);
        completions++;
        continue;
      }

      matchlen = longest_common_prefix (completion, listresp.name, 0, matchlen);
      completions++;
    }
  }
  while (rc == IMAP_CMD_CONTINUE);
  idata->cmddata = NULL;

  if (completions)
  {
    /* reformat output */
    imap_qualify_path (dest, dlen, &mx, completion);
    mutt_pretty_mailbox (dest, dlen);

    FREE (&mx.mbox);
    return 0;
  }

  return -1;
}
