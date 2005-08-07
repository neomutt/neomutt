/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1996-9 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2005 Brendan Cully <brendan@kublai.com>
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
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */ 

/* Support for IMAP4rev1, with the occasional nod to IMAP 4. */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mutt_curses.h"
#include "mx.h"
#include "mailbox.h"
#include "globals.h"
#include "sort.h"
#include "browser.h"
#include "message.h"
#include "imap_private.h"
#if defined(USE_SSL) || defined(USE_GNUTLS)
# include "mutt_ssl.h"
#endif
#include "buffy.h"

#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

/* imap forward declarations */
static int imap_get_delim (IMAP_DATA *idata);
static char* imap_get_flags (LIST** hflags, char* s);
static int imap_check_acl (IMAP_DATA *idata);
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

  if (imap_parse_path (path, &mx))
    return -1;

  if (!(idata = imap_conn_find (&mx.account,
    option (OPTIMAPPASSIVE) ? M_IMAP_CONN_NONEW : 0)))
  {
    FREE (&mx.mbox);
    return -1;
  }

  imap_fix_path (idata, mx.mbox, mailbox, sizeof (mailbox));

  /* we may already be in the folder we're checking */
  if (!ascii_strcmp(idata->mailbox, mx.mbox))
  {
    FREE (&mx.mbox);
    return 0;
  }

  FREE (&mx.mbox);
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

  if (imap_exec (idata, buf, IMAP_CMD_FAIL_OK) < 0)
  {
    dprint (1, (debugfile, "imap_access: Can't check STATUS of %s\n", mbox));
    return -1;
  }

  return 0;
}

int imap_create_mailbox (IMAP_DATA* idata, char* mailbox)
{
  char buf[LONG_STRING], mbox[LONG_STRING];

  imap_munge_mbox_name (mbox, sizeof (mbox), mailbox);
  snprintf (buf, sizeof (buf), "CREATE %s", mbox);
      
  if (imap_exec (idata, buf, 0) != 0)
    return -1;

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
      imap_logout ((IMAP_DATA*) conn->data);
      mutt_clear_error ();
      mutt_socket_close (conn);
      mutt_socket_free (conn);
    }

    conn = tmp;
  }
}

/* imap_read_literal: read bytes bytes from server into file. Not explicitly
 *   buffered, relies on FILE buffering. NOTE: strips \r from \r\n.
 *   Apparently even literals use \r\n-terminated strings ?! */
