/*
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

/* message parsing/updating functions */

#include <stdlib.h>
#include <ctype.h>

#include "mutt.h"
#include "imap.h"
#include "imap_private.h"
#include "mx.h"

#ifdef _PGPPATH
#include "pgp.h"
#endif

static void flush_buffer(char *buf, size_t *len, CONNECTION *conn);
static int parse_fetch (IMAP_HEADER_INFO *h, char *s);

/* imap_read_headers:
 * Changed to read many headers instead of just one. It will return the
 * msgno of the last message read. It will return a value other than
 * msgend if mail comes in while downloading headers (in theory).
 */
int imap_read_headers (CONTEXT *ctx, int msgbegin, int msgend)
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
	    if (imap_get_literal_count(buf, &bytes) < 0)
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
    if (parse_fetch (h, fetchbuf) == -1)
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
    if (mutt_socket_read_line_d (buf, sizeof (buf), CTX_DATA->conn) < 0)
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
	while (*pc)
	{
	  pc = imap_next_word (pc);
	  if (pc[0] == '(')
	    pc++;
	  dprint (2, (debugfile, "Found FETCH word %s\n", pc));
	  if (strncasecmp ("RFC822", pc, 6) == 0)
	  {
	    pc = imap_next_word (pc);
	    if (imap_get_literal_count(pc, &bytes) < 0)
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
	    if (mutt_socket_read_line (buf, sizeof (buf), CTX_DATA->conn) < 0)
	    {
	      return (-1);
	    }
	    pc = buf;
	  }
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

/* parse_fetch: handle headers returned from header fetch */
static int parse_fetch (IMAP_HEADER_INFO *h, char *s)
{
  char tmp[SHORT_STRING];
  char *ptmp;
  int state = 0;
  int recent = 0;

  if (!s)
    return (-1);

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
	    dprint (1, (debugfile, "parse_fetch(): bogus FLAGS entry: %s\n", s));
	    return (-1); /* parse error */
	  }
	  /* we're about to get a new set of headers, so clear the old ones. */
	  h->deleted = 0;
          h->flagged = 0;
	  h->replied = 0;
          h->read = 0;
          h->old = 0;
	  h->changed = 0;
          recent = 0;
	  s++;
	  state = 1;
	}
	else if (mutt_strncasecmp ("INTERNALDATE", s, 12) == 0)
	{
	  s += 12;
	  SKIPWS (s);
	  if (*s != '\"')
	  {
	    dprint (1, (debugfile, "parse_fetch(): bogus INTERNALDATE entry: %s\n", s));
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
	  imap_error ("parse_fetch()", s);
	  return (-1);
	}
	break;
      case 1: /* flags */
	if (*s == ')')
	{
	  s++;
          /* if a message is neither seen nor recent, it is OLD. */
          if (option (OPTMARKOLD) && !recent && !(h->read))
            h->old = 1;
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
	else if (mutt_strncasecmp ("\\recent", s, 5) == 0)
	{
	  s += 7;
	  recent = 1;
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

static void flush_buffer(char *buf, size_t *len, CONNECTION *conn)
{
  buf[*len] = '\0';
  mutt_socket_write(conn, buf);
  *len = 0;
}
