/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (C) 1996-9 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2000 Brendan Cully <brendan@kublai.com>
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

#include "mutt.h"
#include "mutt_curses.h"
#include "mx.h"
#include "mailbox.h"
#include "globals.h"
#include "sort.h"
#include "browser.h"
#include "imap_private.h"

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
static int imap_check_capabilities (IMAP_DATA *idata);

int imap_create_mailbox (CONTEXT* ctx, char* mailbox)
{
  char buf[LONG_STRING], mbox[LONG_STRING];

  imap_munge_mbox_name (mbox, sizeof (mbox), mailbox);
  snprintf (buf, sizeof (buf), "CREATE %s", mbox);
      
  if (imap_exec (buf, sizeof (buf), CTX_DATA, buf, 0) != 0)
  {
    imap_error ("imap_create_mailbox()", buf);
    return -1;
  }
  return 0;
}

int imap_delete_mailbox (CONTEXT* ctx, char* mailbox)
{
  char buf[LONG_STRING], mbox[LONG_STRING];
  
  imap_quote_string (mbox, sizeof (mbox), mailbox);
  snprintf (buf, sizeof (buf), "DELETE %s", mbox);

  if (imap_exec (buf, sizeof (buf), CTX_DATA, buf, 0) != 0)
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
    tmp = conn;

    if (conn->account.type == M_ACCT_TYPE_IMAP && conn->up)
    {
      mutt_message (_("Closing connection to %s..."), conn->account.host);
      imap_logout ((IMAP_DATA*) conn->data);
      mutt_clear_error ();
      mutt_socket_close (conn);

      mutt_socket_free (tmp);
    }

    conn = conn->next;
  }
}

/* imap_parse_date: date is of the form: DD-MMM-YYYY HH:MM:SS +ZZzz */
time_t imap_parse_date (char *s)
{
  struct tm t;
  time_t tz;

  t.tm_mday = (s[0] == ' '? s[1] - '0' : (s[0] - '0') * 10 + (s[1] - '0'));  
  s += 2;
  if (*s != '-')
    return 0;
  s++;
  t.tm_mon = mutt_check_month (s);
  s += 3;
  if (*s != '-')
    return 0;
  s++;
  t.tm_year = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0') - 1900;
  s += 4;
  if (*s != ' ')
    return 0;
  s++;

  /* time */
  t.tm_hour = (s[0] - '0') * 10 + (s[1] - '0');
  s += 2;
  if (*s != ':')
    return 0;
  s++;
  t.tm_min = (s[0] - '0') * 10 + (s[1] - '0');
  s += 2;
  if (*s != ':')
    return 0;
  s++;
  t.tm_sec = (s[0] - '0') * 10 + (s[1] - '0');
  s += 2;
  if (*s != ' ')
    return 0;
  s++;

  /* timezone */
  tz = ((s[1] - '0') * 10 + (s[2] - '0')) * 3600 +
    ((s[3] - '0') * 10 + (s[4] - '0')) * 60;
  if (s[0] == '+')
    tz = -tz;

  return (mutt_mktime (&t, 0) + tz);
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
      return -1;
    }

    if (r == 1 && c != '\n')
      fputc ('\r', fp);

    if (c == '\r')
    {
      r = 1;
      continue;
    }
    else
      r = 0;
    
    fputc (c, fp);
#ifdef DEBUG
    if (debuglevel >= IMAP_LOG_LTRL)
      fputc (c, debugfile);
#endif
  }

  return 0;
}

/* imap_reopen_mailbox: Reopen an imap mailbox.  This is used when the
 * server sends an EXPUNGE message, indicating that some messages may have
 * been deleted. This is a heavy handed approach, as it reparses all of the
 * headers, but it should guarantee correctness.  Later, we might implement
 * something to actually only remove the messages that are marked
 * EXPUNGE.
 */