int imap_read_literal (FILE* fp, IMAP_DATA* idata, long bytes)
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

  for (i = 0; i < idata->ctx->msgcount; i++)
  {
    h = idata->ctx->hdrs[i];

    if (h->index == -1)
    {
      dprint (2, (debugfile, "Expunging message UID %d.\n", HEADER_DATA (h)->uid));

      h->active = 0;

      /* free cached body from disk, if neccessary */
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

  /* We may be called on to expunge at any time. We can't rely on the caller
   * to always know to rethread */
  mx_update_tables (idata->ctx, 0);
  mutt_sort_headers (idata->ctx, 1);
}

static int imap_get_delim (IMAP_DATA *idata)
{
  char *s;
  int rc;

  /* assume that the delim is /.  If this fails, we're in bigger trouble
   * than getting the delim wrong */
  idata->delim = '/';

  imap_cmd_start (idata, "LIST \"\" \"\"");

  do 
  {
    if ((rc = imap_cmd_step (idata)) != IMAP_CMD_CONTINUE)
      break;

    s = imap_next_word (idata->cmd.buf);
    if (ascii_strncasecmp ("LIST", s, 4) == 0)
    {
      s = imap_next_word (s);
      s = imap_next_word (s);
      if (s && s[0] == '\"' && s[1] && s[2] == '\"')
	idata->delim = s[1];
      else if (s && s[0] == '\"' && s[1] && s[1] == '\\' && s[2] && s[3] == '\"')
	idata->delim = s[2];
    }
  }
  while (rc == IMAP_CMD_CONTINUE);

  if (rc != IMAP_CMD_OK)
  {
    dprint (1, (debugfile, "imap_get_delim: failed.\n"));
    return -1;
  }

  dprint (2, (debugfile, "Delimiter: %c\n", idata->delim));

  return -1;
}

/* get rights for folder, let imap_handle_untagged do the rest */
static int imap_check_acl (IMAP_DATA *idata)
{
  char buf[LONG_STRING];
  char mbox[LONG_STRING];

  imap_munge_mbox_name (mbox, sizeof(mbox), idata->mailbox);
  snprintf (buf, sizeof (buf), "MYRIGHTS %s", mbox);
  if (imap_exec (idata, buf, 0) != 0)
  {
    imap_error ("imap_check_acl", buf);
    return -1;
  }
  return 0;
}

/* imap_check_capabilities: make sure we can log in to this server. */
static int imap_check_capabilities (IMAP_DATA* idata)
{
  if (imap_exec (idata, "CAPABILITY", 0) != 0)
  {
    imap_error ("imap_check_capabilities", idata->cmd.buf);
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
  CONNECTION* conn;
  IMAP_DATA* idata;
  ACCOUNT* creds;

  if (!(conn = mutt_conn_find (NULL, account)))
    return NULL;

  /* if opening a new UNSELECTED connection, preserve existing creds */
  creds = &(conn->account);

  /* make sure this connection is not in SELECTED state, if neccessary */
  if (flags & M_IMAP_CONN_NOSELECT)
    while (conn->data && ((IMAP_DATA*) conn->data)->state == IMAP_SELECTED)
    {
      if (!(conn = mutt_conn_find (conn, account)))
	return NULL;
      memcpy (&(conn->account), creds, sizeof (ACCOUNT));
    }
  
  idata = (IMAP_DATA*) conn->data;

  /* don't open a new connection if one isn't wanted */
  if (flags & M_IMAP_CONN_NONEW)
  {
    if (!idata)
    {
      mutt_socket_free (conn);
      return NULL;
    }
    if (idata->state < IMAP_AUTHENTICATED)
      return NULL;
  }
  
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
  }

  if (idata->state == IMAP_DISCONNECTED)
    imap_open_connection (idata);
  if (idata->state == IMAP_CONNECTED)
  {
    if (!imap_authenticate (idata))
    {
      idata->state = IMAP_AUTHENTICATED;
      if (idata->conn->ssf)
	dprint (2, (debugfile, "Communication encrypted at %d bits\n",
		    idata->conn->ssf));
    }
    else
      mutt_account_unsetpass (&idata->conn->account);
    
    FREE (&idata->capstr);
  }
  if (idata->state == IMAP_AUTHENTICATED)
  {
    imap_get_delim (idata);
    if (option (OPTIMAPCHECKSUBSCRIBED))
    {
      mutt_message _("Checking mailbox subscriptions");
      imap_exec (idata, "LSUB \"\" \"*\"", 0);
    }
  }

  return idata;
}

int imap_open_connection (IMAP_DATA* idata)
{
  char buf[LONG_STRING];

  if (mutt_socket_open (idata->conn) < 0)
    return -1;

  idata->state = IMAP_CONNECTED;

  if (imap_cmd_step (idata) != IMAP_CMD_CONTINUE) {
    mutt_socket_close (idata->conn);
    idata->state = IMAP_DISCONNECTED;
    return -1;
  }

  if (ascii_strncasecmp ("* OK", idata->cmd.buf, 4) == 0)
  {
    /* TODO: Parse new tagged CAPABILITY data (* OK [CAPABILITY...]) */
    if (imap_check_capabilities (idata))
      goto bail;
#if defined(USE_SSL) || defined(USE_GNUTLS)
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
#ifdef USE_SSL
	  if (mutt_ssl_starttls (idata->conn))
#elif USE_GNUTLS
	  if (mutt_gnutls_starttls (idata->conn))
#endif
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
  else if (ascii_strncasecmp ("* PREAUTH", idata->cmd.buf, 9) == 0)
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

 err_close_conn:
  mutt_socket_close (idata->conn);
  idata->state = IMAP_DISCONNECTED;
 bail:
  FREE (&idata->capstr);
  return -1;
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
  char buf[LONG_STRING];
  char bufout[LONG_STRING];
  int count = 0;
  IMAP_MBOX mx;
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

  /* Clean up path and replace the one in the ctx */
  imap_fix_path (idata, mx.mbox, buf, sizeof (buf));
  FREE(&(idata->mailbox));
  idata->mailbox = safe_strdup (buf);
  imap_qualify_path (buf, sizeof (buf), &mx, idata->mailbox);

  FREE (&(ctx->path));
  ctx->path = safe_strdup (buf);

  idata->ctx = ctx;

  /* clear mailbox status */
  idata->status = 0;
  memset (idata->rights, 0, (RIGHTSMAX+7)/8);
  idata->newMailCount = 0;

  mutt_message (_("Selecting %s..."), idata->mailbox);
  imap_munge_mbox_name (buf, sizeof(buf), idata->mailbox);
  snprintf (bufout, sizeof (bufout), "%s %s",
    ctx->readonly ? "EXAMINE" : "SELECT", buf);

  idata->state = IMAP_SELECTED;

  imap_cmd_start (idata, bufout);

  do
  {
    char *pc;
    
    if ((rc = imap_cmd_step (idata)) != IMAP_CMD_CONTINUE)
      break;

    pc = idata->cmd.buf + 2;

    /* Obtain list of available flags here, may be overridden by a
     * PERMANENTFLAGS tag in the OK response */
    if (ascii_strncasecmp ("FLAGS", pc, 5) == 0)
    {
      /* don't override PERMANENTFLAGS */
      if (!idata->flags)
      {
	dprint (2, (debugfile, "Getting mailbox FLAGS\n"));
	if ((pc = imap_get_flags (&(idata->flags), pc)) == NULL)
	  goto fail;
      }
    }
    /* PERMANENTFLAGS are massaged to look like FLAGS, then override FLAGS */
    else if (ascii_strncasecmp ("OK [PERMANENTFLAGS", pc, 18) == 0)
    {
      dprint (2, (debugfile, "Getting mailbox PERMANENTFLAGS\n"));
      /* safe to call on NULL */
      mutt_free_list (&(idata->flags));
      /* skip "OK [PERMANENT" so syntax is the same as FLAGS */
      pc += 13;
      if ((pc = imap_get_flags (&(idata->flags), pc)) == NULL)
	goto fail;
    }
#ifdef USE_HCACHE
    /* save UIDVALIDITY for the header cache */
    else if (ascii_strncasecmp("OK [UIDVALIDITY", pc, 14) == 0)
    {
      dprint(2, (debugfile, "Getting mailbox UIDVALIDITY\n"));
      pc += 3;
      pc = imap_next_word(pc);
      sscanf(pc, "%lu", &(idata->uid_validity));
    }
#endif
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
    s = imap_next_word (idata->cmd.buf); /* skip seq */
    s = imap_next_word (s); /* Skip response */
    mutt_error ("%s", s);
    mutt_sleep (2);
    goto fail;
  }

  if (rc != IMAP_CMD_OK)
    goto fail;

  /* check for READ-ONLY notification */
  if (!ascii_strncasecmp (imap_get_qualifier (idata->cmd.buf), "[READ-ONLY]", 11)  \
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

  if (mutt_bit_isset (idata->capabilities, ACL))
  {
    if (imap_check_acl (idata))
      goto fail;
    if (!(mutt_bit_isset(idata->rights, IMAP_ACL_DELETE) ||
          mutt_bit_isset(idata->rights, IMAP_ACL_SEEN) ||
          mutt_bit_isset(idata->rights, IMAP_ACL_WRITE) ||
          mutt_bit_isset(idata->rights, IMAP_ACL_INSERT)))
       ctx->readonly = 1;
  }
  /* assume we have all rights if ACL is unavailable */
  else
  {
    mutt_bit_set (idata->rights, IMAP_ACL_LOOKUP);
    mutt_bit_set (idata->rights, IMAP_ACL_READ);
    mutt_bit_set (idata->rights, IMAP_ACL_SEEN);
    mutt_bit_set (idata->rights, IMAP_ACL_WRITE);
    mutt_bit_set (idata->rights, IMAP_ACL_INSERT);
    mutt_bit_set (idata->rights, IMAP_ACL_POST);
    mutt_bit_set (idata->rights, IMAP_ACL_CREATE);
    mutt_bit_set (idata->rights, IMAP_ACL_DELETE);
  }

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
  FREE (&mx.mbox);

  /* really we should also check for W_OK */
  if (!imap_access (ctx->path, F_OK))
    return 0;

  snprintf (buf, sizeof (buf), _("Create %s?"), mailbox);
  if (option (OPTCONFIRMCREATE) && mutt_yesorno (buf, 1) < 1)
    return -1;

  if (imap_create_mailbox (idata, mailbox) < 0)
    return -1;

  return 0;
}

/* imap_logout: Gracefully log out of server. */
void imap_logout (IMAP_DATA* idata)
{
  /* we set status here to let imap_handle_untagged know we _expect_ to
   * receive a bye response (so it doesn't freak out and close the conn) */
  idata->status = IMAP_BYE;
  imap_cmd_start (idata, "LOGOUT");
  while (imap_cmd_step (idata) == IMAP_CMD_CONTINUE)
    ;

  FREE(& idata->cmd.buf);
  FREE(& idata);
}

/* imap_set_flag: append str to flags if we currently have permission
 *   according to aclbit */
static void imap_set_flag (IMAP_DATA* idata, int aclbit, int flag,
  const char *str, char *flags, size_t flsize)
{
  if (mutt_bit_isset (idata->rights, aclbit))
    if (flag)
      safe_strcat (flags, flsize, str);
}

/* imap_make_msg_set: make an IMAP4rev1 UID message set out of a set of
 *   headers, given a flag enum to filter on.
 * Params: idata: IMAP_DATA containing context containing header set
 *         buf: to write message set into
 *         buflen: length of buffer
 *         flag: enum of flag type on which to filter
 *         changed: include only changed messages in message set
 * Returns: number of messages in message set (0 if no matches) */
int imap_make_msg_set (IMAP_DATA* idata, BUFFER* buf, int flag, int changed)
{
  HEADER** hdrs;	/* sorted local copy */
  int count = 0;	/* number of messages in message set */
  int match = 0;	/* whether current message matches flag condition */
  unsigned int setstart = 0;	/* start of current message range */
  int n;
  short oldsort;	/* we clobber reverse, must restore it */
  /* assuming 32-bit UIDs */
  char uid[12];
  int started = 0;

  /* make copy of header pointers to sort in natural order */
  hdrs = safe_calloc (idata->ctx->msgcount, sizeof (HEADER*));
  memcpy (hdrs, idata->ctx->hdrs, idata->ctx->msgcount * sizeof (HEADER*));

  if (Sort != SORT_ORDER)
  {
    oldsort = Sort;
    Sort = SORT_ORDER;
    qsort ((void*) hdrs, idata->ctx->msgcount, sizeof (HEADER*),
      mutt_get_sort_func (SORT_ORDER));
    Sort = oldsort;
  }
  
  for (n = 0; n < idata->ctx->msgcount; n++)
  {
    match = 0;
    /* don't include pending expunged messages */
    if (hdrs[n]->active)
      switch (flag)
      {
        case M_DELETE:
	  if (hdrs[n]->deleted)
	    match = 1;
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
	  snprintf (uid, sizeof (uid), "%u", HEADER_DATA (hdrs[n])->uid);
	  mutt_buffer_addstr (buf, uid);
	  started = 1;
	}
        else
        {
	  snprintf (uid, sizeof (uid), ",%u", HEADER_DATA (hdrs[n])->uid);
	  mutt_buffer_addstr (buf, uid);
        }
      }
      /* tie up if the last message also matches */
      else if (n == idata->ctx->msgcount-1)
      {
	snprintf (uid, sizeof (uid), ":%u", HEADER_DATA (hdrs[n])->uid);
	mutt_buffer_addstr (buf, uid);
      }
    }
    /* this message is not expunged and doesn't match. End current set. */
    else if (setstart && hdrs[n]->active)
    {
      if (HEADER_DATA (hdrs[n-1])->uid > setstart)
      {
	snprintf (uid, sizeof (uid), ":%u", HEADER_DATA (hdrs[n-1])->uid);
	mutt_buffer_addstr (buf, uid);
      }
      setstart = 0;
    }
  }

  FREE (&hdrs);

  return count;
}

/* Update the IMAP server to reflect the flags a single message.  */

int imap_sync_message (IMAP_DATA *idata, HEADER *hdr, BUFFER *cmd,
		       int *err_continue)
{
  char flags[LONG_STRING];
  char uid[11];

  hdr->changed = 0;

  snprintf (uid, sizeof (uid), "%u", HEADER_DATA(hdr)->uid);
  cmd->dptr = cmd->data;
  mutt_buffer_addstr (cmd, "UID STORE ");
  mutt_buffer_addstr (cmd, uid);

  flags[0] = '\0';
      
  imap_set_flag (idata, IMAP_ACL_SEEN, hdr->read, "\\Seen ",
		 flags, sizeof (flags));
  imap_set_flag (idata, IMAP_ACL_WRITE, hdr->flagged,
		 "\\Flagged ", flags, sizeof (flags));
  imap_set_flag (idata, IMAP_ACL_WRITE, hdr->replied,
		 "\\Answered ", flags, sizeof (flags));
  imap_set_flag (idata, IMAP_ACL_DELETE, hdr->deleted,
		 "\\Deleted ", flags, sizeof (flags));

  /* now make sure we don't lose custom tags */
  if (mutt_bit_isset (idata->rights, IMAP_ACL_WRITE))
    imap_add_keywords (flags, hdr, idata->flags, sizeof (flags));

  mutt_remove_trailing_ws (flags);

  /* UW-IMAP is OK with null flags, Cyrus isn't. The only solution is to
   * explicitly revoke all system flags (if we have permission) */
  if (!*flags)
  {
    imap_set_flag (idata, IMAP_ACL_SEEN, 1, "\\Seen ", flags, sizeof (flags));
    imap_set_flag (idata, IMAP_ACL_WRITE, 1, "\\Flagged ", flags, sizeof (flags));
    imap_set_flag (idata, IMAP_ACL_WRITE, 1, "\\Answered ", flags, sizeof (flags));
    imap_set_flag (idata, IMAP_ACL_DELETE, 1, "\\Deleted ", flags, sizeof (flags));

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
				   idata->cmd.buf);
    if (*err_continue != M_YES)
      return -1;
  }

  hdr->active = 1;
  idata->ctx->changed--;

  return 0;
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
  BUFFER cmd;
  int deleted;
  int n;
  int rc;
  int err_continue = M_NO;	/* continue on error? */

  idata = (IMAP_DATA*) ctx->data;

  if (idata->state != IMAP_SELECTED)
  {
    dprint (2, (debugfile, "imap_sync_mailbox: no mailbox selected\n"));
    return -1;
  }

  /* This function is only called when the calling code	expects the context
   * to be changed. */
  imap_allow_reopen (ctx);

  if ((rc = imap_check_mailbox (ctx, index_hint, 0)) != 0)
    return rc;

  memset (&cmd, 0, sizeof (cmd));

  /* if we are expunging anyway, we can do deleted messages very quickly... */
  if (expunge && mutt_bit_isset (idata->rights, IMAP_ACL_DELETE))
  {
    mutt_buffer_addstr (&cmd, "UID STORE ");
    deleted = imap_make_msg_set (idata, &cmd, M_DELETE, 1);

    /* if we have a message set, then let's delete */
    if (deleted)
    {
      mutt_message (_("Marking %d messages deleted..."), deleted);
      mutt_buffer_addstr (&cmd, " +FLAGS.SILENT (\\Deleted)");
      /* mark these messages as unchanged so second pass ignores them. Done
       * here so BOGUS UW-IMAP 4.7 SILENT FLAGS updates are ignored. */
      for (n = 0; n < ctx->msgcount; n++)
	if (ctx->hdrs[n]->deleted && ctx->hdrs[n]->changed)
	  ctx->hdrs[n]->active = 0;
      if (imap_exec (idata, cmd.data, 0) != 0)
      {
	mutt_error (_("Expunge failed"));
	mutt_sleep (1);
	rc = -1;
	goto out;
      }
    }
  }

  /* save status changes */
  for (n = 0; n < ctx->msgcount; n++)
  {
    if (ctx->hdrs[n]->active && ctx->hdrs[n]->changed)
    {
      mutt_message (_("Saving message status flags... [%d/%d]"), n+1,
        ctx->msgcount);

      /* if the message has been rethreaded or attachments have been deleted
       * we delete the message and reupload it.
       * This works better if we're expunging, of course. */
      if ((ctx->hdrs[n]->env && (ctx->hdrs[n]->env->refs_changed || ctx->hdrs[n]->env->irt_changed)) ||
	  ctx->hdrs[n]->attach_del)
      {
	dprint (3, (debugfile, "imap_sync_mailbox: Attachments to be deleted, falling back to _mutt_save_message\n"));
	if (!appendctx)
	  appendctx = mx_open_mailbox (ctx->path, M_APPEND | M_QUIET, NULL);
	if (!appendctx)
	{
	  dprint (1, (debugfile, "imap_sync_mailbox: Error opening mailbox in append mode\n"));
	}
	else
	  _mutt_save_message (ctx->hdrs[n], appendctx, 1, 0, 0);
      }

      if (imap_sync_message (idata, ctx->hdrs[n], &cmd, &err_continue) < 0)
      {
	rc = -1;
	goto out;
      }
    }
  }
  ctx->changed = 0;

  /* We must send an EXPUNGE command if we're not closing. */
  if (expunge && !(ctx->closing) &&
      mutt_bit_isset(idata->rights, IMAP_ACL_DELETE))
  {
    mutt_message _("Expunging messages from server...");
    /* Set expunge bit so we don't get spurious reopened messages */
    idata->reopen |= IMAP_EXPUNGE_EXPECTED;
    if (imap_exec (idata, "EXPUNGE", 0) != 0)
    {
      imap_error (_("imap_sync_mailbox: EXPUNGE failed"), idata->cmd.buf);
      rc = -1;
      goto out;
    }
  }

  if (expunge && ctx->closing)
  {
    if (imap_exec (idata, "CLOSE", 0))
      mutt_error (_("CLOSE failed"));
  
    idata->state = IMAP_AUTHENTICATED;
  }

  rc = 0;
 out:
  if (cmd.data)
    FREE (&cmd.data);
  if (appendctx)
  {
    mx_fastclose_mailbox (appendctx);
    FREE (&appendctx);
  }
  return rc;
}

