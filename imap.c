static const char rcsid[]="$Id$";
/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
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

#include "mutt.h"
#include "mutt_curses.h"
#include "mx.h"
#include "mailbox.h"
#include "globals.h"
#include "mutt_socket.h"
#include "sort.h"

#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

/* Minimal support for IMAP 4rev1 */

#define IMAP_PORT 143

#define SEQLEN 5

/* number of entries in the hash table */
#define IMAP_CACHE_LEN 10

enum
{
  IMAP_FATAL = 1,
  IMAP_NEW_MAIL,
  IMAP_EXPUNGE,
  IMAP_BYE,
  IMAP_OK_FAIL
};

typedef struct
{
  unsigned int index;
  char *path;
} IMAP_CACHE;

typedef struct
{
  short status;
  unsigned short newMailCount;
  char *mailbox;
  short xxx;
  IMAP_CACHE cache[IMAP_CACHE_LEN];
  CONNECTION *conn;
} IMAP_DATA;

#define CTX_DATA ((IMAP_DATA *) ctx->data)

/* Linked list to hold header information while downloading message
 * headers
 */
typedef struct imap_header_info
{
  unsigned int read : 1;
  unsigned int old : 1;
  unsigned int deleted : 1;
  unsigned int flagged : 1;
  unsigned int replied : 1;
  unsigned int changed : 1;
  unsigned int number;

  time_t received;
  long content_length;
  struct imap_header_info *next;
} IMAP_HEADER_INFO;


static void imap_make_sequence (char *buf, size_t buflen)
{
  static int sequence = 0;
  
  snprintf (buf, buflen, "a%04d", sequence++);
}


static void imap_error (const char *where, const char *msg)
{
  mutt_error (_("imap_error(): unexpected response in %s: %s\n"), where, msg);
}

/* date is of the form: DD-MMM-YYYY HH:MM:SS +ZZzz */
static time_t imap_parse_date (char *s)
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

static int imap_parse_fetch (IMAP_HEADER_INFO *h, char *s)
{
  char tmp[SHORT_STRING];
  char *ptmp;
  int state = 0;

  if (!s)
    return (-1);

  h->read = 0;
  h->old = 0;

  while (*s)
  {
    SKIPWS (s);

    switch (state)
    {
      case 0:
	if (strncasecmp ("FLAGS", s, 5) == 0)
	{
	  s += 5;
	  SKIPWS (s);
	  if (*s != '(')
	  {
	    dprint (1, (debugfile, "imap_parse_fetch(): bogus FLAGS entry: %s\n", s));
	    return (-1); /* parse error */
	  }
	  /* we're about to get a new set of headers, so clear the old ones. */
	  h->deleted=0; h->flagged=0;
	  h->replied=0; h->read=0;
	  h->changed=0;
	  s++;
	  state = 1;
	}
	else if (strncasecmp ("INTERNALDATE", s, 12) == 0)
	{
	  s += 12;
	  SKIPWS (s);
	  if (*s != '\"')
	  {
	    dprint (1, (debugfile, "imap_parse_fetch(): bogus INTERNALDATE entry: %s\n", s));
	    return (-1);
	  }
	  s++;
	  ptmp = tmp;
	  while (*s && *s != '\"')
	    *ptmp++ = *s++;
	  if (*s != '\"')
	    return (-1);
	  s++; /* skip past the trailing " */
	  *ptmp = 0;
	  h->received = imap_parse_date (tmp);
	}
	else if (strncasecmp ("RFC822.SIZE", s, 11) == 0)
	{
	  s += 11;
	  SKIPWS (s);
	  ptmp = tmp;
	  while (isdigit (*s))
	    *ptmp++ = *s++;
	  *ptmp = 0;
	  h->content_length += atoi (tmp);
	}
	else if (*s == ')')
	  s++; /* end of request */
	else if (*s)
	{
	  /* got something i don't understand */
	  imap_error ("imap_parse_fetch()", s);
	  return (-1);
	}
	break;
      case 1: /* flags */
	if (*s == ')')
	{
	  s++;
	  state = 0;
	}
	else if (strncasecmp ("\\deleted", s, 8) == 0)
	{
	  s += 8;
	  h->deleted = 1;
	}
	else if (strncasecmp ("\\flagged", s, 8) == 0)
	{
	  s += 8;
	  h->flagged = 1;
	}
	else if (strncasecmp ("\\answered", s, 9) == 0)
	{
	  s += 9;
	  h->replied = 1;
	}
	else if (strncasecmp ("\\seen", s, 5) == 0)
	{
	  s += 5;
	  h->read = 1;
	}
	else
	{
	  while (*s && !ISSPACE (*s) && *s != ')')
	    s++;
	}
	break;
    }
  }
  return 0;
}