int imap_reopen_mailbox (CONTEXT *ctx, int *index_hint)
{
  HEADER **old_hdrs;
  int old_msgcount;
  char buf[LONG_STRING];
  char bufout[LONG_STRING];
  char *pc = NULL;
  int count = 0;
  int msg_mod = 0;
  int n;
  int i, j;
  int index_hint_set;

  ctx->quiet = 1;

  if (Sort != SORT_ORDER)
  {
    short old_sort;

    old_sort = Sort;
    Sort = SORT_ORDER;
    mutt_sort_headers (ctx, 1);
    Sort = old_sort;
  }

  old_hdrs = NULL;
  old_msgcount = 0;

  /* simulate a close */
  hash_destroy (&ctx->id_hash, NULL);
  hash_destroy (&ctx->subj_hash, NULL);
  safe_free ((void **) &ctx->v2r);
  if (ctx->readonly)
  {
    for (i = 0; i < ctx->msgcount; i++)
      mutt_free_header (&(ctx->hdrs[i])); /* nothing to do! */
    safe_free ((void **) &ctx->hdrs);
  }
  else
  {
    /* save the old headers */
    old_msgcount = ctx->msgcount;
    old_hdrs = ctx->hdrs;
    ctx->hdrs = NULL;
  }

  ctx->hdrmax = 0;      /* force allocation of new headers */
  ctx->msgcount = 0;
  ctx->vcount = 0;
  ctx->tagged = 0;
  ctx->deleted = 0;
  ctx->new = 0;
  ctx->unread = 0;
  ctx->flagged = 0;
  ctx->changed = 0;
  ctx->id_hash = hash_create (1031);
  ctx->subj_hash = hash_create (1031);

  mutt_message (_("Reopening mailbox... %s"), CTX_DATA->selected_mailbox);
  imap_munge_mbox_name (buf, sizeof (buf), CTX_DATA->selected_mailbox);
  snprintf (bufout, sizeof (bufout), "STATUS %s (MESSAGES)", buf);

  imap_cmd_start (CTX_DATA, bufout);

  do
  {
    if (mutt_socket_readln (buf, sizeof (buf), CTX_DATA->conn) < 0)
      break;

    if (buf[0] == '*')
    {
      pc = buf + 2;

      if (!mutt_strncasecmp ("STATUS", pc, 6) &&
	  (pc = (char *) mutt_stristr (pc, "MESSAGES")))
      {
	char* pn;
	
	/* skip "MESSAGES" */
	pc += 8;
	SKIPWS (pc);
	pn = pc;

	while (*pc && isdigit (*pc))
	  pc++;
	*pc++ = 0;
	n = atoi (pn);
	count = n;
      }
      else if (imap_handle_untagged (CTX_DATA, buf) != 0)
	return -1;
    }
  }
  while (mutt_strncmp (CTX_DATA->seq, buf, mutt_strlen (CTX_DATA->seq)) != 0);

  if (!imap_code (buf))
  {
    char *s;
    s = imap_next_word (buf); /* skip seq */
    s = imap_next_word (s); /* Skip response */
    mutt_error ("%s", s);
    sleep (1);
    return -1;
  }

  ctx->hdrmax = count;
  ctx->hdrs = safe_malloc (count * sizeof (HEADER *));
  ctx->v2r = safe_malloc (count * sizeof (int));
  ctx->msgcount = 0;
  count = imap_read_headers (ctx, 0, count - 1) + 1;

  index_hint_set = (index_hint == NULL);

  if (!ctx->readonly)
  {
    for (i = 0; i < ctx->msgcount; i++)
    {
      int found = 0;

      /* some messages have been deleted, and new  messages have been
       * appended at the end; the heuristic is that old messages have then
       * "advanced" towards the beginning of the folder, so we begin the
       * search at index "i"
       */
      for (j = i; j < old_msgcount; j++)
      {
	if (old_hdrs[j] == NULL)
	  continue;
	if (mbox_strict_cmp_headers (ctx->hdrs[i], old_hdrs[j]))
	{
	  found = 1;
	  break;
	}
      }
      if (!found)
      {
	for (j = 0; j < i && j < old_msgcount; j++)
	{
	  if (old_hdrs[j] == NULL)
	    continue;
	  if (mbox_strict_cmp_headers (ctx->hdrs[i], old_hdrs[j]))
	  {
	    found = 1;
	    break;
	  }
	}
      }
      if (found)
      {
	/* this is best done here */
	if (!index_hint_set && *index_hint == j)
	  *index_hint = i;

	if (old_hdrs[j]->changed)
	{
	  /* Only update the flags if the old header was changed;
	   * otherwise, the header may have been modified
	   * externally, and we don't want to lose _those_ changes 
	   */
	  
	  mutt_set_flag (ctx, ctx->hdrs[i], M_FLAG, old_hdrs[j]->flagged);
	  mutt_set_flag (ctx, ctx->hdrs[i], M_REPLIED, old_hdrs[j]->replied);
	  mutt_set_flag (ctx, ctx->hdrs[i], M_OLD, old_hdrs[j]->old);
	  mutt_set_flag (ctx, ctx->hdrs[i], M_READ, old_hdrs[j]->read);
	}
	mutt_set_flag (ctx, ctx->hdrs[i], M_DELETE, old_hdrs[j]->deleted);
	mutt_set_flag (ctx, ctx->hdrs[i], M_TAG, old_hdrs[j]->tagged);

	/* we don't need this header any more */
	mutt_free_header (&(old_hdrs[j]));
      }
    }

    /* free the remaining old headers */
    for (j = 0; j < old_msgcount; j++)
    {
      if (old_hdrs[j])
      {
	mutt_free_header (&(old_hdrs[j]));
	msg_mod = 1;
      }
    }
    safe_free ((void **) &old_hdrs);
  }

  ctx->quiet = 0;

  return 0;
}

static int imap_get_delim (IMAP_DATA *idata)
{
  char buf[LONG_STRING];
  char *s;

  /* assume that the delim is /.  If this fails, we're in bigger trouble
   * than getting the delim wrong */
  idata->delim = '/';

  imap_cmd_start (idata, "LIST \"\" \"\"");

  do 
  {
    if (mutt_socket_readln (buf, sizeof (buf), idata->conn) < 0)
      return -1;

    if (buf[0] == '*') 
    {
      s = imap_next_word (buf);
      if (mutt_strncasecmp ("LIST", s, 4) == 0)
      {
	s = imap_next_word (s);
	s = imap_next_word (s);
	if (s && s[0] == '\"' && s[1] && s[2] == '\"')
	  idata->delim = s[1];
	else if (s && s[0] == '\"' && s[1] && s[1] == '\\' && s[2] && s[3] == '\"')
	  idata->delim = s[2];
      }
      else
      {
	if (imap_handle_untagged (idata, buf) != 0)
	  return -1;
      }
    }
  }
  while ((mutt_strncmp (buf, idata->seq, SEQLEN) != 0));

  return 0;
}

