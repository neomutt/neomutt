/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (C) 1996-9 Brandon Long <blong@fiction.net>
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
#include "browser.h"

#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>






#ifdef _PGPPATH
#include "pgp.h"
#endif





/* Minimal support for IMAP 4rev1. Slightly more minimal for IMAP 4. */

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

/* Capabilities */
enum
{
  IMAP4 = 0,
  IMAP4REV1,
  STATUS,
  ACL,				/* RFC 2086: IMAP4 ACL extension */
  NAMESPACE,                   	/* RFC 2342: IMAP4 Namespace */
  /* From here down, we don't care */
  ACRAM_MD5,			/* AUTH=CRAM-MD5 */
  AKERBEROS_V4,			/* AUTH=KERBEROS_V4 */
  AGSSAPI,			/* AUTH=GSSAPI */
  ALOGIN,			/* AUTH=LOGIN */
  AUTH_LOGIN,			/* AUTH-LOGIN */
  APLAIN,			/* AUTH=PLAIN */
  ASKEY,			/* AUTH=SKEY */
  IDLE,				/* RFC 2177: IMAP4 IDLE command */
  LOGIN_REFERRALS,		/* LOGIN-REFERRALS */
  MAILBOX_REFERRALS,		/* MAILBOX-REFERRALS */
  SCAN,
  SORT,
  TORDEREDSUBJECT,		/* THREAD=ORDEREDSUBJECT */
  UIDPLUS,			/* RFC 2859: IMAP4 UIDPLUS extension */
  
  CAPMAX
};

static char *Capabilities[] = {"IMAP4", "IMAP4rev1", "STATUS", "ACL", 
  "NAMESPACE", "AUTH=CRAM-MD5", "AUTH=KERBEROS_V4", "AUTH=GSSAPI", 
  "AUTH=LOGIN", "AUTH-LOGIN", "AUTH=PLAIN", "AUTH=SKEY", "IDLE", 
  "LOGIN-REFERRALS", "MAILBOX-REFERRALS", "QUOTA", "SCAN", "SORT", 
  "THREAD=ORDEREDSUBJECT", "UIDPLUS", NULL};

/* ACL Rights */
enum
{
  IMAP_ACL_LOOKUP = 0,
  IMAP_ACL_READ,
  IMAP_ACL_SEEN,
  IMAP_ACL_WRITE,
  IMAP_ACL_INSERT,
  IMAP_ACL_POST,
  IMAP_ACL_CREATE,
  IMAP_ACL_DELETE,
  IMAP_ACL_ADMIN,

  RIGHTSMAX
};

enum
{
  /* States */
  IMAP_DISCONNECTED = 0,
  IMAP_CONNECTED,
  IMAP_AUTHENTICATED,
  IMAP_SELECTED
};

typedef struct
{
  unsigned int index;
  char *path;
} IMAP_CACHE;

typedef struct
{
  /* This data is specific to a CONNECTION to an IMAP server */
  short status;
  short state;
  char delim;
  unsigned char capabilities[(CAPMAX + 7)/8];
  CONNECTION *conn;

  /* The following data is all specific to the currently SELECTED mbox */
  CONTEXT *selected_ctx;
  char *selected_mailbox;
  unsigned char rights[(RIGHTSMAX + 7)/8];
  unsigned int newMailCount;
  IMAP_CACHE cache[IMAP_CACHE_LEN];
} IMAP_DATA;

#define CTX_DATA ((IMAP_DATA *) ctx->data)
#define CONN_DATA ((IMAP_DATA *) conn->data)
#define SELCTX_DATA ((IMAP_DATA *) selctx->data)

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

enum
{
  /* Namespace types */
  IMAP_NS_PERSONAL = 0,
  IMAP_NS_OTHER,
  IMAP_NS_SHARED
};

typedef struct 
{
  int type;
  int listable;
  char *prefix;
  char delim;
  int home_namespace;
  /* We get these when we check if namespace exists - cache them */
  int noselect;			
  int noinferiors;
} IMAP_NAMESPACE_INFO;