static void imap_quote_string (char *dest, size_t slen, const char *src)
{
  char quote[] = "\"\\", *pt;
  const char *s;
  size_t len = slen;

  pt = dest;
  s  = src;

  *pt++ = '"';
  /* save room for trailing quote-char */
  len -= 2;
  
  for (; *s && len; s++)
  {
    if (strchr (quote, *s))
    {
      len -= 2;
      if (!len)
	break;
      *pt++ = '\\';
      *pt++ = *s;
    }
    else
    {
      *pt++ = *s;
      len--;
    }
  }
  *pt++ = '"';
  *pt = 0;
}

static int imap_read_bytes (FILE *fp, CONNECTION *conn, long bytes)
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
static int imap_code (const char *s)
{
  s += SEQLEN;
  SKIPWS (s);
  return (strncasecmp ("OK", s, 2) == 0);
}

static char *imap_next_word (char *s)
{
  while (*s && !ISSPACE (*s))
    s++;
  SKIPWS (s);
  return s;
}

static int imap_handle_untagged (CONTEXT *ctx, char *s)
{
  char *pn;
  int count;

  s = imap_next_word (s);

  if (isdigit (*s))
  {
    pn = s;
    s = imap_next_word (s);

    if (strncasecmp ("EXISTS", s, 6) == 0)
    {
      /* new mail arrived */
      count = atoi (pn);

      if ( (CTX_DATA->status != IMAP_EXPUNGE) && 
      	count < ctx->msgcount)
      {
	/* something is wrong because the server reported fewer messages
	 * than we previously saw
	 */
	mutt_error _("Fatal error.  Message count is out of sync!");
	CTX_DATA->status = IMAP_FATAL;
	mx_fastclose_mailbox (ctx);
	return (-1);
      }
      else
      {
	if (CTX_DATA->status != IMAP_EXPUNGE)
	  CTX_DATA->status = IMAP_NEW_MAIL;
	CTX_DATA->newMailCount = count;
      }
    }
    else if (strncasecmp ("EXPUNGE", s, 7) == 0)
    {
       CTX_DATA->status = IMAP_EXPUNGE;
    }
  }
  else if (strncasecmp ("BYE", s, 3) == 0)
  {
    /* server shut down our connection */
    s += 3;
    SKIPWS (s);
    mutt_error (s);
    CTX_DATA->status = IMAP_BYE;
    mx_fastclose_mailbox (ctx);
    return (-1);
  }
  else
  {
    dprint (1, (debugfile, "imap_unhandle_untagged(): unhandled request: %s\n",
		s));
  }

  return 0;
}

/*
 * Changed to read many headers instead of just one. It will return the
 * msgno of the last message read. It will return a value other than
 * msgend if mail comes in while downloading headers (in theory).
 */