/* imap_close_mailbox: clean up IMAP data in CONTEXT */
void imap_close_mailbox (CONTEXT* ctx)
{
  IMAP_DATA* idata;
  int i;

  idata = (IMAP_DATA*) ctx->data;
  /* Check to see if the mailbox is actually open */
  if (!idata)
    return;

  if (ctx == idata->ctx)
  {
    if (idata->status != IMAP_FATAL && idata->state == IMAP_SELECTED)
    {
      /* mx_close_mailbox won't sync if there are no deleted messages
       * and the mailbox is unchanged, so we may have to close here */
      if (!ctx->deleted && imap_exec (idata, "CLOSE", 0))
        mutt_error (_("CLOSE failed"));
      idata->state = IMAP_AUTHENTICATED;
    }

    idata->reopen &= IMAP_REOPEN_ALLOW;
    FREE (&(idata->mailbox));
    mutt_free_list (&idata->flags);
    idata->ctx = NULL;
  }

  /* free IMAP part of headers */
  for (i = 0; i < ctx->msgcount; i++)
    imap_free_header_data (&(ctx->hdrs[i]->data));

  for (i = 0; i < IMAP_CACHE_LEN; i++)
  {
    if (idata->cache[i].path)
    {
      unlink (idata->cache[i].path);
      FREE (&idata->cache[i].path);
    }
  }
}