static void imap_make_sequence (char *buf, size_t buflen)
{
  static int sequence = 0;
  
  snprintf (buf, buflen, "a%04d", sequence++);

  if (sequence > 9999)
    sequence = 0;
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
	if (mutt_strncasecmp ("FLAGS", s, 5) == 0)
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
	else if (mutt_strncasecmp ("INTERNALDATE", s, 12) == 0)
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
	else if (mutt_strncasecmp ("RFC822.SIZE", s, 11) == 0)
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
	else if (mutt_strncasecmp ("\\deleted", s, 8) == 0)
	{
	  s += 8;
	  h->deleted = 1;
	}
	else if (mutt_strncasecmp ("\\flagged", s, 8) == 0)
	{
	  s += 8;
	  h->flagged = 1;
	}
	else if (mutt_strncasecmp ("\\answered", s, 9) == 0)
	{
	  s += 9;
	  h->replied = 1;
	}
	else if (mutt_strncasecmp ("\\seen", s, 5) == 0)
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

static void imap_unquote_string (char *s)
{
  char *d = s;

  if (*s == '\"')
    s++;
  else
    return;

  while (*s)
  {
    if (*s == '\"')
    {
      *d = '\0';
      return;
    }
    if (*s == '\\')
    {
      s++;
    }
    if (*s)
    {
      *d = *s;
      d++;
      s++;
    }
  }
  *d = '\0';
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
  return (mutt_strncasecmp ("OK", s, 2) == 0);
}

static char *imap_next_word (char *s)
{
  while (*s && !ISSPACE (*s))
    s++;
  SKIPWS (s);
  return s;
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

static int imap_handle_untagged (IMAP_DATA *idata, char *s)
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
    dprint (1, (debugfile, "imap_unhandle_untagged(): unhandled request: %s\n",
		s));
  }

  return 0;
}

static int get_literal_count(const char *buf, long *bytes)
{
  char *pc;
  char *pn;

  if (!(pc = strchr (buf, '{')))
    return (-1);
  pc++;
  pn = pc;
  while (isdigit (*pc))
    pc++;
  *pc = 0;
  *bytes = atoi(pn);
  return (0);
}

/*
 * Changed to read many headers instead of just one. It will return the
 * msgno of the last message read. It will return a value other than
 * msgend if mail comes in while downloading headers (in theory).
 */