static int imap_read_headers (CONTEXT *ctx, int msgbegin, int msgend)
{
  char buf[LONG_STRING],fetchbuf[LONG_STRING];
  FILE *fp;
  char tempfile[_POSIX_PATH_MAX];
  char seq[8];
  char *pc,*fpc,*hdr;
  char *pn;
  long ploc;
  long bytes = 0;
  int msgno,fetchlast;
  IMAP_HEADER_INFO *h0,*h,*htemp;

  fetchlast = 0;

  /*
   * We now download all of the headers into one file. This should be
   * faster on most systems.
   */
  mutt_mktemp (tempfile);
  if (!(fp = safe_fopen (tempfile, "w+")))
  {
    return (-1);
  }

  h0=safe_malloc(sizeof(IMAP_HEADER_INFO));
  h=h0;
  for (msgno=msgbegin; msgno <= msgend ; msgno++)
  {
    snprintf (buf, sizeof (buf), _("Fetching message headers... [%d/%d]"), 
      msgno + 1, msgend + 1);
    mutt_message (buf);

    if (msgno + 1 > fetchlast)
    {
      imap_make_sequence (seq, sizeof (seq));
      /*
       * Make one request for everything. This makes fetching headers an
       * order of magnitude faster if you have a large mailbox.
       *
       * If we get more messages while doing this, we make another
       * request for all the new messages.
       */
      snprintf (buf, sizeof (buf), "%s FETCH %d:%d (FLAGS INTERNALDATE RFC822.SIZE BODY.PEEK[HEADER.FIELDS (DATE FROM SUBJECT TO CC MESSAGE-ID REFERENCES CONTENT-TYPE IN-REPLY-TO REPLY-TO)])\r\n", seq, msgno + 1, msgend + 1);
      mutt_socket_write (CTX_DATA->conn, buf);
      fetchlast = msgend + 1;
    }

    do
    {
      if (mutt_socket_read_line_d (buf, sizeof (buf), CTX_DATA->conn) < 0)
      {
        return (-1);
      }

      if (buf[0] == '*')
      {
        pc = buf;
        pc = imap_next_word (pc);
	h->number = atoi (pc);
	dprint (1, (debugfile, "fetching message %d\n", h->number));
        pc = imap_next_word (pc);
        if (strncasecmp ("FETCH", pc, 5) == 0)
        {
          if (!(pc = strchr (pc, '(')))
          {
            imap_error ("imap_read_headers()", buf);
            return (-1);
          }
          pc++;
          fpc=fetchbuf;
          while (*pc != '\0' && *pc != ')')
          {
            hdr=strstr(pc,"BODY");
            strncpy(fpc,pc,hdr-pc);
            fpc += hdr-pc;
            *fpc = '\0';
            pc=hdr;
            /* get some number of bytes */
            if (!(pc = strchr (pc, '{')))
            {
              imap_error ("imap_read_headers()", buf);
              return (-1);
            }
            pc++;
            pn = pc;
            while (isdigit (*pc))
              pc++;
            *pc = 0;
            bytes = atoi (pn);

            imap_read_bytes (fp, CTX_DATA->conn, bytes);
            if (mutt_socket_read_line_d (buf, sizeof (buf), CTX_DATA->conn) < 0)
            {
              return (-1);
            }
            pc = buf;
          }
        }
        else if (imap_handle_untagged (ctx, buf) != 0)
          return (-1);
      }
    }
    while ((msgno + 1) >= fetchlast && strncmp (seq, buf, SEQLEN) != 0);

    h->content_length = -bytes;
    if (imap_parse_fetch (h, fetchbuf) == -1)
      return (-1);

    /* subtract the header length; the total message size will be
       added to this */

    /* in case we get new mail while fetching the headers */
    if (((IMAP_DATA *) ctx->data)->status == IMAP_NEW_MAIL)
    {
      msgend = ((IMAP_DATA *) ctx->data)->newMailCount - 1;
      while ((msgend + 1) > ctx->hdrmax)
        mx_alloc_memory (ctx);
      ((IMAP_DATA *) ctx->data)->status = 0;
    }

    h->next=safe_malloc(sizeof(IMAP_HEADER_INFO));
    h=h->next;
  }

  rewind(fp);
  h=h0;

  /*
   * Now that we have all the header information, we can tell mutt about
   * it.
   */
  ploc=0;
  for (msgno = msgbegin; msgno <= msgend;msgno++)
  {
    ctx->hdrs[ctx->msgcount] = mutt_new_header ();
    ctx->hdrs[ctx->msgcount]->index = ctx->msgcount;

    ctx->hdrs[msgno]->env = mutt_read_rfc822_header (fp, ctx->hdrs[msgno], 0);
    ploc=ftell(fp);
    ctx->hdrs[msgno]->read = h->read;
    ctx->hdrs[msgno]->old = h->old;
    ctx->hdrs[msgno]->deleted = h->deleted;
    ctx->hdrs[msgno]->flagged = h->flagged;
    ctx->hdrs[msgno]->replied = h->replied;
    ctx->hdrs[msgno]->changed = h->changed;
    ctx->hdrs[msgno]->received = h->received;
    ctx->hdrs[msgno]->content->length = h->content_length;

    mx_update_context(ctx); /* increments ->msgcount */

    htemp=h;
    h=h->next;
    safe_free((void **) &htemp);
  }
  fclose(fp);
  unlink(tempfile);

  return (msgend);
}