/* use the NOOP command to poll for new mail
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

  if ((force || time(NULL) > idata->lastread + Timeout)
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

/* returns count of recent messages if new = 1, else count of total messages.
 * (useful for at least postponed function)
 * Question of taste: use RECENT or UNSEEN for new?
 *   0+   number of messages in mailbox
 *  -1    error while polling mailboxes
 */
int imap_mailbox_check (char* path, int new)
{
  CONNECTION *conn;
  IMAP_DATA *idata;
  char buf[LONG_STRING];
  char mbox[LONG_STRING];
  char mbox_unquoted[LONG_STRING];
  char *s;
  int msgcount = 0;
  int connflags = 0;
  IMAP_MBOX mx;
  int rc;
  
  if (imap_parse_path (path, &mx))
    return -1;

  /* If imap_passive is set, don't open a connection to check for new mail */
  if (option (OPTIMAPPASSIVE))
    connflags = M_IMAP_CONN_NONEW;

  if (!(idata = imap_conn_find (&(mx.account), connflags)))
  {
    FREE (&mx.mbox);
    return -1;
  }
  conn = idata->conn;

  imap_fix_path (idata, mx.mbox, buf, sizeof (buf));
  FREE (&mx.mbox);

  imap_munge_mbox_name (mbox, sizeof(mbox), buf);
  strfcpy (mbox_unquoted, buf, sizeof (mbox_unquoted));

  /* The draft IMAP implementor's guide warns againts using the STATUS
   * command on a mailbox that you have selected 
   */

  if (mutt_strcmp (mbox_unquoted, idata->mailbox) == 0
      || (ascii_strcasecmp (mbox_unquoted, "INBOX") == 0
	  && mutt_strcasecmp (mbox_unquoted, idata->mailbox) == 0))
  {
    strfcpy (buf, "NOOP", sizeof (buf));
  }
  else if (mutt_bit_isset(idata->capabilities,IMAP4REV1) ||
	   mutt_bit_isset(idata->capabilities,STATUS))
  {				
    snprintf (buf, sizeof (buf), "STATUS %s (%s)", mbox,
      new ? "RECENT" : "MESSAGES");
  }
  else
    /* Server does not support STATUS, and this is not the current mailbox.
     * There is no lightweight way to check recent arrivals */
    return -1;

  imap_cmd_start (idata, buf);

  do 
  {
    if ((rc = imap_cmd_step (idata)) != IMAP_CMD_CONTINUE)
      break;

    s = imap_next_word (idata->cmd.buf);
    if (ascii_strncasecmp ("STATUS", s, 6) == 0)
    {
      s = imap_next_word (s);
      /* The mailbox name may or may not be quoted here. We could try to 
       * munge the server response and compare with quoted (or vise versa)
       * but it is probably more efficient to just strncmp against both. */
      if (mutt_strncmp (mbox_unquoted, s, mutt_strlen (mbox_unquoted)) == 0
	  || mutt_strncmp (mbox, s, mutt_strlen (mbox)) == 0)
      {
	s = imap_next_word (s);
	s = imap_next_word (s);
	if (isdigit ((unsigned char) *s))
	{
	  if (*s != '0')
	  {
	    msgcount = atoi(s);
	    dprint (2, (debugfile, "%d new messages in %s\n", msgcount, path));
	  }
	}
      }
      else
	dprint (1, (debugfile, "imap_mailbox_check: STATUS response doesn't match requested mailbox.\n"));
    }
  }
  while (rc == IMAP_CMD_CONTINUE);

  return msgcount;
}