static int imap_read_headers (CONTEXT *ctx, int msgbegin, int msgend)
{
  char buf[LONG_STRING],fetchbuf[LONG_STRING];
  char hdrreq[STRING];
  FILE *fp;
  char tempfile[_POSIX_PATH_MAX];
  char seq[8];
  char *pc,*fpc,*hdr;
  long ploc;
  long bytes = 0;
  int msgno,fetchlast;
  IMAP_HEADER_INFO *h0,*h,*htemp;
#define WANT_HEADERS  "DATE FROM SUBJECT TO CC MESSAGE-ID REFERENCES CONTENT-TYPE IN-REPLY-TO REPLY-TO" 
  const char *want_headers = WANT_HEADERS;
  int using_body_peek = 0;
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
      if (mutt_bit_isset(CTX_DATA->capabilities,IMAP4REV1))
      {
	snprintf(hdrreq, sizeof (hdrreq), "BODY.PEEK[HEADER.FIELDS (%s)]", 
		want_headers); 
	using_body_peek = 1;
      } 
      else if (mutt_bit_isset(CTX_DATA->capabilities,IMAP4))
      {
	snprintf(hdrreq, sizeof (hdrreq), "RFC822.HEADER.LINES (%s)", 
		want_headers);
      }
      else
      {	/* Unable to fetch headers for lower versions */
	mutt_error _("Unable to fetch headers from this IMAP server version.");
	sleep (1);	/* pause a moment to let the user see the error */
	return (-1);
      }
      snprintf (buf, sizeof (buf), 
		"%s FETCH %d:%d (FLAGS INTERNALDATE RFC822.SIZE %s)\r\n", 
		seq, msgno + 1, msgend + 1, hdrreq);

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
        if (mutt_strncasecmp ("FETCH", pc, 5) == 0)
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
	    if (using_body_peek) 
	      hdr=strstr(pc,"BODY");
	    else
	      hdr=strstr(pc,"RFC822.HEADER");
	    if (!hdr)
            {
              imap_error ("imap_read_headers()", buf);
              return (-1);
            }
            strncpy(fpc,pc,hdr-pc);
            fpc += hdr-pc;
            *fpc = '\0';
            pc=hdr;
            /* get some number of bytes */
	    if (get_literal_count(buf, &bytes) < 0)
            {
              imap_error ("imap_read_headers()", buf);
              return (-1);
            }
            imap_read_bytes (fp, CTX_DATA->conn, bytes);
            if (mutt_socket_read_line_d (buf, sizeof (buf), CTX_DATA->conn) < 0)
            {
              return (-1);
            }
            pc = buf;
          }
        }
        else if (imap_handle_untagged (CTX_DATA, buf) != 0)
          return (-1);
      }
    }
    while ((msgno + 1) >= fetchlast && mutt_strncmp (seq, buf, SEQLEN) != 0);

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
  ctx->id_hash = hash_create (1031);
  ctx->subj_hash = hash_create (1031);

  mutt_message (_("Reopening mailbox... %s"), CTX_DATA->selected_mailbox);
  imap_quote_string (buf, sizeof(buf), CTX_DATA->selected_mailbox);
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
static int imap_exec (char *buf, size_t buflen, IMAP_DATA *idata,
		      const char *seq, const char *cmd, int flags)
{
  int count;

  mutt_socket_write (idata->conn, cmd);

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
    }
    else
    {
      imap_reopen_mailbox (idata->selected_ctx, NULL);
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

static int imap_parse_path (char *path, char *host, size_t hlen, int *port, 
    char **mbox)
{
  int n;
  char *pc;
  char *pt;

  /* set default port */
  *port = IMAP_PORT;
  pc = path;
  if (*pc != '{')
    return (-1);
  pc++;
  n = 0;
  while (*pc && *pc != '}' && *pc != ':' && (n < hlen-1))
    host[n++] = *pc++;
  host[n] = 0;
  if (!*pc)
    return (-1);
  if (*pc == ':')
  {
    pc++;
    pt = pc;
    while (*pc && *pc != '}') pc++;
    if (!*pc)
      return (-1);
    *pc = '\0';
    *port = atoi (pt);
    *pc = '}';
  }
  pc++;

  *mbox = pc;
  return 0;
}

/*
 * Fix up the imap path.  This is necessary because the rest of mutt
 * assumes a hierarchy delimiter of '/', which is not necessarily true
 * in IMAP.  Additionally, the filesystem converts multiple hierarchy
 * delimiters into a single one, ie "///" is equal to "/".  IMAP servers
 * are not required to do this.
 */
static char *imap_fix_path (IMAP_DATA *idata, char *mailbox, char *path, 
    size_t plen)
{
  int x = 0;

  if (!mailbox || !*mailbox)
  {
    strfcpy (path, "INBOX", plen);
    return path;
  }

  while (mailbox && *mailbox && (x < (plen - 1)))
  {
    if ((*mailbox == '/') || (*mailbox == idata->delim))
    {
      while ((*mailbox == '/') || (*mailbox == idata->delim)) mailbox++;
      path[x] = idata->delim;
    }
    else
    {
      path[x] = *mailbox;
      mailbox++;
    }
    x++;
  }
  path[x] = '\0';
  return path;
}

static int imap_check_acl (IMAP_DATA *idata)
{
  char buf[LONG_STRING];
  char mbox[LONG_STRING];
  char seq[16];

  imap_make_sequence (seq, sizeof (seq));
  imap_quote_string (mbox, sizeof(mbox), idata->selected_mailbox);
  snprintf (buf, sizeof (buf), "%s MYRIGHTS %s\r\n", seq, mbox);
  if (imap_exec (buf, sizeof (buf), idata, seq, buf, 0) != 0)
  {
    imap_error ("imap_check_acl", buf);
    return (-1);
  }
  return (0);
}

static int imap_check_capabilities (IMAP_DATA *idata)
{
  char buf[LONG_STRING];
  char seq[16];

  imap_make_sequence (seq, sizeof (seq));
  snprintf (buf, sizeof (buf), "%s CAPABILITY\r\n", seq);
  if (imap_exec (buf, sizeof (buf), idata, seq, buf, 0) != 0)
  {
    imap_error ("imap_check_capabilities", buf);
    return (-1);
  }
  if (!(mutt_bit_isset(idata->capabilities,IMAP4)
      ||mutt_bit_isset(idata->capabilities,IMAP4REV1)))
  {
    mutt_error _("This IMAP server is ancient. Mutt does not work with it.");
    sleep (5);	/* pause a moment to let the user see the error */
    return (-1);
  }
  return (0);
}

static int imap_authenticate (IMAP_DATA *idata, CONNECTION *conn)
{
  char buf[LONG_STRING];
  char user[SHORT_STRING], q_user[SHORT_STRING];
  char pass[SHORT_STRING], q_pass[SHORT_STRING];
  char seq[16];

  int r = 1;

  while (r != 0)
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
    r = imap_exec (buf, sizeof (buf), idata, seq, buf, IMAP_OK_FAIL);
    if (r == -1)
    {
      /* connection or protocol problem */
      imap_error ("imap_open_connection()", buf);
      return (-1);
    }
    else if (r == -2)
    {
      /* Login failed, try again */
      mutt_error _("Login failed.");
      sleep (1);
      FREE (&ImapUser);
      FREE (&ImapPass);
    }
    else
    {
      /* If they have a successful login, we may as well cache the 
       * user/password. */
      if (!ImapUser)
	ImapUser = safe_strdup (user);
      if (!ImapPass)
	ImapPass = safe_strdup (pass);
    }
  }
  return 0;
}

