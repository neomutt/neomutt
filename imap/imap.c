/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (C) 1996-9 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999 Brendan Cully <brendan@kublai.com>
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
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */ 

/* Support for IMAP4rev1, with the occasional nod to IMAP 4. */

#include "mutt.h"
#include "mutt_curses.h"
#include "mx.h"
#include "mailbox.h"
#include "globals.h"
#include "sort.h"
#include "browser.h"
#include "imap.h"
#include "imap_private.h"
#include "imap_socket.h"

#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

static char *Capabilities[] = {"IMAP4", "IMAP4rev1", "STATUS", "ACL", 
  "NAMESPACE", "AUTH=CRAM-MD5", "AUTH=KERBEROS_V4", "AUTH=GSSAPI", 
  "AUTH=LOGIN", "AUTH-LOGIN", "AUTH=PLAIN", "AUTH=SKEY", "IDLE", 
  "LOGIN-REFERRALS", "MAILBOX-REFERRALS", "QUOTA", "SCAN", "SORT", 
  "THREAD=ORDEREDSUBJECT", "UIDPLUS", NULL};

/* imap forward declarations */
static int imap_wordcasecmp(const char *a, const char *b);
static void imap_parse_capabilities (IMAP_DATA *idata, char *s);
static int imap_reopen_mailbox (CONTEXT *ctx, int *index_hint);
static int imap_get_delim (IMAP_DATA *idata, CONNECTION *conn);
static char* imap_get_flags (LIST** hflags, char* s);
static int imap_check_acl (IMAP_DATA *idata);
static int imap_check_capabilities (IMAP_DATA *idata);