/* all this listing/browsing is a mess. I don't like that name is a pointer
 *   into idata->buf (used to be a pointer into the passed in buffer, just
 *   as bad), nor do I like the fact that the fetch is done here. This
 *   code can't possibly handle non-LIST untagged responses properly.
 *   FIXME. ?! */
int imap_parse_list_response(IMAP_DATA* idata, char **name, int *noselect,
  int *noinferiors, char *delim)
{
  char *s;
  long bytes;
  int rc;

  *name = NULL;

  rc = imap_cmd_step (idata);
  if (rc == IMAP_CMD_OK)
    return 0;
  if (rc != IMAP_CMD_CONTINUE)
    return -1;

  s = imap_next_word (idata->cmd.buf);
  if ((ascii_strncasecmp ("LIST", s, 4) == 0) ||
      (ascii_strncasecmp ("LSUB", s, 4) == 0))
  {
    *noselect = 0;
    *noinferiors = 0;
      
    s = imap_next_word (s); /* flags */
    if (*s == '(')
    {
      char *ep;

      s++;
      ep = s;
      while (*ep && *ep != ')') ep++;
      do
      {
	if (!ascii_strncasecmp (s, "\\NoSelect", 9))
	  *noselect = 1;
	if (!ascii_strncasecmp (s, "\\NoInferiors", 12))
	  *noinferiors = 1;
	/* See draft-gahrns-imap-child-mailbox-?? */
	if (!ascii_strncasecmp (s, "\\HasNoChildren", 14))
	  *noinferiors = 1;
	if (*s != ')')
	  s++;
	while (*s && *s != '\\' && *s != ')') s++;
      } while (s != ep);
    }
    else
      return 0;
    s = imap_next_word (s); /* delim */
    /* Reset the delimiter, this can change */
    if (ascii_strncasecmp (s, "NIL", 3))
    {
      if (s && s[0] == '\"' && s[1] && s[2] == '\"')
	*delim = s[1];
      else if (s && s[0] == '\"' && s[1] && s[1] == '\\' && s[2] && s[3] == '\"')
	*delim = s[2];
    }
    s = imap_next_word (s); /* name */
    if (s && *s == '{')	/* Literal */
    { 
      if (imap_get_literal_count(idata->cmd.buf, &bytes) < 0)
	return -1;
      if (imap_cmd_step (idata) != IMAP_CMD_CONTINUE)
	return -1;
      *name = idata->cmd.buf;
    }
    else
      *name = s;
  }

  return 0;
}