static int imap_open_connection (IMAP_DATA *idata, CONNECTION *conn)
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
  else if (mutt_strncmp ("* PREAUTH", buf, 9) != 0)
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
  if (port != IMAP_PORT)
    snprintf (buf, sizeof (buf), "{%s:%d}%s", host, port, 
	idata->selected_mailbox);
  else
    snprintf (buf, sizeof (buf), "{%s}%s", host, idata->selected_mailbox);

  FREE (&(ctx->path));
  ctx->path = safe_strdup (buf);

  idata->selected_ctx = ctx;

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
	*pc++ = 0;
	n = atoi (pn);
	SKIPWS (pc);
	if (mutt_strncasecmp ("EXISTS", pc, 6) == 0)
	  count = n;
      }
      else if (imap_handle_untagged (idata, buf) != 0)
	return (-1);
    }
  }
  while (mutt_strncmp (seq, buf, mutt_strlen (seq)) != 0);

  if (!imap_code (buf))
  {
    char *s;
    s = imap_next_word (buf); /* skip seq */
    s = imap_next_word (s); /* Skip response */
    mutt_error (s);
    idata->state = IMAP_AUTHENTICATED;
    sleep (1);
    return (-1);
  }

  if (mutt_bit_isset (idata->capabilities, ACL))
  {
    if (imap_check_acl (idata))
    {
      return (-1);
    }
  }
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

static int imap_create_mailbox (IMAP_DATA *idata, char *mailbox)
{
  char seq[8];
  char buf[LONG_STRING], mbox[LONG_STRING];

  imap_make_sequence (seq, sizeof (seq));
  imap_quote_string (mbox, sizeof (mbox), mailbox);
  snprintf (buf, sizeof (buf), "%s CREATE %s\r\n", seq, mbox);
      
  if (imap_exec (buf, sizeof (buf), idata, seq, buf, 0) != 0)
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
  char seq[16];
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
  imap_make_sequence (seq, sizeof (seq));
				
  if (mutt_bit_isset(idata->capabilities,IMAP4REV1))
  {
    snprintf (buf, sizeof (buf), "%s STATUS %s (UIDVALIDITY)\r\n", seq, mbox);
  }
  else if (mutt_bit_isset(idata->capabilities,STATUS))
  { 
    /* We have no idea what the other guy wants. UW imapd 8.3 wants this
     * (but it does not work if another mailbox is selected) */
    snprintf (buf, sizeof (buf), "%s STATUS %s (UID-VALIDITY)\r\n", seq, mbox);
  }
  else
  {
    /* STATUS not supported
     * The thing to do seems to be:
     *  - Open a *new* IMAP session, select, and then close it. Report the
     * error if the mailbox did not exist. */
    mutt_message _("Unable to append to IMAP mailboxes at this server");
    return (-1);
  }

  r = imap_exec (buf, sizeof (buf), idata, seq, buf, IMAP_OK_FAIL);
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
    return (-1);
  }
  return 0;
}

int imap_fetch_message (MESSAGE *msg, CONTEXT *ctx, int msgno)
{
  char seq[8];
  char buf[LONG_STRING];
  char path[_POSIX_PATH_MAX];
  char *pc;
  long bytes;
  int pos, len;
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
      if (mutt_strncasecmp ("FETCH", pc, 5) == 0)
      {
        if (get_literal_count(buf, &bytes) < 0)
	{
	  imap_error ("imap_fetch_message()", buf);
	  return (-1);
	}
	for (pos = 0; pos < bytes; )
	{
	  len = mutt_socket_read_line (buf, sizeof (buf), CTX_DATA->conn);
	  if (len < 0)
	    return (-1);
	  pos += len;
	  fputs (buf, msg->fp);
	  fputs ("\n", msg->fp);
	}
      }
      else if (imap_handle_untagged (CTX_DATA, buf) != 0)
	return (-1);
    }
  }
  while (mutt_strncmp (buf, seq, SEQLEN) != 0)
    ;

  if (!imap_code (buf))
    return (-1);

  /* Update the header information.  Previously, we only downloaded a
   * portion of the headers, those required for the main display.
   */
  rewind (msg->fp);
  mutt_free_envelope (&ctx->hdrs[msgno]->env);
  ctx->hdrs[msgno]->env = mutt_read_rfc822_header (msg->fp, ctx->hdrs[msgno],0);
  fgets (buf, sizeof (buf), msg->fp);
  while (!feof (msg->fp))
  {
    ctx->hdrs[msgno]->lines++;
    fgets (buf, sizeof (buf), msg->fp);
  }

  ctx->hdrs[msgno]->content->length = ftell (msg->fp) - 
                                        ctx->hdrs[msgno]->content->offset;

  /* This needs to be done in case this is a multipart message */
#ifdef _PGPPATH
  ctx->hdrs[msgno]->pgp = pgp_query (ctx->hdrs[msgno]->content);