/* get rights for folder, let imap_handle_untagged do the rest */
static int imap_check_acl (IMAP_DATA *idata)
{
  char buf[LONG_STRING];
  char mbox[LONG_STRING];

  imap_munge_mbox_name (mbox, sizeof(mbox), idata->selected_mailbox);
  snprintf (buf, sizeof (buf), "MYRIGHTS %s", mbox);
  if (imap_exec (buf, sizeof (buf), idata, buf, 0) != 0)
  {
    imap_error ("imap_check_acl", buf);
    return -1;
  }
  return 0;
}

static int imap_check_capabilities (IMAP_DATA *idata)
{
  char buf[LONG_STRING];

  if (imap_exec (buf, sizeof (buf), idata, "CAPABILITY", 0) != 0)
  {
    imap_error ("imap_check_capabilities", buf);
    return -1;
  }
  if (!(mutt_bit_isset(idata->capabilities,IMAP4)
      ||mutt_bit_isset(idata->capabilities,IMAP4REV1)))
  {
    mutt_error _("This IMAP server is ancient. Mutt does not work with it.");
    sleep (5);	/* pause a moment to let the user see the error */
    return -1;
  }
  return 0;
}

int imap_open_connection (IMAP_DATA* idata)
{
  char buf[LONG_STRING];

  if (mutt_socket_open (idata->conn) < 0)
    return -1;

  idata->state = IMAP_CONNECTED;

  if (mutt_socket_readln (buf, sizeof (buf), idata->conn) < 0)
  {
    mutt_socket_close (idata->conn);
    idata->state = IMAP_DISCONNECTED;

    return -1;
  }

  if (mutt_strncmp ("* OK", buf, 4) == 0)
  {
    if (imap_check_capabilities(idata) != 0 
	|| imap_authenticate (idata) != 0)
    {
      mutt_socket_close (idata->conn);
      idata->state = IMAP_DISCONNECTED;
      return -1;
    }
  }
  else if (mutt_strncmp ("* PREAUTH", buf, 9) == 0)
  {
    if (imap_check_capabilities(idata) != 0)
    {
      mutt_socket_close (idata->conn);
      idata->state = IMAP_DISCONNECTED;
      return -1;
    }
  } 
  else
  {
    imap_error ("imap_open_connection()", buf);
    mutt_socket_close (idata->conn);
    idata->state = IMAP_DISCONNECTED;
    return -1;
  }

  idata->state = IMAP_AUTHENTICATED;

  imap_get_delim (idata);
  return 0;
}

/* imap_get_flags: Make a simple list out of a FLAGS response.
 *   return stream following FLAGS response */