void imap_set_logout (CONTEXT *ctx)
{
  if (CTX_DATA)
    CTX_DATA->status = IMAP_LOGOUT;
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

/* imap_read_bytes: read bytes bytes from server into file */
int imap_read_bytes (FILE *fp, CONNECTION *conn, long bytes)
{
  long pos;
  long len;
  char buf[LONG_STRING];

  for (pos = 0; pos < bytes; )
  {
    len = mutt_socket_read_line (buf, sizeof (buf), conn);
    if (len < 0)
      return (-1);
    pos += len;
    fputs (buf, fp);
    fputs ("\n", fp);
  }

  return 0;
}

/* returns 1 if the command result was OK, or 0 if NO or BAD */
int imap_code (const char *s)
{
  s += SEQLEN;
  SKIPWS (s);
  return (mutt_strncasecmp ("OK", s, 2) == 0);
}

/* a is a word, b a string of words */
static int imap_wordcasecmp(const char *a, const char *b)
{
  char tmp[SHORT_STRING];
  char *s = (char *)b;
  int i;

  tmp[SHORT_STRING-1] = 0;
  for(i=0;i < SHORT_STRING-2;i++,s++)
  {
    if (!*s || ISSPACE(*s))
    {
      tmp[i] = 0;
      break;
    }
    tmp[i] = *s;
  }
  tmp[i+1] = 0;
  return mutt_strcasecmp(a, tmp);
    
}

static void imap_parse_capabilities (IMAP_DATA *idata, char *s)
{
  int x;

  while (*s) 
  {
    for (x = 0; x < CAPMAX; x++)
      if (imap_wordcasecmp(Capabilities[x], s) == 0)
      {
	mutt_bit_set (idata->capabilities, x);
	break;
      }
    s = imap_next_word (s);
  }   
}

int imap_handle_untagged (IMAP_DATA *idata, char *s)
{
  char *pn;
  int count;

  s = imap_next_word (s);

  if ((idata->state == IMAP_SELECTED) && isdigit (*s))
  {
    pn = s;
    s = imap_next_word (s);

    /* EXISTS and EXPUNGE are always related to the SELECTED mailbox for the
     * connection, so update that one.
     */
    if (mutt_strncasecmp ("EXISTS", s, 6) == 0)
    {
      /* new mail arrived */
      count = atoi (pn);

      if ( (idata->status != IMAP_EXPUNGE) && 
      	count < idata->selected_ctx->msgcount)
      {
	/* something is wrong because the server reported fewer messages
	 * than we previously saw
	 */
	mutt_error _("Fatal error.  Message count is out of sync!");
	idata->status = IMAP_FATAL;
	mx_fastclose_mailbox (idata->selected_ctx);
	return (-1);
      }
      else
      {
	if (idata->status != IMAP_EXPUNGE)
	  idata->status = IMAP_NEW_MAIL;
	idata->newMailCount = count;
      }
    }
    else if (mutt_strncasecmp ("EXPUNGE", s, 7) == 0)
    {
       idata->status = IMAP_EXPUNGE;
    }
  }
  else if (mutt_strncasecmp ("CAPABILITY", s, 10) == 0)
  {
    /* parse capabilities */
    imap_parse_capabilities (idata, s);
  }
  else if (mutt_strncasecmp ("MYRIGHTS", s, 8) == 0)
  {
    s = imap_next_word (s);
    s = imap_next_word (s);
    while (*s && !isspace(*s))
    {
      switch (*s) 
      {
	case 'l':
	  mutt_bit_set (idata->rights, IMAP_ACL_LOOKUP);
	  break;
	case 'r':
	  mutt_bit_set (idata->rights, IMAP_ACL_READ);
	  break;
	case 's':
	  mutt_bit_set (idata->rights, IMAP_ACL_SEEN);
	  break;
	case 'w':
	  mutt_bit_set (idata->rights, IMAP_ACL_WRITE);
	  break;
	case 'i':
	  mutt_bit_set (idata->rights, IMAP_ACL_INSERT);
	  break;
	case 'p':
	  mutt_bit_set (idata->rights, IMAP_ACL_POST);
	  break;
	case 'c':
	  mutt_bit_set (idata->rights, IMAP_ACL_CREATE);
	  break;
	case 'd':
	  mutt_bit_set (idata->rights, IMAP_ACL_DELETE);
	  break;
	case 'a':
	  mutt_bit_set (idata->rights, IMAP_ACL_ADMIN);
	  break;
      }
      s++;
    }
  }
  else if (mutt_strncasecmp ("BYE", s, 3) == 0)
  {
    /* server shut down our connection */
    s += 3;
    SKIPWS (s);
    mutt_error (s);
    idata->status = IMAP_BYE;
    if (idata->state == IMAP_SELECTED)
      mx_fastclose_mailbox (idata->selected_ctx);
    return (-1);
  }
  else
  {
    dprint (1, (debugfile, "imap_handle_untagged(): unhandled request: %s\n",
		s));
  }

  return 0;
}

/* reopen an imap mailbox.  This is used when the server sends an
 * EXPUNGE message, indicating that some messages may have been deleted.
 * This is a heavy handed approach, as it reparses all of the headers,
 * but it should guarantee correctness.  Later, we might implement
 * something to actually only remove the messages that are marked
 * EXPUNGE.
 */
static int imap_reopen_mailbox (CONTEXT *ctx, int *index_hint)
{
  HEADER **old_hdrs;
  int old_msgcount;
  char buf[LONG_STRING];
  char bufout[LONG_STRING];
  char seq[8];
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
  imap_quote_string (buf, sizeof (buf), CTX_DATA->selected_mailbox);
  imap_make_sequence (seq, sizeof (seq));
  snprintf (bufout, sizeof (bufout), "%s SELECT %s\r\n", seq, buf);
  mutt_socket_write (CTX_DATA->conn, bufout);

  do
  {
    if (mutt_socket_read_line_d (buf, sizeof (buf), CTX_DATA->conn) < 0)
      break;

    if (buf[0] == '*')
    {
      pc = buf + 2;

      if (isdigit (*pc))
      {
	char *pn = pc;

	while (*pc && isdigit (*pc))
	  pc++;
	*pc++ = 0;
	n = atoi (pn);
	SKIPWS (pc);
	if (mutt_strncasecmp ("EXISTS", pc, 6) == 0)
	  count = n;
      }
      else if (imap_handle_untagged (CTX_DATA, buf) != 0)
	return (-1);
    }
  }
  while (mutt_strncmp (seq, buf, mutt_strlen (seq)) != 0);

  if (!imap_code (buf))
  {
    char *s;
    s = imap_next_word (buf); /* skip seq */
    s = imap_next_word (s); /* Skip response */
    CTX_DATA->state = IMAP_AUTHENTICATED;
    mutt_error (s);
    sleep (1);
    return (-1);
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

/*
 * Execute a command, and wait for the response from the server.
 * Also, handle untagged responses
 * If flags == IMAP_OK_FAIL, then the calling procedure can handle a response 
 * failing, this is used for checking for a mailbox on append and login
 * Return 0 on success, -1 on Failure, -2 on OK Failure
 */
int imap_exec (char* buf, size_t buflen, IMAP_DATA* idata, const char* cmd,
  int flags)
{
  char* out;
  int outlen;
  char seq[8];
  int count;

  /* create sequence for command */
  imap_make_sequence (seq, sizeof (seq));
  /* seq, space, cmd, \r\n\0 */
  outlen = strlen (seq) + strlen (cmd) + 4;
  out = (char*) safe_malloc (outlen);
  snprintf (out, outlen, "%s %s\r\n", seq, cmd);

  mutt_socket_write (idata->conn, out);

  safe_free ((void**) &out);

  do
  {
    if (mutt_socket_read_line_d (buf, buflen, idata->conn) < 0)
      return (-1);

    if (buf[0] == '*' && imap_handle_untagged (idata, buf) != 0)
      return (-1);
  }
  while (mutt_strncmp (buf, seq, SEQLEN) != 0);

  if ((idata->state == IMAP_SELECTED) && 
      !idata->selected_ctx->closing && 
      (idata->status == IMAP_NEW_MAIL || 
       idata->status == IMAP_EXPUNGE))
  {

    count = idata->newMailCount;

    if (idata->status == IMAP_NEW_MAIL && count > idata->selected_ctx->msgcount)
    {
      /* read new mail messages */
      dprint (1, (debugfile, "imap_exec(): new mail detected\n"));

      while (count > idata->selected_ctx->hdrmax)
	mx_alloc_memory (idata->selected_ctx);

      count = imap_read_headers (idata->selected_ctx, 
	  idata->selected_ctx->msgcount, count - 1) + 1;
      idata->check_status = IMAP_NEW_MAIL;
    }
    else
    {
      imap_reopen_mailbox (idata->selected_ctx, NULL);
      idata->check_status = IMAP_REOPENED;
    }

    idata->status = 0;

    mutt_clear_error ();
  }

  if (!imap_code (buf))
  {
    char *pc;

    if (flags == IMAP_OK_FAIL)
      return (-2);
    dprint (1, (debugfile, "imap_exec(): command failed: %s\n", buf));
    pc = buf + SEQLEN;
    SKIPWS (pc);
    pc = imap_next_word (pc);
    mutt_error (pc);
    sleep (1);
    return (-1);
  }

  return 0;
}

static int imap_get_delim (IMAP_DATA *idata, CONNECTION *conn)
{
  char buf[LONG_STRING];
  char seq[8];
  char *s;

  /* assume that the delim is /.  If this fails, we're in bigger trouble
   * than getting the delim wrong */
  idata->delim = '/';

  imap_make_sequence (seq, sizeof (seq));
  snprintf (buf, sizeof (buf), "%s LIST \"\" \"\"\r\n", seq);

  mutt_socket_write (conn, buf);

  do 
  {
    if (mutt_socket_read_line_d (buf, sizeof (buf), conn) < 0)
    {
      return (-1);
    }

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
	if (conn->data && 
	    imap_handle_untagged (idata, buf) != 0)
	  return (-1);
      }
    }
  }
  while ((mutt_strncmp (buf, seq, SEQLEN) != 0));
  return 0;
}

/* get rights for folder, let imap_handle_untagged do the rest */
static int imap_check_acl (IMAP_DATA *idata)
{
  char buf[LONG_STRING];
  char mbox[LONG_STRING];

  imap_quote_string (mbox, sizeof(mbox), idata->selected_mailbox);
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

int imap_open_connection (IMAP_DATA *idata, CONNECTION *conn)
{
  char buf[LONG_STRING];

  if (mutt_socket_open_connection (conn) < 0)
  {
    return (-1);
  }

  idata->state = IMAP_CONNECTED;

  if (mutt_socket_read_line_d (buf, sizeof (buf), conn) < 0)
  {
    close (conn->fd);
    idata->state = IMAP_DISCONNECTED;
    return (-1);
  }

  if (mutt_strncmp ("* OK", buf, 4) == 0)
  {
    if (imap_check_capabilities(idata) != 0 
	|| imap_authenticate (idata, conn) != 0)
    {
      close (conn->fd);
      idata->state = IMAP_DISCONNECTED;
      return (-1);
    }
  }
  else if (mutt_strncmp ("* PREAUTH", buf, 9) == 0)
  {
    if (imap_check_capabilities(idata) != 0)
    {
      close (conn->fd);
      idata->state = IMAP_DISCONNECTED;
      return (-1);
    }
  } 
  else
  {
    imap_error ("imap_open_connection()", buf);
    close (conn->fd);
    idata->state = IMAP_DISCONNECTED;
    return (-1);
  }

  idata->state = IMAP_AUTHENTICATED;

  imap_get_delim (idata, conn);
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

int imap_open_mailbox (CONTEXT *ctx)
{
  CONNECTION *conn;
  IMAP_DATA *idata;
  char buf[LONG_STRING];
  char bufout[LONG_STRING];
  char host[SHORT_STRING];
  char seq[16];
  char *pc = NULL;
  int count = 0;
  int n;
  int port;

  if (imap_parse_path (ctx->path, host, sizeof (host), &port, &pc))
    return (-1);

  conn = mutt_socket_select_connection (host, port, 0);
  idata = CONN_DATA;

  if (!idata || (idata->state != IMAP_AUTHENTICATED))
  {
    if (!idata || (idata->state == IMAP_SELECTED) ||
        (idata->state == IMAP_CONNECTED))
    {
      /* We need to create a new connection, the current one isn't useful */
      idata = safe_calloc (1, sizeof (IMAP_DATA));

      conn = mutt_socket_select_connection (host, port, M_NEW_SOCKET);
      conn->data = idata;
      idata->conn = conn;
    }
    if (imap_open_connection (idata, conn))
      return (-1);
  }
  ctx->data = (void *) idata;

  /* Clean up path and replace the one in the ctx */
  imap_fix_path (idata, pc, buf, sizeof (buf));
  FREE(&(idata->selected_mailbox));
  idata->selected_mailbox = safe_strdup (buf);
  imap_qualify_path (buf, sizeof (buf), host, port, idata->selected_mailbox,
    NULL);

  FREE (&(ctx->path));
  ctx->path = safe_strdup (buf);

  idata->selected_ctx = ctx;

  /* clear ACL */
  memset (idata->rights, 0, (RIGHTSMAX+7)/8);

  mutt_message (_("Selecting %s..."), idata->selected_mailbox);
  imap_quote_string (buf, sizeof(buf), idata->selected_mailbox);
  imap_make_sequence (seq, sizeof (seq));
  snprintf (bufout, sizeof (bufout), "%s SELECT %s\r\n", seq, buf);
  mutt_socket_write (conn, bufout);

  idata->state = IMAP_SELECTED;

  do
  {
    if (mutt_socket_read_line_d (buf, sizeof (buf), conn) < 0)
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
  while (mutt_strncmp (seq, buf, mutt_strlen (seq)) != 0);

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

  if (!imap_code (buf))
  {
    char *s;
    s = imap_next_word (buf); /* skip seq */
    s = imap_next_word (s); /* Skip response */
    mutt_error (s);
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
  CONNECTION* conn;
  char host[SHORT_STRING];
  int port;
  char curpath[LONG_STRING];
  char* mbox = NULL;

  strfcpy (curpath, path, sizeof (curpath));
  /* check that the target folder makes sense */
  if (imap_parse_path (curpath, host, sizeof (host), &port, &mbox))
    return -1;

  /* and that it's on the same server as the current folder */
  conn = mutt_socket_select_connection (host, port, 0);
  if (!CTX_DATA || !CONN_DATA || (CTX_DATA->conn != CONN_DATA->conn))
  {
    dprint(2, (debugfile,
      "imap_select_mailbox: source server is not target server\n"));
    return -1;
  }

  if (imap_sync_mailbox (ctx, 0) < 0)
    return -1;

  idata = CTX_DATA;
  /* now trick imap_open_mailbox into thinking it can just select */
  FREE (&(ctx->path));
  ctx->path = safe_strdup(path);
  idata->state = IMAP_AUTHENTICATED;
  
  return imap_open_mailbox (ctx);
}

int imap_create_mailbox (IMAP_DATA* idata, char* mailbox)
{
  char buf[LONG_STRING], mbox[LONG_STRING];

  imap_quote_string (mbox, sizeof (mbox), mailbox);
  snprintf (buf, sizeof (buf), "CREATE %s", mbox);
      
  if (imap_exec (buf, sizeof (buf), idata, buf, 0) != 0)
  {
    imap_error ("imap_create_mailbox()", buf);
    return (-1);
  }
  return 0;
}

int imap_open_mailbox_append (CONTEXT *ctx)
{
  CONNECTION *conn;
  IMAP_DATA *idata;
  char host[SHORT_STRING];
  char buf[LONG_STRING], mbox[LONG_STRING];
  char mailbox[LONG_STRING];
  char *pc;
  int r;
  int port;

  if (imap_parse_path (ctx->path, host, sizeof (host), &port, &pc))
    return (-1);

  ctx->magic = M_IMAP;

  conn = mutt_socket_select_connection (host, port, 0);
  idata = CONN_DATA;

  if (!idata || (idata->state == IMAP_DISCONNECTED))
  {
    if (!idata)
    {
      /* The current connection is a new connection */
      idata = safe_calloc (1, sizeof (IMAP_DATA));
      conn->data = idata;
      idata->conn = conn;
    }
    if (imap_open_connection (idata, conn))
      return (-1);
  }
  ctx->data = (void *) idata;

  /* check mailbox existance */

  imap_fix_path (idata, pc, mailbox, sizeof (mailbox));

  imap_quote_string (mbox, sizeof (mbox), mailbox);
				
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
    /* STATUS not supported
     * The thing to do seems to be:
     *  - Open a *new* IMAP session, select, and then close it. Report the
     * error if the mailbox did not exist. */
    mutt_message _("Unable to append to IMAP mailboxes at this server");
    return -1;
  }

  r = imap_exec (buf, sizeof (buf), idata, buf, IMAP_OK_FAIL);
  if (r == -2)
  {
    /* command failed cause folder doesn't exist */
    if (option (OPTCONFIRMCREATE))
    {
      snprintf (buf, sizeof (buf), _("Create %s?"), mailbox);
      if (mutt_yesorno (buf, 1) < 1)
      {
	return (-1);
      }
      if (imap_create_mailbox (idata, mailbox) < 0)
      {
	return (-1);
      }
    }
  }
  else if (r == -1)
  {
    /* Hmm, some other failure */
    return -1;
  }
  return 0;
}

int imap_close_connection (CONTEXT *ctx)
{
  char buf[LONG_STRING];
  char seq[8];

  dprint (1, (debugfile, "imap_close_connection(): closing connection\n"));
  /* if the server didn't shut down on us, close the connection gracefully */
  if (CTX_DATA->status != IMAP_BYE)
  {
    mutt_message _("Closing connection to IMAP server...");
    imap_make_sequence (seq, sizeof (seq));
    snprintf (buf, sizeof (buf), "%s LOGOUT\r\n", seq);
    mutt_socket_write (CTX_DATA->conn, buf);
    do
    {
      if (mutt_socket_read_line_d (buf, sizeof (buf), CTX_DATA->conn) < 0)
	break;
    }
    while (mutt_strncmp (seq, buf, SEQLEN) != 0);
    mutt_clear_error ();
  }
  close (CTX_DATA->conn->fd);
  CTX_DATA->state = IMAP_DISCONNECTED;
  CTX_DATA->conn->uses = 0;
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
 *   expunge: 0 or 1 - do expunge? */
int imap_sync_mailbox (CONTEXT* ctx, int expunge)
{
  char buf[LONG_STRING];
  char flags[LONG_STRING];
  char tmp[LONG_STRING];
  int deleted;
  int n;

  if (CTX_DATA->state != IMAP_SELECTED)
  {
    dprint (2, (debugfile, "imap_sync_mailbox: no mailbox selected\n"));
    return -1;
  }
  
  /* if we are expunging anyway, we can do deleted messages very quickly... */
  if (expunge && mutt_bit_isset (CTX_DATA->rights, IMAP_ACL_DELETE))
  {
    deleted = imap_make_msg_set (buf, sizeof (buf), ctx, M_DELETE, 1);

    /* if we have a message set, then let's delete */
    if (deleted)
    {
      snprintf (tmp, sizeof (tmp), _("Marking %d messages for deletion..."),
        deleted);
      mutt_message (tmp);
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
      snprintf (buf, sizeof (buf), _("Saving message status flags... [%d/%d]"),
		n+1, ctx->msgcount);
      mutt_message (buf);
      
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
      if (*flags && imap_exec (buf, sizeof (buf), CTX_DATA, buf, 0) != 0)
      {
        imap_error ("imap_sync_mailbox: STORE failed", buf);
        /* give up on this message if we pass here again */
        ctx->hdrs[n]->changed = 0;
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
  if (CTX_DATA->status == IMAP_BYE || CTX_DATA->status == IMAP_FATAL ||
    CTX_DATA->status == IMAP_LOGOUT)
  {
    imap_close_connection (ctx);
    CTX_DATA->conn->data = NULL;
    safe_free ((void **) &ctx->data);
  }
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

  if (ImapCheckTimeout)
  {
    time_t k=time(NULL);
    if (k-checktime < ImapCheckTimeout)
      return 0;
    checktime=k;
  }

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
  return 0;
}

/* returns count of recent messages if new = 1, else count of total messages.
 * (useful for at least postponed function)
 * Question of taste: use RECENT or UNSEEN for new? */
int imap_mailbox_check (char *path, int new)
{
  CONNECTION *conn;
  IMAP_DATA *idata;
  char host[SHORT_STRING];
  char buf[LONG_STRING];
  char mbox[LONG_STRING];
  char mbox_unquoted[LONG_STRING];
  char seq[8];
  char *s;
  char *pc;
  int msgcount = 0;
  int port;

  if (imap_parse_path (path, host, sizeof (host), &port, &pc))
    return -1;

  conn = mutt_socket_select_connection (host, port, 0);
  idata = CONN_DATA;

  if (!idata || (idata->state == IMAP_DISCONNECTED))
  {
    /* If passive is selected, then we don't open connections to check
     * for new mail */
    if (option (OPTIMAPPASSIVE))
      return (-1);
    if (!idata)
    {
      /* The current connection is a new connection */
      idata = safe_calloc (1, sizeof (IMAP_DATA));
      conn->data = idata;
      idata->conn = conn;
    }
    if (imap_open_connection (idata, conn))
      return (-1);
  }

  imap_fix_path (idata, pc, buf, sizeof (buf));
  /* Update the path, if it fits */
  if (strlen (buf) < strlen (pc))
      strcpy (pc, buf);

  imap_make_sequence (seq, sizeof (seq));		
  imap_quote_string (mbox, sizeof(mbox), buf);
  strfcpy (mbox_unquoted, buf, sizeof (mbox_unquoted));

  /* The draft IMAP implementor's guide warns againts using the STATUS
   * command on a mailbox that you have selected 
   */

  if (mutt_strcmp (mbox_unquoted, idata->selected_mailbox) == 0
      || (mutt_strcasecmp (mbox_unquoted, "INBOX") == 0
	  && mutt_strcasecmp (mbox_unquoted, idata->selected_mailbox) == 0))
  {
    snprintf (buf, sizeof (buf), "%s NOOP\r\n", seq);
  }
  else if (mutt_bit_isset(idata->capabilities,IMAP4REV1) ||
	   mutt_bit_isset(idata->capabilities,STATUS))
  {				
    snprintf (buf, sizeof (buf), "%s STATUS %s (%s)\r\n", seq, mbox,
      new ? "RECENT" : "MESSAGES");
  }
  else
  {
    /* Server does not support STATUS, and this is not the current mailbox.
     * There is no lightweight way to check recent arrivals */
      return (-1);
  }

  mutt_socket_write (conn, buf);

  do 
  {
    if (mutt_socket_read_line_d (buf, sizeof (buf), conn) < 0)
    {
      return (-1);
    }

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
	  return (-1);
      }
    }
  }
  while ((mutt_strncmp (buf, seq, SEQLEN) != 0));

  conn->uses--;

  return msgcount;
}

int imap_parse_list_response(CONNECTION *conn, char *buf, int buflen,
  char **name, int *noselect, int *noinferiors, char *delim)
{
  IMAP_DATA *idata = CONN_DATA;
  char *s;
  long bytes;

  *name = NULL;
  if (mutt_socket_read_line_d (buf, buflen, conn) < 0)
  {
    return (-1);
  }

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
	  return (-1);
	len = mutt_socket_read_line (buf, buflen, conn);
	if (len < 0)
	  return (-1);
	*name = buf;
      }
      else
	*name = s;
    }
    else
    {
      if (imap_handle_untagged (idata, buf) != 0)
	return (-1);
    }
  }
  return (0);
}

int imap_subscribe (char *path, int subscribe)
{
  CONNECTION *conn;
  IMAP_DATA *idata;
  char buf[LONG_STRING];
  char mbox[LONG_STRING];
  char host[SHORT_STRING];
  char *ipath = NULL;
  int port;

  if (imap_parse_path (path, host, sizeof (host), &port, &ipath))
    return (-1);

  conn = mutt_socket_select_connection (host, port, 0);
  idata = CONN_DATA;

  if (!idata || (idata->state == IMAP_DISCONNECTED))
  {
    if (!idata)
    {
      /* The current connection is a new connection */
      idata = safe_calloc (1, sizeof (IMAP_DATA));
      conn->data = idata;
      idata->conn = conn;
    }
    if (imap_open_connection (idata, conn))
      return -1;
  }

  imap_fix_path (idata, ipath, buf, sizeof (buf));
  if (subscribe)
    mutt_message (_("Subscribing to %s..."), buf);
  else
    mutt_message (_("Unsubscribing to %s..."), buf);
  imap_quote_string (mbox, sizeof(mbox), buf);

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
  char host[SHORT_STRING];
  int port;
  char list[LONG_STRING];
  char buf[LONG_STRING];
  char seq[16];
  char* mbox = NULL;
  char* list_word = NULL;
  int noselect, noinferiors;
  char delim;
  char completion[LONG_STRING];
  int clen, matchlen = 0;
  int completions = 0;
  int pos = 0;

  /* verify passed in path is an IMAP path */
  if (imap_parse_path (path, host, sizeof(host), &port, &mbox))
  {
    dprint(2, (debugfile, "imap_complete: bad path %s\n", path));
    return -1;
  }

  conn = mutt_socket_select_connection (host, port, 0);
  idata = CONN_DATA;

  /* don't open a new socket just for completion */
  if (!idata)
  {
    dprint (2, (debugfile,
      "imap_complete: refusing to open new connection for %s", path));
    return -1;
  }

  /* reformat path for IMAP list, and append wildcard */
  /* don't use INBOX in place of "" */
  if (mbox[0])
    imap_fix_path (idata, mbox, list, sizeof(list));
  else
    list[0] = '\0';

  /* fire off command */
  imap_make_sequence (seq, sizeof(seq));
  snprintf (buf, sizeof(buf), "%s %s \"\" \"%s%%\"\r\n", seq, 
    option (OPTIMAPLSUB) ? "LSUB" : "LIST", list);
  mutt_socket_write (conn, buf);

  /* and see what the results are */
  strfcpy (completion, mbox, sizeof(completion));
  do
  {
    if (imap_parse_list_response(conn, buf, sizeof(buf), &list_word,
        &noselect, &noinferiors, &delim))
      break;

    if (list_word)
    {
      /* store unquoted */
      imap_unquote_string (list_word);

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
  while (mutt_strncmp(seq, buf, strlen(seq)));

  if (completions)
  {
    /* reformat output */
    imap_qualify_path (dest, dlen, host, port, completion, NULL);
    mutt_pretty_mailbox (dest);

    return 0;
  }

  return -1;
}