#endif /* _PGPPATH */

  mutt_clear_error();
  rewind (msg->fp);

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
  char buf[LONG_STRING];
  char host[SHORT_STRING];
  char mbox[LONG_STRING];
  char mailbox[LONG_STRING]; 
  char seq[16];
  char *pc;
  int port;
  size_t len;
  int c, last;

  if (imap_parse_path (ctx->path, host, sizeof (host), &port, &pc))
    return (-1);

  imap_fix_path (CTX_DATA, pc, mailbox, sizeof (mailbox));
  
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

  imap_quote_string (mbox, sizeof (mbox), mailbox);
  imap_make_sequence (seq, sizeof (seq));
  snprintf (buf, sizeof (buf), "%s APPEND %s {%d}\r\n", seq, mbox, len);

  mutt_socket_write (CTX_DATA->conn, buf);

  do 
  {
    if (mutt_socket_read_line_d (buf, sizeof (buf), CTX_DATA->conn) < 0)
    {
      fclose (fp);
      return (-1);
    }

    if (buf[0] == '*' && imap_handle_untagged (CTX_DATA, buf) != 0)
    {
      fclose (fp);
      return (-1);
    }
  }
  while ((mutt_strncmp (buf, seq, SEQLEN) != 0) && (buf[0] != '+'));

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

    if (buf[0] == '*' && imap_handle_untagged (CTX_DATA, buf) != 0)
      return (-1);
  }
  while (mutt_strncmp (buf, seq, SEQLEN) != 0);

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
    while (mutt_strncmp (seq, buf, SEQLEN) != 0);
    mutt_clear_error ();
  }
  close (CTX_DATA->conn->fd);
  CTX_DATA->state = IMAP_DISCONNECTED;
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
      if (ctx->hdrs[n]->read && mutt_bit_isset(CTX_DATA->rights, IMAP_ACL_SEEN))
	strcat (tmp, "\\Seen ");
      if (mutt_bit_isset(CTX_DATA->rights, IMAP_ACL_WRITE))
      {
	if (ctx->hdrs[n]->flagged)
	  strcat (tmp, "\\Flagged ");
	if (ctx->hdrs[n]->replied)
	  strcat (tmp, "\\Answered ");
      }
      if (ctx->hdrs[n]->deleted && 
	  mutt_bit_isset(CTX_DATA->rights, IMAP_ACL_DELETE))
        strcat (tmp, "\\Deleted");
      mutt_remove_trailing_ws (tmp);

      if (!*tmp) continue; /* imapd doesn't like empty flags. */
      imap_make_sequence (seq, sizeof (seq));
      snprintf (buf, sizeof (buf), "%s STORE %d FLAGS.SILENT (%s)\r\n", seq, 
      	ctx->hdrs[n]->index + 1, tmp);
      if (imap_exec (buf, sizeof (buf), CTX_DATA, seq, buf, 0) != 0)
      {
	imap_error ("imap_sync_mailbox()", buf);
	return (-1);
      }
    }
  }

  if (mutt_bit_isset(CTX_DATA->rights, IMAP_ACL_DELETE))
  {
    mutt_message _("Expunging messages from server...");
    CTX_DATA->status = IMAP_EXPUNGE;
    imap_make_sequence (seq, sizeof (seq));
    snprintf (buf, sizeof (buf), "%s EXPUNGE\r\n", seq);
    if (imap_exec (buf, sizeof (buf), CTX_DATA, seq, buf, 0) != 0)
    {
      imap_error ("imap_sync_mailbox()", buf);
      return (-1);
    }
    CTX_DATA->status = 0;
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

/* commit changes and terminate connection */
static int imap_close_mailbox (IMAP_DATA *idata)
{
  char seq[8];
  char buf[LONG_STRING];

  /* tell the server to commit changes */
  mutt_message _("Closing mailbox...");
  imap_make_sequence (seq, sizeof (seq));
  snprintf (buf, sizeof (buf), "%s CLOSE\r\n", seq);
  if (imap_exec (buf, sizeof (buf), idata, seq, buf, 0) != 0)
  {
    imap_error ("imap_close_mailbox()", buf);
    idata->status = IMAP_FATAL;
    return (-1);
  }
  idata->state = IMAP_AUTHENTICATED;
  return 0;
}


void imap_fastclose_mailbox (CONTEXT *ctx)
{
  int i;

  /* Check to see if the mailbox is actually open */
  if (!ctx->data)
    return;

  if ((CTX_DATA->state == IMAP_SELECTED) && (ctx == CTX_DATA->selected_ctx))
    if (imap_close_mailbox (CTX_DATA) != 0)

  for (i = 0; i < IMAP_CACHE_LEN; i++)
  {
    if (CTX_DATA->cache[i].path)
    {
      unlink (CTX_DATA->cache[i].path);
      safe_free ((void **) &CTX_DATA->cache[i].path);
    }
  }
  if (CTX_DATA->status == IMAP_BYE || CTX_DATA->status == IMAP_FATAL)
  {
    imap_close_connection (ctx);
    CTX_DATA->conn->data = NULL;
    safe_free ((void **) &ctx->data);
  }
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
  if (imap_exec (buf, sizeof (buf), CTX_DATA, seq, buf, 0) != 0)
  {
    imap_error ("imap_check_mailbox()", buf);
    return (-1);
  }

  return (msgcount != ctx->msgcount);
}

int imap_buffy_check (char *path)
{
  CONNECTION *conn;
  IMAP_DATA *idata;
  char host[SHORT_STRING];
  char buf[LONG_STRING];
  char mbox[LONG_STRING];
  char seq[8];
  char *s;
  char *pc;
  char recent = FALSE;
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
  /* The draft IMAP implementor's guide warns againts using the STATUS
   * command on a mailbox that you have selected */
  if (mutt_strcmp(mbox, idata->selected_mailbox) == 0
      || (mutt_strcasecmp(mbox, "INBOX") == 0
	  && mutt_strcasecmp(mbox, idata->selected_mailbox) == 0))
  {
    snprintf (buf, sizeof (buf), "%s NOOP\r\n", seq);
  }
  else if (mutt_bit_isset(idata->capabilities,IMAP4REV1) ||
	   mutt_bit_isset(idata->capabilities,STATUS))
  {				
    snprintf (buf, sizeof (buf), "%s STATUS %s (RECENT)\r\n", seq, mbox);
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
	if (mutt_strncmp (mbox, s, mutt_strlen (mbox)) == 0)
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
	    imap_handle_untagged (idata, buf) != 0)
	  return (-1);
      }
    }
  }
  while ((mutt_strncmp (buf, seq, SEQLEN) != 0));

  conn->uses--;

  return recent;
}