static char* imap_get_flags (LIST** hflags, char* s)
{
  LIST* flags;
  char* flag_word;
  char ctmp;

  /* sanity-check string */
  if (mutt_strncasecmp ("FLAGS", s, 5) != 0)
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
  int n;
  IMAP_MBOX mx;
  
  if (imap_parse_path (ctx->path, &mx))
  {
    mutt_error ("%s is an invalid IMAP path", ctx->path);
    return -1;
  }

  conn = mutt_socket_find (&(mx.account), 0);
  idata = (IMAP_DATA*) conn->data;

  if (!idata || (idata->state != IMAP_AUTHENTICATED))
  {
    if (!idata || (idata->state == IMAP_SELECTED) ||
        (idata->state == IMAP_CONNECTED))
    {
      /* We need to create a new connection, the current one isn't useful */
      idata = safe_calloc (1, sizeof (IMAP_DATA));

      conn = mutt_socket_find (&(mx.account), 1);
      conn->data = idata;
      idata->conn = conn;
    }
    if (imap_open_connection (idata))
      return -1;
  }
  ctx->data = (void *) idata;

  /* Clean up path and replace the one in the ctx */
  imap_fix_path (idata, mx.mbox, buf, sizeof (buf));
  FREE(&(idata->selected_mailbox));
  idata->selected_mailbox = safe_strdup (buf);
  imap_qualify_path (buf, sizeof (buf), &mx, idata->selected_mailbox,
    NULL);

  FREE (&(ctx->path));
  ctx->path = safe_strdup (buf);

  idata->selected_ctx = ctx;

  /* clear status, ACL */
  idata->status = 0;
  memset (idata->rights, 0, (RIGHTSMAX+7)/8);

  mutt_message (_("Selecting %s..."), idata->selected_mailbox);
  imap_munge_mbox_name (buf, sizeof(buf), idata->selected_mailbox);
  snprintf (bufout, sizeof (bufout), "%s %s",
    ctx->readonly ? "EXAMINE" : "SELECT", buf);

  imap_cmd_start (idata, bufout);

  idata->state = IMAP_SELECTED;

  do
  {
    char *pc;
    
    if (mutt_socket_readln (buf, sizeof (buf), conn) < 0)
      break;

    if (buf[0] == '*')
    {
      pc = buf + 2;

      if (isdigit (*pc))
      {
	char *pn = pc;

	while (*pc && isdigit (*pc))
	  pc++;
	*pc++ = '\0';
	n = atoi (pn);
	SKIPWS (pc);
	if (mutt_strncasecmp ("EXISTS", pc, 6) == 0)
	  count = n;
      }
      /* Obtain list of available flags here, may be overridden by a
       * PERMANENTFLAGS tag in the OK response */
      else if (mutt_strncasecmp ("FLAGS", pc, 5) == 0)
      {
        /* don't override PERMANENTFLAGS */
        if (!idata->flags)
        {
          dprint (2, (debugfile, "Getting mailbox FLAGS\n"));
          if ((pc = imap_get_flags (&(idata->flags), pc)) == NULL)
            return -1;
        }
      }
      /* PERMANENTFLAGS are massaged to look like FLAGS, then override FLAGS */
      else if (mutt_strncasecmp ("OK [PERMANENTFLAGS", pc, 18) == 0)
      {
        dprint (2, (debugfile,
          "Getting mailbox PERMANENTFLAGS\n"));
        /* safe to call on NULL */
        mutt_free_list (&(idata->flags));
	/* skip "OK [PERMANENT" so syntax is the same as FLAGS */
        pc += 13;
        if ((pc = imap_get_flags (&(idata->flags), pc)) == NULL)
          return -1;
      }
      else if (imap_handle_untagged (idata, buf) != 0)
	return (-1);
    }
  }
  while (mutt_strncmp (idata->seq, buf, mutt_strlen (idata->seq)) != 0);
  /* check for READ-ONLY notification */
  if (!strncmp (imap_get_qualifier (buf), "[READ-ONLY]", 11))
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

  if (!imap_code (buf))
  {
    char *s;
    s = imap_next_word (buf); /* skip seq */
    s = imap_next_word (s); /* Skip response */
    mutt_error ("%s", s);
    idata->state = IMAP_AUTHENTICATED;
    sleep (1);
    return -1;
  }

  if (mutt_bit_isset (idata->capabilities, ACL))
  {
    if (imap_check_acl (idata))
      return -1;
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
  ctx->hdrs = safe_malloc (count * sizeof (HEADER *));
  ctx->v2r = safe_malloc (count * sizeof (int));
  ctx->msgcount = 0;
  count = imap_read_headers (ctx, 0, count - 1) + 1;

  dprint (1, (debugfile, "imap_open_mailbox(): msgcount is %d\n", ctx->msgcount));
  return 0;
}

/* fast switch mailboxes on the same connection - sync without expunge and
 * SELECT */
int imap_select_mailbox (CONTEXT* ctx, const char* path)
{
  IMAP_DATA* idata;
  char curpath[LONG_STRING];
  IMAP_MBOX mx;

  idata = CTX_DATA;

  strfcpy (curpath, path, sizeof (curpath));
  /* check that the target folder makes sense */
  if (imap_parse_path (curpath, &mx))
    return -1;

  /* and that it's on the same server as the current folder */
  if (!mutt_account_match (&(mx.account), &(idata->conn->account)))
  {
    dprint(2, (debugfile,
      "imap_select_mailbox: source server is not target server\n"));
    return -1;
  }

  if (imap_sync_mailbox (ctx, 0, NULL) < 0)
    return -1;

  idata = CTX_DATA;
  /* now trick imap_open_mailbox into thinking it can just select */
  FREE (&(ctx->path));
  ctx->path = safe_strdup(path);
  idata->state = IMAP_AUTHENTICATED;
  
  return imap_open_mailbox (ctx);
}

int imap_open_mailbox_append (CONTEXT *ctx)
{
  CONNECTION *conn;
  IMAP_DATA *idata;
  char buf[LONG_STRING], mbox[LONG_STRING];
  char mailbox[LONG_STRING];
  int r;
  IMAP_MBOX mx;

  if (imap_parse_path (ctx->path, &mx))
    return -1;

  ctx->magic = M_IMAP;

  conn = mutt_socket_find (&(mx.account), 0);
  idata = (IMAP_DATA*) conn->data;

  if (!idata || (idata->state == IMAP_DISCONNECTED))
  {
    if (!idata)
    {
      /* The current connection is a new connection */
      idata = safe_calloc (1, sizeof (IMAP_DATA));
      conn->data = idata;
      idata->conn = conn;
    }
    if (imap_open_connection (idata))
      return -1;
  }
  ctx->data = (void *) idata;

  /* check mailbox existance */

  imap_fix_path (idata, mx.mbox, mailbox, sizeof (mailbox));

  imap_munge_mbox_name (mbox, sizeof (mbox), mailbox);
				
  if (mutt_bit_isset(idata->capabilities,IMAP4REV1))
  {
    snprintf (buf, sizeof (buf), "STATUS %s (UIDVALIDITY)", mbox);
  }
  else if (mutt_bit_isset(idata->capabilities,STATUS))
  { 
    /* We have no idea what the other guy wants. UW imapd 8.3 wants this
     * (but it does not work if another mailbox is selected) */
    snprintf (buf, sizeof (buf), "STATUS %s (UID-VALIDITY)", mbox);
  }
  else
  {
    /* STATUS not supported */
    mutt_message _("Unable to append to IMAP mailboxes at this server");

    return -1;
  }

  r = imap_exec (buf, sizeof (buf), idata, buf, IMAP_CMD_FAIL_OK);
  if (r == -2)
  {
    /* command failed cause folder doesn't exist */
    snprintf (buf, sizeof (buf), _("Create %s?"), mailbox);
    if (option (OPTCONFIRMCREATE) && mutt_yesorno (buf, 1) < 1)
      return -1;

    if (imap_create_mailbox (ctx, mailbox) < 0)
      return -1;
  }
  else if (r == -1)
    /* Hmm, some other failure */
    return -1;

  return 0;
}

void imap_logout (IMAP_DATA* idata)
{
  char buf[LONG_STRING];

  imap_cmd_start (idata, "LOGOUT");

  do
  {
    if (mutt_socket_readln (buf, sizeof (buf), idata->conn) < 0)
      break;
  }
  while (mutt_strncmp (idata->seq, buf, SEQLEN) != 0);
}

int imap_close_connection (CONTEXT *ctx)
{
  dprint (1, (debugfile, "imap_close_connection(): closing connection\n"));
  /* if the server didn't shut down on us, close the connection gracefully */
  if (CTX_DATA->status != IMAP_BYE)
  {
    mutt_message _("Closing connection to IMAP server...");
    imap_logout (CTX_DATA);
    mutt_clear_error ();
  }
  mutt_socket_close (CTX_DATA->conn);
  CTX_DATA->state = IMAP_DISCONNECTED;
  CTX_DATA->conn->data = NULL;
  return 0;
}

static void imap_set_flag (CONTEXT *ctx, int aclbit, int flag, const char *str, 
  char *flags)
{
  if (mutt_bit_isset (CTX_DATA->rights, aclbit))
    if (flag)
      strcat (flags, str);
}

/* imap_make_msg_set: make an IMAP4rev1 message set out of a set of headers,
 *   given a flag enum to filter on.
 * Params: buf: to write message set into
 *         buflen: length of buffer
 *         ctx: CONTEXT containing header set
 *         flag: enum of flag type on which to filter
 *         changed: include only changed messages in message set
 * Returns: number of messages in message set (0 if no matches) */
int imap_make_msg_set (char* buf, size_t buflen, CONTEXT* ctx, int flag,
  int changed)
{
  HEADER** hdrs;	/* sorted local copy */
  int count = 0;	/* number of messages in message set */
  int match = 0;	/* whether current message matches flag condition */
  int setstart = 0;	/* start of current message range */
  char* tmp;
  int n;
  short oldsort;	/* we clobber reverse, must restore it */

  /* sanity-check */
  if (!buf || buflen < 2)
    return 0;

  *buf = '\0';

  /* make copy of header pointers to sort in natural order */
  hdrs = safe_calloc (ctx->msgcount, sizeof (HEADER*));
  memcpy (hdrs, ctx->hdrs, ctx->msgcount * sizeof (HEADER*));

  oldsort = Sort;
  Sort = SORT_ORDER;
  qsort ((void*) hdrs, ctx->msgcount, sizeof (HEADER*),
    mutt_get_sort_func (SORT_ORDER));
  Sort = oldsort;

  tmp = safe_malloc (buflen);

  for (n = 0; n < ctx->msgcount; n++)
  {
    match = 0;
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
        setstart = n+1;
        if (!buf[0])
          snprintf (buf, buflen, "%u", n+1);
        else
        {
          strncpy (tmp, buf, buflen);
          snprintf (buf, buflen, "%s,%u", tmp, n+1);
        }
      }
      /* tie up if the last message also matches */
      else if (n == ctx->msgcount-1)
      {
        strncpy (tmp, buf, buflen);
        snprintf (buf, buflen, "%s:%u", tmp, n+1);
      }
    }
    else if (setstart)
    {
      if (n > setstart)
      {
        strncpy (tmp, buf, buflen);
        snprintf (buf, buflen, "%s:%u", tmp, n);
      }
      setstart = 0;
    }
  }

  safe_free ((void**) &tmp);
  safe_free ((void**) &hdrs);

  return count;
}