int imap_subscribe (char *path, int subscribe)
{
  CONNECTION *conn;
  IMAP_DATA *idata;
  char buf[LONG_STRING];
  char mbox[LONG_STRING];
  IMAP_MBOX mx;

  if (!mx_is_imap (path) || imap_parse_path (path, &mx))
  {
    mutt_error (_("Bad mailbox name"));
    return -1;
  }
  

  if (!(idata = imap_conn_find (&(mx.account), 0)))
    goto fail;
  
  conn = idata->conn;

  imap_fix_path (idata, mx.mbox, buf, sizeof (buf));
  if (subscribe)
    mutt_message (_("Subscribing to %s..."), buf);
  else
    mutt_message (_("Unsubscribing to %s..."), buf);
  imap_munge_mbox_name (mbox, sizeof(mbox), buf);

  snprintf (buf, sizeof (buf), "%s %s", subscribe ? "SUBSCRIBE" :
    "UNSUBSCRIBE", mbox);

  if (imap_exec (idata, buf, 0) < 0)
    goto fail;

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
  for (mailbox = Incoming; mailbox->next; mailbox = mailbox->next)
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
  
  for (conn = mutt_socket_head (); conn->next; conn = conn->next)
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
  char* list_word = NULL;
  int noselect, noinferiors;
  char delim;
  char completion[LONG_STRING];
  int clen, matchlen = 0;
  int completions = 0;
  IMAP_MBOX mx;

  if (imap_parse_path (path, &mx) || !mx.mbox)
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
  do
  {
    if (imap_parse_list_response(idata, &list_word, &noselect, &noinferiors,
        &delim))
      break;

    if (list_word)
    {
      /* store unquoted */
      imap_unmunge_mbox_name (list_word);

      /* if the folder isn't selectable, append delimiter to force browse
       * to enter it on second tab. */
      if (noselect)
      {
        clen = strlen(list_word);
        list_word[clen++] = delim;
        list_word[clen] = '\0';
      }
      /* copy in first word */
      if (!completions)
      {
        strfcpy (completion, list_word, sizeof(completion));
        matchlen = strlen (completion);
        completions++;
        continue;
      }

      matchlen = longest_common_prefix (completion, list_word, 0, matchlen);
      completions++;
    }
  }
  while (ascii_strncmp(idata->cmd.seq, idata->cmd.buf, SEQLEN));

  if (completions)
  {
    /* reformat output */
    imap_qualify_path (dest, dlen, &mx, completion);
    mutt_pretty_mailbox (dest);

    FREE (&mx.mbox);
    return 0;
  }
  
  return -1;
}