static void add_folder (char delim, char *folder, char *host, int port, 
    int noselect, int noinferiors, struct browser_state *state)
{
  char tmp[LONG_STRING];
  char idir[LONG_STRING];
  int flen = strlen (folder);
  int x;
  char *f;

  imap_unquote_string (folder);
  for (x = 0; x < state->entrylen; x++)
  {
    f = strchr ((state->entry)[x].name, '}');
    if (f)
    {
      int len = strlen (folder);

      f++;
      if (!strncmp (f, folder, len) && (f[len] == '\0' || f[len] == delim))
	return;
    }
  }
  if (state->entrylen + 1 == state->entrymax)
  {
    safe_realloc ((void **) &state->entry,
	sizeof (struct folder_file) * (state->entrymax += 256));
  }
  if (!noselect)
  {
    if (port == IMAP_PORT)
      snprintf (tmp, sizeof (tmp), "{%s}%s", host, folder);
    else
      snprintf (tmp, sizeof (tmp), "{%s:%d}%s", host, port, folder);
    (state->entry)[state->entrylen].name = safe_strdup (tmp);
    snprintf (tmp, sizeof (tmp), "IMAP %-25s %-25s", host, folder);
    (state->entry)[state->entrylen].desc = safe_strdup (tmp);
    (state->entry)[state->entrylen].notfolder = 0;
    (state->entrylen)++;
  }
  if (!noinferiors)
  {
    int nodelim = 0;

    if (flen == 0 || delim == folder[flen - 1])
      nodelim = 1;
    if (port == IMAP_PORT)
      snprintf (tmp, sizeof (tmp), "{%s}%s%c", host, folder, 
	  nodelim ? '\0' : delim);
    else
      snprintf (tmp, sizeof (tmp), "{%s:%d}%s%c", host, port, folder, 
	  nodelim ? '\0' : delim);
    (state->entry)[state->entrylen].name = safe_strdup (tmp);
    snprintf (idir, sizeof (idir), "%s%c", folder, 
	nodelim ? '\0' : delim);
    snprintf (tmp, sizeof (tmp), "IMAP %-25s %-25s", host, idir);
    (state->entry)[state->entrylen].desc = safe_strdup (tmp);
    (state->entry)[state->entrylen].notfolder = 1;
    (state->entrylen)++;
  }
}