/* update the IMAP server to reflect message changes done within mutt.
 * Arguments
 *   ctx: the current context
 *   expunge: 0 or 1 - do expunge? 
 */

int imap_sync_mailbox (CONTEXT* ctx, int expunge, int *index_hint)
{
  char buf[HUGE_STRING];
  char flags[LONG_STRING];
  char tmp[LONG_STRING];
  int deleted;
  int n;
  int err_continue = M_NO;	/* continue on error? */
  int rc;

  if (CTX_DATA->state != IMAP_SELECTED)
  {
    dprint (2, (debugfile, "imap_sync_mailbox: no mailbox selected\n"));
    return -1;
  }

  imap_allow_reopen (ctx);	/* This function is only called when the calling code
				 * expects the context to be changed.
				 */

  if ((rc = imap_check_mailbox (ctx, index_hint)) != 0)
    return rc;

  /* if we are expunging anyway, we can do deleted messages very quickly... */
  if (expunge && mutt_bit_isset (CTX_DATA->rights, IMAP_ACL_DELETE))
  {
    deleted = imap_make_msg_set (buf, sizeof (buf), ctx, M_DELETE, 1);

    /* if we have a message set, then let's delete */
    if (deleted)
    {
      mutt_message (_("Marking %d messages deleted..."), deleted);
      snprintf (tmp, sizeof (tmp), "STORE %s +FLAGS.SILENT (\\Deleted)", buf);
      if (imap_exec (buf, sizeof (buf), CTX_DATA, tmp, 0) != 0)
        /* continue, let regular store try before giving up */
        dprint(2, (debugfile, "imap_sync_mailbox: fast delete failed\n"));
      else
        /* mark these messages as unchanged so second pass ignores them */
        for (n = 0; n < ctx->msgcount; n++)
          if (ctx->hdrs[n]->deleted && ctx->hdrs[n]->changed)
            ctx->hdrs[n]->changed = 0;
    }
  }

  /* save status changes */
  for (n = 0; n < ctx->msgcount; n++)
  {
    if (ctx->hdrs[n]->changed)
    {
      mutt_message (_("Saving message status flags... [%d/%d]"), n+1, ctx->msgcount);
      
      flags[0] = '\0';
      
      imap_set_flag (ctx, IMAP_ACL_SEEN, ctx->hdrs[n]->read, "\\Seen ",
        flags);
      imap_set_flag (ctx, IMAP_ACL_WRITE, ctx->hdrs[n]->flagged, "\\Flagged ",
        flags);
      imap_set_flag (ctx, IMAP_ACL_WRITE, ctx->hdrs[n]->replied,
        "\\Answered ", flags);
      imap_set_flag (ctx, IMAP_ACL_DELETE, ctx->hdrs[n]->deleted, "\\Deleted ",
        flags);

      /* now make sure we don't lose custom tags */
      if (mutt_bit_isset (CTX_DATA->rights, IMAP_ACL_WRITE))
        imap_add_keywords (flags, ctx->hdrs[n], CTX_DATA->flags);
      
      mutt_remove_trailing_ws (flags);
      
      /* UW-IMAP is OK with null flags, Cyrus isn't. The only solution is to
       * explicitly revoke all system flags (if we have permission) */
      if (!*flags)
      {
        imap_set_flag (ctx, IMAP_ACL_SEEN, 1, "\\Seen ", flags);
        imap_set_flag (ctx, IMAP_ACL_WRITE, 1, "\\Flagged ", flags);
        imap_set_flag (ctx, IMAP_ACL_WRITE, 1, "\\Answered ", flags);
        imap_set_flag (ctx, IMAP_ACL_DELETE, 1, "\\Deleted ", flags);

        mutt_remove_trailing_ws (flags);

        snprintf (buf, sizeof (buf), "STORE %d -FLAGS.SILENT (%s)",
          ctx->hdrs[n]->index + 1, flags);
      }
      else
        snprintf (buf, sizeof (buf), "STORE %d FLAGS.SILENT (%s)",
          ctx->hdrs[n]->index + 1, flags);

      /* after all this it's still possible to have no flags, if you
       * have no ACL rights */
      if (*flags && (imap_exec (buf, sizeof (buf), CTX_DATA, buf, 0) != 0) &&
        (err_continue != M_YES))
      {
        err_continue = imap_continue ("imap_sync_mailbox: STORE failed", buf);
        if (err_continue != M_YES)
          return -1;
      }

      ctx->hdrs[n]->changed = 0;
    }
  }
  ctx->changed = 0;

  if (expunge == 1)
  {
    /* expunge is implicit if closing */
    if (ctx->closing)
    {
      mutt_message _("Closing mailbox...");
      if (imap_exec (buf, sizeof (buf), CTX_DATA, "CLOSE", 0) != 0)
      {
        imap_error ("imap_sync_mailbox: CLOSE failed", buf);
        return -1;
      }
      CTX_DATA->state = IMAP_AUTHENTICATED;
    }
    else if (mutt_bit_isset(CTX_DATA->rights, IMAP_ACL_DELETE))
    {
      mutt_message _("Expunging messages from server...");
      CTX_DATA->status = IMAP_EXPUNGE;
      if (imap_exec (buf, sizeof (buf), CTX_DATA, "EXPUNGE", 0) != 0)
      {
        imap_error ("imap_sync_mailbox: EXPUNGE failed", buf);
        return -1;
      }
      CTX_DATA->status = 0;
    }
  }

  for (n = 0; n < IMAP_CACHE_LEN; n++)
  {
    if (CTX_DATA->cache[n].path)
    {
      unlink (CTX_DATA->cache[n].path);
      safe_free ((void **) &CTX_DATA->cache[n].path);
    }
  }

  return 0;
}