/* reopen an imap mailbox.  This is used when the server sends an
 * EXPUNGE message, indicating that some messages may have been deleted.
 * This is a heavy handed approach, as it reparses all of the headers,
 * but it should guaruntee correctness.  Later, we might implement
 * something to actually only remove the messages taht are marked
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
  ctx->id_hash = hash_create (257);
  ctx->subj_hash = hash_create (257);

  mutt_message (_("Reopening mailbox..."), CTX_DATA->mailbox);
  imap_quote_string (buf, sizeof(buf), CTX_DATA->mailbox);
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
	if (strncasecmp ("EXISTS", pc, 6) == 0)
	  count = n;
      }
      else if (imap_handle_untagged (ctx, buf) != 0)
	return (-1);
    }
  }
  while (strncmp (seq, buf, strlen (seq)) != 0);

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
	for (j = 0; j < i; j++)
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
	   *            * otherwise, the header may have been modified
	   *            externally,
	   *                       * and we don't want to lose _those_
	   *                       changes
	   *                                  */
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

static int imap_exec (char *buf, size_t buflen,
		      CONTEXT *ctx, const char *seq, const char *cmd, int flags)
{
  int count;

  mutt_socket_write (CTX_DATA->conn, cmd);

  do
  {
    if (mutt_socket_read_line_d (buf, buflen, CTX_DATA->conn) < 0)
      return (-1);

    if (buf[0] == '*' && imap_handle_untagged (ctx, buf) != 0)
      return (-1);
  }
  while (strncmp (buf, seq, SEQLEN) != 0);

  if (CTX_DATA->status == IMAP_NEW_MAIL || CTX_DATA->status == IMAP_EXPUNGE)
  {

    count = CTX_DATA->newMailCount;

    if (CTX_DATA->status == IMAP_NEW_MAIL && count > ctx->msgcount)
    {
      /* read new mail messages */
      dprint (1, (debugfile, "imap_exec(): new mail detected\n"));

      while (count > ctx->hdrmax)
	mx_alloc_memory (ctx);

      count = imap_read_headers (ctx, ctx->msgcount, count - 1) + 1;
    }
    else
    {
      imap_reopen_mailbox (ctx, NULL);
    }

    CTX_DATA->status = 0;

    mutt_clear_error ();
  }

  if (!imap_code (buf))
  {
    char *pc;

    if (flags == IMAP_OK_FAIL)
      return (-1);
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

static int imap_parse_path (char *path, char *host, size_t hlen, char **mbox)
{
  int n;
  char *pc;

  pc = path;
  if (*pc != '{')
    return (-1);
  pc++;
  n = 0;
  while (*pc && *pc != '}' && (n < hlen-1))
    host[n++] = *pc++;
  host[n] = 0;
  if (!*pc)
    return (-1);
  pc++;

  *mbox = pc;
  return 0;
}

static int imap_open_connection (CONTEXT *ctx, CONNECTION *conn)
{
  char *portnum, *hostname;
  struct sockaddr_in sin;
  struct hostent *he;
  char buf[LONG_STRING];
  char user[SHORT_STRING], q_user[SHORT_STRING];
  char pass[SHORT_STRING], q_pass[SHORT_STRING];
  char seq[16];

  memset (&sin, 0, sizeof (sin));
  hostname = safe_strdup (conn->server);
  portnum = strrchr (hostname, ':');
  if (portnum) 
  { 
    *portnum=0; 
    portnum++; 
  }
  sin.sin_port = htons (portnum ? atoi(portnum) : IMAP_PORT);
  sin.sin_family = AF_INET;
  if ((he = gethostbyname (hostname)) == NULL)
  {
    safe_free ((void*)&hostname);
    mutt_perror (conn->server);
    return (-1);
  }
  safe_free ((void*)&hostname);
  memcpy (&sin.sin_addr, he->h_addr_list[0], he->h_length);

  if ((conn->fd = socket (AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0)
  {
    mutt_perror ("socket");
    return (-1);
  }

  mutt_message (_("Connecting to %s..."), conn->server);

  if (connect (conn->fd, (struct sockaddr *) &sin, sizeof (sin)) < 0)
  {
    mutt_perror ("connect");
    close (conn->fd);
  }

  if (mutt_socket_read_line_d (buf, sizeof (buf), conn) < 0)
  {
    close (conn->fd);
    return (-1);
  }

  if (strncmp ("* OK", buf, 4) == 0)
  {
    if (!ImapUser)
    {
      strfcpy (user, NONULL(Username), sizeof (user));
      if (mutt_get_field (_("IMAP Username: "), user, sizeof (user), 0) != 0 ||
	  !user[0])
      {
	user[0] = 0;
	return (-1);
      }
    }
    else
      strfcpy (user, ImapUser, sizeof (user));

    if (!ImapPass)
    {
      pass[0]=0;
      snprintf (buf, sizeof (buf), _("Password for %s@%s: "), user, conn->server);
      if (mutt_get_field (buf, pass, sizeof (pass), M_PASS) != 0 ||
	  !pass[0])
      {
	return (-1);
      }
    }
    else
      strfcpy (pass, ImapPass, sizeof (pass));

    imap_quote_string (q_user, sizeof (q_user), user);
    imap_quote_string (q_pass, sizeof (q_pass), pass);

    mutt_message _("Logging in...");
    imap_make_sequence (seq, sizeof (seq));
    snprintf (buf, sizeof (buf), "%s LOGIN %s %s\r\n", seq, q_user, q_pass);
    if (imap_exec (buf, sizeof (buf), ctx, seq, buf, 0) != 0)
    {
      imap_error ("imap_open_connection()", buf);
      return (-1);
    }
    /* If they have a successful login, we may as well cache the user/password. */
    if (!ImapUser)
      ImapUser = safe_strdup (user);
    if (!ImapPass)
      ImapPass = safe_strdup (pass);
  }
  else if (strncmp ("* PREAUTH", buf, 9) != 0)
  {
    imap_error ("imap_open_connection()", buf);
    close (conn->fd);
    return (-1);
  }

  return 0;
}

int imap_open_mailbox (CONTEXT *ctx)
{
  CONNECTION *conn;
  char buf[LONG_STRING];
  char bufout[LONG_STRING];
  char host[SHORT_STRING];
  char seq[16];
  char *pc = NULL;
  int count = 0;
  int n;

  if (imap_parse_path (ctx->path, host, sizeof (host), &pc))
    return (-1);

  /* create IMAP-specific state struct */
  ctx->data = safe_malloc (sizeof (IMAP_DATA));
  memset (ctx->data, 0, sizeof (IMAP_DATA));

  CTX_DATA->mailbox = safe_strdup (pc);

  conn = mutt_socket_select_connection (host, IMAP_PORT, M_NEW_SOCKET);
  CTX_DATA->conn = conn;

  if (conn->uses == 0)
  {
    if (imap_open_connection (ctx, conn))
      return (-1);
    conn->data = (void *)ctx;
  }
  conn->uses++;

  mutt_message (_("Selecting %s..."), CTX_DATA->mailbox);
  imap_quote_string(buf, sizeof(buf), CTX_DATA->mailbox);
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
	if (strncasecmp ("EXISTS", pc, 6) == 0)
	  count = n;
      }
      else if (imap_handle_untagged (ctx, buf) != 0)
	return (-1);
    }
  }
  while (strncmp (seq, buf, strlen (seq)) != 0);

  ctx->hdrmax = count;
  ctx->hdrs = safe_malloc (count * sizeof (HEADER *));
  ctx->v2r = safe_malloc (count * sizeof (int));
  ctx->msgcount = 0;
  count=imap_read_headers (ctx, 0, count - 1) + 1;

  dprint (1, (debugfile, "imap_open_mailbox(): msgcount is %d\n", ctx->msgcount));
  return 0;
}

static int imap_create_mailbox (CONTEXT *ctx)
{
  char seq[8];
  char buf[LONG_STRING], mbox[LONG_STRING];

  imap_make_sequence (seq, sizeof (seq));
  imap_quote_string (mbox, sizeof (mbox), CTX_DATA->mailbox);
  snprintf (buf, sizeof (buf), "%s CREATE %s\r\n", seq, mbox);
      
  if (imap_exec (buf, sizeof (buf), ctx, seq, buf, 0) != 0)
  {
    imap_error ("imap_create_mailbox()", buf);
    return (-1);
  }
  return 0;
}

int imap_open_mailbox_append (CONTEXT *ctx)
{
  CONNECTION *conn;
  char host[SHORT_STRING];
  char buf[LONG_STRING], mbox[LONG_STRING];
  char seq[16];
  char *pc;

  if (imap_parse_path (ctx->path, host, sizeof (host), &pc))
    return (-1);

  /* create IMAP-specific state struct */
  ctx->data = safe_malloc (sizeof (IMAP_DATA));
  memset (ctx->data, 0, sizeof (IMAP_DATA));
  ctx->magic = M_IMAP;

  CTX_DATA->mailbox = pc;

  conn = mutt_socket_select_connection (host, IMAP_PORT, 0);
  CTX_DATA->conn = conn;

  if (conn->uses == 0)
    if (imap_open_connection (ctx, conn))
      return (-1);
  conn->uses++;

  /* check mailbox existance */

  imap_quote_string (mbox, sizeof (mbox), CTX_DATA->mailbox);
  imap_make_sequence (seq, sizeof (seq));
  snprintf (buf, sizeof (buf), "%s STATUS %s (UIDVALIDITY)\r\n", seq, mbox);
      
  if (imap_exec (buf, sizeof (buf), ctx, seq, buf, IMAP_OK_FAIL) != 0)
  {
    if (option (OPTCONFIRMCREATE))
    {
      snprintf (buf, sizeof (buf), _("Create %s?"), CTX_DATA->mailbox);
      if (mutt_yesorno (buf, 1) < 1)
	return (-1);
      if (imap_create_mailbox (ctx) < 0)
	return (-1);
    }
  }
  return 0;
}

int imap_fetch_message (MESSAGE *msg, CONTEXT *ctx, int msgno)
{
  char seq[8];
  char buf[LONG_STRING];
  char path[_POSIX_PATH_MAX];
  char *pc;
  char *pn;
  long bytes;
  int pos,len,onbody=0;
  IMAP_CACHE *cache;

  /* see if we already have the message in our cache */
  cache = &CTX_DATA->cache[ctx->hdrs[msgno]->index % IMAP_CACHE_LEN];

  if (cache->path)
  {
    if (cache->index == ctx->hdrs[msgno]->index)
    {
      /* yes, so just return a pointer to the message */
      if (!(msg->fp = fopen (cache->path, "r")))
      {
	mutt_perror (cache->path);
	return (-1);
      }
      return 0;
    }
    else
    {
      /* clear the previous entry */
      unlink (cache->path);
      FREE (&cache->path);
    }
  }

  mutt_message _("Fetching message...");

  cache->index = ctx->hdrs[msgno]->index;
  mutt_mktemp (path);
  cache->path = safe_strdup (path);
  if (!(msg->fp = safe_fopen (path, "w+")))
  {
    safe_free ((void **) &cache->path);
    return (-1);
  }

  imap_make_sequence (seq, sizeof (seq));
  snprintf (buf, sizeof (buf), "%s FETCH %d RFC822\r\n", seq,
	    ctx->hdrs[msgno]->index + 1);
  mutt_socket_write (CTX_DATA->conn, buf);
  do
  {
    if (mutt_socket_read_line (buf, sizeof (buf), CTX_DATA->conn) < 0)
    {
      return (-1);
    }

    if (buf[0] == '*')
    {
      pc = buf;
      pc = imap_next_word (pc);
      pc = imap_next_word (pc);
      if (strncasecmp ("FETCH", pc, 5) == 0)
      {
	if (!(pc = strchr (buf, '{')))
	{
	  imap_error ("imap_fetch_message()", buf);
	  return (-1);
	}
	pc++;
	pn = pc;
	while (isdigit (*pc))
	  pc++;
	*pc = 0;
	bytes = atoi (pn);
	for (pos = 0; pos < bytes; )
	{
	  len = mutt_socket_read_line (buf, sizeof (buf), CTX_DATA->conn);
	  if (len < 0)
	    return (-1);
	  pos += len;
	  fputs (buf, msg->fp);
	  fputs ("\n", msg->fp);
	  if (! onbody && len == 2)
	  {
	    /*
	     * This is the first time we really know how long the full
	     * header is. We must set it now, or mutt will not display
	     * the message properly
	     */
	    ctx->hdrs[msgno]->content->offset = ftell(msg->fp);
	    ctx->hdrs[msgno]->content->length = bytes - 
	      ctx->hdrs[msgno]->content->offset;
	    onbody=1;
	  }
	}
      }
      else if (imap_handle_untagged (ctx, buf) != 0)
	return (-1);
    }
  }
  while (strncmp (buf, seq, SEQLEN) != 0)
    ;

  mutt_clear_error();
  
  if (!imap_code (buf))
    return (-1);

  return 0;
}

static void
flush_buffer(char *buf, size_t *len, CONNECTION *conn)
{
  buf[*len] = '\0';
  mutt_socket_write(conn, buf);
  *len = 0;
}

int imap_append_message (CONTEXT *ctx, MESSAGE *msg)
{
  FILE *fp;
  char seq[8];
  char buf[LONG_STRING];
  size_t len;
  int c, last;
  
  if ((fp = fopen (msg->path, "r")) == NULL)
  {
    mutt_perror (msg->path);
    return (-1);
  }

  for(last = EOF, len = 0; (c = fgetc(fp)) != EOF; last = c)
  {
    if(c == '\n' && last != '\r')
      len++;

    len++;
  }
  rewind(fp);
  
  mutt_message _("Sending APPEND command ...");
  imap_make_sequence (seq, sizeof (seq));
  snprintf (buf, sizeof (buf), "%s APPEND %s {%d}\r\n", seq, 
      CTX_DATA->mailbox, len);

  mutt_socket_write (CTX_DATA->conn, buf);

  do 
  {
    if (mutt_socket_read_line_d (buf, sizeof (buf), CTX_DATA->conn) < 0)
    {
      fclose (fp);
      return (-1);
    }

    if (buf[0] == '*' && ctx->data && 
	imap_handle_untagged (((CONTEXT *)(CTX_DATA->conn))->data, buf) != 0)
    {
      return (-1);
      fclose (fp);
    }
  }
  while ((strncmp (buf, seq, SEQLEN) != 0) && (buf[0] != '+'));

  if (buf[0] != '+')
  {
    char *pc;

    dprint (1, (debugfile, "imap_append_message(): command failed: %s\n", buf));

    pc = buf + SEQLEN;
    SKIPWS (pc);
    pc = imap_next_word (pc);
    mutt_error (pc);
    sleep (1);
    fclose (fp);
    return (-1);
  }

  mutt_message _("Uploading message ...");

  for(last = EOF, len = 0; (c = fgetc(fp)) != EOF; last = c)
  {
    if(c == '\n' && last != '\r')
      buf[len++] = '\r';

    buf[len++] = c;

    if(len > sizeof(buf) - 3)
      flush_buffer(buf, &len, CTX_DATA->conn);
  }
  
  if(len)
    flush_buffer(buf, &len, CTX_DATA->conn);

    
  mutt_socket_write (CTX_DATA->conn, "\r\n");
  fclose (fp);

  do
  {
    if (mutt_socket_read_line_d (buf, sizeof (buf), CTX_DATA->conn) < 0)
      return (-1);

    if (buf[0] == '*' && imap_handle_untagged (ctx, buf) != 0)
      return (-1);
  }
  while (strncmp (buf, seq, SEQLEN) != 0);

  if (!imap_code (buf))
  {
    char *pc;

    dprint (1, (debugfile, "imap_append_message(): command failed: %s\n", buf));
    pc = buf + SEQLEN;
    SKIPWS (pc);
    pc = imap_next_word (pc);
    mutt_error (pc);
    sleep (1);
    return (-1);
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
    while (strncmp (seq, buf, SEQLEN) != 0);
    mutt_clear_error ();
  }
  close (CTX_DATA->conn->fd);
  CTX_DATA->conn->uses = 0;
  CTX_DATA->conn->data = NULL;
  return 0;
}

int imap_sync_mailbox (CONTEXT *ctx)
{
  char seq[8];
  char buf[LONG_STRING];
  char tmp[LONG_STRING];
  int n;

  /* save status changes */
  for (n = 0; n < ctx->msgcount; n++)
  {
    if (ctx->hdrs[n]->deleted || ctx->hdrs[n]->changed)
    {
      snprintf (tmp, sizeof (tmp), _("Saving message status flags... [%d/%d]"),
	n+1, ctx->msgcount);
      mutt_message (tmp);
      *tmp = 0;
      if (ctx->hdrs[n]->read)
	strcat (tmp, "\\Seen ");
      if (ctx->hdrs[n]->flagged)
	strcat (tmp, "\\Flagged ");
      if (ctx->hdrs[n]->replied)
	strcat (tmp, "\\Answered ");
      if (ctx->hdrs[n]->deleted)
        strcat (tmp, "\\Deleted");
      mutt_remove_trailing_ws (tmp);

      if (!*tmp) continue; /* imapd doesn't like empty flags. */
      imap_make_sequence (seq, sizeof (seq));
      snprintf (buf, sizeof (buf), "%s STORE %d FLAGS.SILENT (%s)\r\n", seq, 
      	ctx->hdrs[n]->index + 1, tmp);
      if (imap_exec (buf, sizeof (buf), ctx, seq, buf, 0) != 0)
      {
	imap_error ("imap_sync_mailbox()", buf);
	return (-1);
      }
    }
  }

  mutt_message _("Expunging messages from server...");
  CTX_DATA->status = IMAP_EXPUNGE;
  imap_make_sequence (seq, sizeof (seq));
  snprintf (buf, sizeof (buf), "%s EXPUNGE\r\n", seq);
  if (imap_exec (buf, sizeof (buf), ctx, seq, buf, 0) != 0)
  {
    imap_error ("imap_sync_mailbox()", buf);
    return (-1);
  }
  CTX_DATA->status = 0;

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
  CTX_DATA->conn->uses--;
  if ((CTX_DATA->conn->uses == 0) || (CTX_DATA->status == IMAP_BYE))
    imap_close_connection (ctx);
  for (i = 0; i < IMAP_CACHE_LEN; i++)
  {
    if (CTX_DATA->cache[i].path)
    {
      unlink (CTX_DATA->cache[i].path);
      safe_free ((void **) &CTX_DATA->cache[i].path);
    }
  }
  safe_free ((void **) &ctx->data);
}

/* commit changes and terminate connection */
int imap_close_mailbox (CONTEXT *ctx)
{
  char seq[8];
  char buf[LONG_STRING];

  /* tell the server to commit changes */
  mutt_message _("Closing mailbox...");
  imap_make_sequence (seq, sizeof (seq));
  snprintf (buf, sizeof (buf), "%s CLOSE\r\n", seq);
  if (imap_exec (buf, sizeof (buf), ctx, seq, buf, 0) != 0)
  {
    imap_error ("imap_close_mailbox()", buf);
    return (-1);
  }
  return 0;
}

/* use the NOOP command to poll for new mail */
int imap_check_mailbox (CONTEXT *ctx, int *index_hint)
{
  char seq[8];
  char buf[LONG_STRING];
  static time_t checktime=0;
  int msgcount = ctx->msgcount;

  if (ImapCheckTime)
  {
    time_t k=time(NULL);
    if (checktime && (k-checktime < ImapCheckTime)) return 0;
    checktime=k;
  }

  imap_make_sequence (seq, sizeof (seq));
  snprintf (buf, sizeof (buf), "%s NOOP\r\n", seq);
  if (imap_exec (buf, sizeof (buf), ctx, seq, buf, 0) != 0)
  {
    imap_error ("imap_check_mailbox()", buf);
    return (-1);
  }

  return (msgcount != ctx->msgcount);
}

int imap_buffy_check (char *path)
{
  CONNECTION *conn;
  char host[SHORT_STRING];
  char buf[LONG_STRING];
  char seq[8];
  char *mbox;
  char *s;
  char recent = FALSE;

  if (imap_parse_path (path, host, sizeof (host), &mbox))
    return -1;

  conn = mutt_socket_select_connection (host, IMAP_PORT, 0);

  /* Currently, we don't open a connection to check, but we'll check
   * over an existing connection */
  if (conn->uses == 0)
      return (-1);
  conn->uses++;

  imap_make_sequence (seq, sizeof (seq));
  snprintf (buf, sizeof (buf), "%s STATUS %s (RECENT)\r\n", seq, mbox);

  mutt_socket_write (conn, buf);

  do 
  {
    if (mutt_socket_read_line_d (buf, sizeof (buf), conn) < 0)
    {
      return (-1);
    }

    /* BUG: We don't handle other untagged messages here.  This is
     * actually because of a more general problem in that this
     * connection is being used for a different mailbox, and all
     * untagged messages we don't handle here (ie, STATUS) need to be
     * sent to imap_handle_untagged() but with the CONTEXT of the
     * currently selected mailbox.  The same is broken in the APPEND
     * stuff above.
     * FIXED?  We now store the "real" CTX in the CONNECTION structure
     * and use that here and in append.
     */
    if (buf[0] == '*') 
    {
      s = imap_next_word (buf);
      if (strncasecmp ("STATUS", s, 6) == 0)
      {
	s = imap_next_word (s);
	if (strncmp (mbox, s, strlen (mbox)) == 0)
	{
	  s = imap_next_word (s);
	  s = imap_next_word (s);
	  if (isdigit (*s))
	  {
	    if (*s != '0')
	    {
	      dprint (1, (debugfile, "New mail in %s\n", path));
	      recent = TRUE;
	    }
	  }
	}
      }
      else
      {
	if (conn->data && 
	    imap_handle_untagged ((CONTEXT *)(conn->data), buf) != 0)
	  return (-1);
      }
    }
  }
  while ((strncmp (buf, seq, SEQLEN) != 0));

  conn->uses--;

  return recent;
}