static int imap_parse_list_response(CONNECTION *conn, char *buf, int buflen,
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
	  if (!strncmp (s, "\\NoSelect", 9))
	    *noselect = 1;
	  if (!strncmp (s, "\\NoInferiors", 12))
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
	if (s && s[0] == '\"' && s[1] && s[2] == '\"')
	  *delim = s[1];
	else if (s && s[0] == '\"' && s[1] && s[1] == '\\' && s[2] && s[3] == '\"')
	  *delim = s[2];
      s = imap_next_word (s); /* name */
      if (s && *s == '{')	/* Literal */
      { 
	int len;
	
	if (get_literal_count(buf, &bytes) < 0)
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

static int add_list_result (CONNECTION *conn, const char *seq, const char *cmd,
			    char *host, int port,  struct browser_state *state)
{
  IMAP_DATA *idata = CONN_DATA;
  char buf[LONG_STRING];
  char *name;
  int noselect;
  int noinferiors;

  mutt_socket_write (conn, cmd);

  do 
  {
    if (imap_parse_list_response(conn, buf, sizeof(buf), &name,
	    &noselect, &noinferiors, &(idata->delim)) != 0)
      return (-1);
    if (name)
    {
      add_folder (idata->delim, name, host, port, 
		  noselect, noinferiors, state);
    }
  }
  while ((mutt_strncmp (buf, seq, SEQLEN) != 0));
  return (0);
}

static int get_namespace (IMAP_DATA *idata, char *nsbuf, int nsblen, 
			  IMAP_NAMESPACE_INFO *nsi, int nsilen, int *nns)
{
  char buf[LONG_STRING];
  char seq[16];
  char *s;
  int n;
  char ns[LONG_STRING];
  char delim = '/';
  int type;
  int nsbused = 0;

  *nns = 0;
  nsbuf[nsblen-1] = '\0';

  imap_make_sequence (seq, sizeof (seq));
  snprintf (buf, sizeof (buf), "%s NAMESPACE\r\n", seq);

  mutt_socket_write (idata->conn, buf);
  do 
  {
    if (mutt_socket_read_line_d (buf, sizeof (buf), idata->conn) < 0)
    {
      return (-1);
    }

    if (buf[0] == '*') 
    {
      s = imap_next_word (buf);
      if (mutt_strncasecmp ("NAMESPACE", s, 9) == 0)
      {
	/* There are three sections to the response, User, Other, Shared,
	 * and maybe more by extension */
	for (type = IMAP_NS_PERSONAL; *s; type++)
	{
	  s = imap_next_word (s);
	  if (*s && strncmp (s, "NIL", 3))
	  {
	    s++;
	    while (*s && *s != ')')
	    {
	      s++; /* skip ( */
	      /* copy namespace */
	      n = 0;
	      if (*s == '\"')
	      {
		s++;
		while (*s && *s != '\"') 
		{
		  if (*s == '\\')
		    s++;
		  ns[n++] = *s;
		  s++;
		}
	      }
	      else
		while (*s && !ISSPACE (*s)) 
		{
		  ns[n++] = *s;
		  s++;
		}
	      ns[n] = '\0';
	      /* delim? */
	      s = imap_next_word (s);
	      if (*s && *s == '\"')
		if (s[1] && s[2] == '\"')
		  delim = s[1];
		else if (s[1] && s[1] == '\\' && s[2] && s[3] == '\"')
		  delim = s[2];
	      /* Save result (if space) */
	      if ((nsbused < nsblen) && (*nns < nsilen))
	      {
		nsi->type = type;
		strncpy(nsbuf+nsbused,ns,nsblen-nsbused-1);
		nsi->prefix = nsbuf+nsbused;
		nsbused += n+1;
		nsi->delim = delim;
		nsi++;
		(*nns)++;
	      }
	      while (*s && *s != ')') s++;
	      s++;
	    }
	  }
	}
      }
      else
      {
	if (imap_handle_untagged (idata, buf) != 0)
	  return (-1);
      }
    }
  }
  while ((mutt_strncmp (buf, seq, SEQLEN) != 0));
  return 0;
}

/* Check which namespaces actually exist */
static int verify_namespace (CONNECTION *conn, IMAP_NAMESPACE_INFO *nsi, 
			     int nns)
{
  char buf[LONG_STRING];
  char seq[16];
  int i = 0;
  char *name;
  char delim;

  for (i = 0; i < nns; i++, nsi++)
  {
    imap_make_sequence (seq, sizeof (seq));
    snprintf (buf, sizeof (buf), "%s %s \"\" \"%s\"\r\n", seq, 
	      option (OPTIMAPLSUB) ? "LSUB" : "LIST", nsi->prefix);
    mutt_socket_write (conn, buf);

    nsi->listable = 0;
    nsi->home_namespace = 0;
    do 
    {
      if (imap_parse_list_response(conn, buf, sizeof(buf), &name,
				   &(nsi->noselect), &(nsi->noinferiors), 
				   &delim) != 0)
	return (-1);
      nsi->listable |= (name != NULL);
    }
    while ((mutt_strncmp (buf, seq, SEQLEN) != 0));
  }
  return (0);
}

static int compare_names(struct folder_file *a, struct folder_file *b) 
{
  return mutt_strcmp(a->name, b->name);
}

int imap_init_browse (char *path, struct browser_state *state)
{
  CONNECTION *conn;
  IMAP_DATA *idata;
  char buf[LONG_STRING];
  char nsbuf[LONG_STRING];
  char mbox[LONG_STRING];
  char host[SHORT_STRING];
  char seq[16];
  char *ipath = NULL;
  int n;
  int i;
  int nsup;
  int port;
  IMAP_NAMESPACE_INFO nsi[16];
  int nns;
  int home_namespace = 0;

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
      return (-1);
  }

  if (ipath[0] == '\0')
  {
    home_namespace = 1;
    mbox[0] = 0;		/* Do not replace "" with "INBOX" here */
    ipath = ImapHomeNamespace;
    nns = 0;
    if (mutt_bit_isset(idata->capabilities,NAMESPACE))
    {
      if (get_namespace (idata, nsbuf, sizeof (nsbuf), 
			 nsi, sizeof (nsi),  &nns) != 0)
	return (-1);
      if (verify_namespace (conn, nsi, nns) != 0)
	return (-1);
    }
    if (!ipath)		/* Any explicitly set imap_home_namespace wins */
    { 
      for (i = 0; i < nns; i++)
	if (nsi[i].listable &&
	    (nsi[i].type == IMAP_NS_PERSONAL || nsi[i].type == IMAP_NS_SHARED))
	{
	  ipath = nsi->prefix;
	  nsi->home_namespace = 1;
	  break;
	}
    }
  }
  if (ipath && ipath[0] != '\0')
  {
    imap_fix_path (idata, ipath, mbox, sizeof (mbox) - 1);
    n = mutt_strlen (mbox);
    if (mbox[n-1] != idata->delim)
    {
      mbox[n] = idata->delim;		
      n++;
      mbox[n] = 0;		
    }
    /* Find superiors to list */
    for (n = n-2; n >= 0 && mbox[n] != idata->delim ; n--);
    if (n > 0)			/* "aaaa/bbbb/" -> add "aaaa" */
    {
      mbox[n] = '\0';
      /* List it to see if it can be selected */
      imap_make_sequence (seq, sizeof (seq));
      snprintf (buf, sizeof (buf), "%s %s \"\" \"%s%%\"\r\n", seq, 
	  option (OPTIMAPLSUB) ? "LSUB" : "LIST", mbox);
      if (add_list_result(conn, seq, buf, host, port, state) != 0)
	return (-1);
      mbox[n] = idata->delim;
    } 
    else  /* "/bbbb/" -> add  "/", "aaaa/" -> add "" */
    {
      snprintf (buf, sizeof (buf), "%c" , n<0 ? '\0' :idata->delim);
      add_folder (idata->delim, buf, host, port, 1, 0, state); 
    }
  }

  if (home_namespace && mbox[0] != '\0')
  {
    /* Listing the home namespace, so INBOX should be included. Home 
     * namespace is not "", so we have to list it explicitly. We ask the 
     * server to see if it has descendants. */
    imap_make_sequence (seq, sizeof (seq));
    snprintf (buf, sizeof (buf), "%s LIST \"\" \"INBOX\"\r\n", seq);
    if (add_list_result(conn, seq, buf, host, port, state) != 0)
      return (-1);
  }

  nsup = state->entrylen;

  imap_make_sequence (seq, sizeof (seq));
  mutt_message _("Contacted server, getting folder list...");
  snprintf (buf, sizeof (buf), "%s %s \"\" \"%s%%\"\r\n", seq, 
      option (OPTIMAPLSUB) ? "LSUB" : "LIST", mbox);
  if (add_list_result(conn, seq, buf, host, port, state) != 0)
    return (-1);

  qsort(&(state->entry[nsup]),state->entrylen-nsup,sizeof(state->entry[0]),
	(int (*)(const void*,const void*)) compare_names);
  if (home_namespace)
  {				/* List additional namespaces */
    for (i = 0; i < nns; i++)
      if (nsi[i].listable && !nsi[i].home_namespace)
	add_folder(nsi[i].delim, nsi[i].prefix, host, port, 
		   nsi[i].noselect, nsi[i].noinferiors, state);
  }

  mutt_clear_error ();
  return 0;
}

int imap_subscribe (char *path, int subscribe)
{
  CONNECTION *conn;
  IMAP_DATA *idata;
  char buf[LONG_STRING];
  char mbox[LONG_STRING];
  char host[SHORT_STRING];
  char seq[16];
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
      return (-1);
  }

  imap_fix_path (idata, ipath, buf, sizeof (buf));
  if (subscribe)
    mutt_message (_("Subscribing to %s..."), buf);
  else
    mutt_message (_("Unsubscribing to %s..."), buf);
  imap_quote_string (mbox, sizeof(mbox), buf);
  imap_make_sequence (seq, sizeof (seq));

  snprintf (buf, sizeof (buf), "%s %s %s\r\n", seq, 
      subscribe ? "SUBSCRIBE" : "UNSUBSCRIBE", mbox);

  if (imap_exec (buf, sizeof (buf), idata, seq, buf, 0) < 0)
  {
    return (-1);
  }
  return 0;
}