void imap_fastclose_mailbox (CONTEXT *ctx)
{
  int i;

  /* Check to see if the mailbox is actually open */
  if (!ctx->data)
    return;

  CTX_DATA->reopen &= IMAP_REOPEN_ALLOW;

  if ((CTX_DATA->state == IMAP_SELECTED) && (ctx == CTX_DATA->selected_ctx))
    CTX_DATA->state = IMAP_AUTHENTICATED;

  /* free IMAP part of headers */
  for (i = 0; i < ctx->msgcount; i++)
    imap_free_header_data (&(ctx->hdrs[i]->data));

  for (i = 0; i < IMAP_CACHE_LEN; i++)
  {
    if (CTX_DATA->cache[i].path)
    {
      unlink (CTX_DATA->cache[i].path);
      safe_free ((void **) &CTX_DATA->cache[i].path);
    }
  }

#if 0
  /* This is not the right place to logout, actually. There are two dangers:
   * 1. status is set to IMAP_LOGOUT as soon as the user says q, even if she
   *    cancels a bit later.
   * 2. We may get here when closing the $received folder, but before we sync
   *    the spool. So the sync will currently cause an abort. */
  if (CTX_DATA->status == IMAP_BYE || CTX_DATA->status == IMAP_FATAL ||
    CTX_DATA->status == IMAP_LOGOUT)
  {
    imap_close_connection (ctx);
    CTX_DATA->conn->data = NULL;
    safe_free ((void **) &ctx->data);
  }
#endif
}

/* use the NOOP command to poll for new mail
 *
 * return values:
 *	M_REOPENED	mailbox has been reopened
 *	M_NEW_MAIL	new mail has arrived!
 *	0		no change
 *	-1		error
 */
int imap_check_mailbox (CONTEXT *ctx, int *index_hint)
{
  char buf[LONG_STRING];
  static time_t checktime=0;
  time_t t = 0;

  /* 
   * gcc thinks it has to warn about uninitialized use
   * of t.  This is wrong.
   */
  
  if (ImapCheckTimeout)
  { 
    t = time(NULL);
    t -= checktime;
  }

  if ((ImapCheckTimeout && t >= ImapCheckTimeout)
      || ((CTX_DATA->reopen & IMAP_REOPEN_ALLOW) && (CTX_DATA->reopen & ~IMAP_REOPEN_ALLOW)))
  {
    if (ImapCheckTimeout) checktime += t;

    CTX_DATA->check_status = 0;
    if (imap_exec (buf, sizeof (buf), CTX_DATA, "NOOP", 0) != 0)
    {
      imap_error ("imap_check_mailbox()", buf);
      return -1;
    }
    
    if (CTX_DATA->check_status == IMAP_NEW_MAIL)
      return M_NEW_MAIL;
    if (CTX_DATA->check_status == IMAP_REOPENED)
      return M_REOPENED;
  }
  
  return 0;
}

/* returns count of recent messages if new = 1, else count of total messages.
 * (useful for at least postponed function)
 * Question of taste: use RECENT or UNSEEN for new? */
int imap_mailbox_check (char* path, int new)
{
  CONNECTION *conn;
  IMAP_DATA *idata;
  char buf[LONG_STRING];
  char mbox[LONG_STRING];
  char mbox_unquoted[LONG_STRING];
  char *s;
  int msgcount = 0;
  IMAP_MBOX mx;
  
  if (imap_parse_path (path, &mx))
    return -1;

  conn = mutt_socket_find (&(mx.account), 0);
  idata = (IMAP_DATA*) conn->data;

  if (!idata || (idata->state == IMAP_DISCONNECTED))
  {
    /* If passive is selected, then we don't open connections to check
     * for new mail */
    if (option (OPTIMAPPASSIVE))
      return -1;
    if (!idata)
    {
      /* The current connection is a new connection */
      idata = safe_calloc (1, sizeof (IMAP_DATA));
      conn->data = idata;
      idata->conn = conn;
    }
    if (imap_open_connection (idata))
      return -1;
  }

  imap_fix_path (idata, mx.mbox, buf, sizeof (buf));
  /* Update the path, if it fits */
  if (strlen (buf) < strlen (mx.mbox))
      strcpy (mx.mbox, buf);

  imap_munge_mbox_name (mbox, sizeof(mbox), buf);
  strfcpy (mbox_unquoted, buf, sizeof (mbox_unquoted));

  /* The draft IMAP implementor's guide warns againts using the STATUS
   * command on a mailbox that you have selected 
   */

  if (mutt_strcmp (mbox_unquoted, idata->selected_mailbox) == 0
      || (mutt_strcasecmp (mbox_unquoted, "INBOX") == 0
	  && mutt_strcasecmp (mbox_unquoted, idata->selected_mailbox) == 0))
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
  {
    /* Server does not support STATUS, and this is not the current mailbox.
     * There is no lightweight way to check recent arrivals */
      return -1;
  }

  imap_cmd_start (idata, buf);

  do 
  {
    if (mutt_socket_readln (buf, sizeof (buf), conn) < 0)
      return -1;

    if (buf[0] == '*') 
    {
      s = imap_next_word (buf);
      if (mutt_strncasecmp ("STATUS", s, 6) == 0)
      {
	s = imap_next_word (s);
	if (mutt_strncmp (mbox_unquoted, s, mutt_strlen (mbox_unquoted)) == 0)
	{
	  s = imap_next_word (s);
	  s = imap_next_word (s);
	  if (isdigit (*s))
	  {
	    if (*s != '0')
	    {
	      dprint (1, (debugfile, "Mail in %s\n", path));
	      msgcount = atoi(s);
	    }
	  }
	}
      }
      else
      {
	if (conn->data && 
	    imap_handle_untagged (idata, buf) != 0)
	  return -1;
      }
    }
  }
  while ((mutt_strncmp (buf, idata->seq, SEQLEN) != 0));

  imap_cmd_finish (idata);

  return msgcount;
}

int imap_parse_list_response(IMAP_DATA* idata, char *buf, int buflen,
  char **name, int *noselect, int *noinferiors, char *delim)
{
  char *s;
  long bytes;

  *name = NULL;

  if (mutt_socket_readln (buf, buflen, idata->conn) < 0)
    return -1;

  if (buf[0] == '*')
  {
    s = imap_next_word (buf);
    if ((mutt_strncasecmp ("LIST", s, 4) == 0) ||
	(mutt_strncasecmp ("LSUB", s, 4) == 0))
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
	do {
	  if (!strncasecmp (s, "\\NoSelect", 9))
	    *noselect = 1;
	  if (!strncasecmp (s, "\\NoInferiors", 12))
	    *noinferiors = 1;
	  if (*s != ')')
	    s++;
	  while (*s && *s != '\\' && *s != ')') s++;
	} while (s != ep);
      }
      else
	return (0);
      s = imap_next_word (s); /* delim */
      /* Reset the delimiter, this can change */
      if (strncmp (s, "NIL", 3))
      {
	if (s && s[0] == '\"' && s[1] && s[2] == '\"')
	  *delim = s[1];
	else if (s && s[0] == '\"' && s[1] && s[1] == '\\' && s[2] && s[3] == '\"')
	  *delim = s[2];
      }
      s = imap_next_word (s); /* name */
      if (s && *s == '{')	/* Literal */
      { 
	int len;
	
	if (imap_get_literal_count(buf, &bytes) < 0)
	  return -1;
	len = mutt_socket_readln (buf, buflen, idata->conn);
	if (len < 0)
	  return -1;
	*name = buf;
      }
      else
	*name = s;
    }
    else
    {
      if (imap_handle_untagged (idata, buf) != 0)
	return -1;
    }
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

  if (imap_parse_path (path, &mx))
    return -1;

  conn = mutt_socket_find (&(mx.account), 0);
  idata = (IMAP_DATA*) conn->data;

  if (!idata || (idata->state == IMAP_DISCONNECTED))
  {
    if (!idata)
    {
      /* The current connection is a new connection */
      idata = safe_calloc (1, sizeof (IMAP_DATA));
      conn->data = idata;
      idata->conn = conn;
    }
    if (imap_open_connection (idata))
      return -1;
  }

  imap_fix_path (idata, mx.mbox, buf, sizeof (buf));
  if (subscribe)
    mutt_message (_("Subscribing to %s..."), buf);
  else
    mutt_message (_("Unsubscribing to %s..."), buf);
  imap_munge_mbox_name (mbox, sizeof(mbox), buf);

  snprintf (buf, sizeof (buf), "%s %s", subscribe ? "SUBSCRIBE" :
    "UNSUBSCRIBE", mbox);

  if (imap_exec (buf, sizeof (buf), idata, buf, 0) < 0)
    return -1;

  return 0;
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
  int pos = 0;
  IMAP_MBOX mx;

  /* verify passed in path is an IMAP path */
  if (imap_parse_path (path, &mx))
  {
    dprint(2, (debugfile, "imap_complete: bad path %s\n", path));
    return -1;
  }

  conn = mutt_socket_find (&(mx.account), 0);
  idata = (IMAP_DATA*) conn->data;

  /* don't open a new socket just for completion */
  if (!idata)
  {
    dprint (2, (debugfile,
      "imap_complete: refusing to open new connection for %s", path));
    return -1;
  }

  /* reformat path for IMAP list, and append wildcard */
  /* don't use INBOX in place of "" */
  if (mx.mbox[0])
    imap_fix_path (idata, mx.mbox, list, sizeof(list));
  else
    list[0] = '\0';

  /* fire off command */
  snprintf (buf, sizeof(buf), "%s \"\" \"%s%%\"",
    option (OPTIMAPLSUB) ? "LSUB" : "LIST", list);

  imap_cmd_start (idata, buf);

  /* and see what the results are */
  strfcpy (completion, mx.mbox, sizeof(completion));
  do
  {
    if (imap_parse_list_response(idata, buf, sizeof(buf), &list_word,
        &noselect, &noinferiors, &delim))
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

      pos = 0;
      while (pos < matchlen && list_word[pos] &&
          completion[pos] == list_word[pos])
        pos++;
      completion[pos] = '\0';
      matchlen = pos;

      completions++;
    }
  }
  while (mutt_strncmp(idata->seq, buf, SEQLEN));

  if (completions)
  {
    /* reformat output */
    imap_qualify_path (dest, dlen, &mx, completion, NULL);
    mutt_pretty_mailbox (dest);

    return 0;
  }

  return -1;
}
