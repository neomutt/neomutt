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
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */ 

/* message parsing/updating functions */

#include <stdlib.h>
#include <ctype.h>

#include "mutt.h"
#include "imap_private.h"
#include "message.h"
#include "mx.h"

#ifdef HAVE_PGP
#include "pgp.h"
#endif

static void flush_buffer(char* buf, size_t* len, CONNECTION* conn);
static int msg_has_flag (LIST* flag_list, const char* flag);
static IMAP_HEADER* msg_new_header (void);
static int msg_parse_fetch (IMAP_HEADER* h, char* s);
static char* msg_parse_flags (IMAP_HEADER* h, char* s);

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
  IMAP_HEADER *h, *h0;
  const char *want_headers = "DATE FROM SUBJECT TO CC MESSAGE-ID REFERENCES CONTENT-TYPE IN-REPLY-TO REPLY-TO LINES";
  int using_body_peek = 0;
  fetchlast = 0;

  /* define search string */
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
    return -1;
  }

  /*
   * We now download all of the headers into one file. This should be
   * faster on most systems.
   */
  mutt_mktemp (tempfile);
  if (!(fp = safe_fopen (tempfile, "w+")))
    return -1;

  unlink (tempfile);

  h = msg_new_header ();
  h0 = h;
  for (msgno = msgbegin; msgno <= msgend ; msgno++)
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
      snprintf (buf, sizeof (buf), 
	"%s FETCH %d:%d (UID FLAGS INTERNALDATE RFC822.SIZE %s)\r\n", 
	seq, msgno + 1, msgend + 1, hdrreq);

      mutt_socket_write (CTX_DATA->conn, buf);
      fetchlast = msgend + 1;
    }

    do
    {
      if (mutt_socket_read_line_d (buf, sizeof (buf), CTX_DATA->conn) < 0)
      {
	fclose (fp);
        return -1;
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
	    fclose (fp);
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
	      fclose (fp);
              return -1;
            }
            strncpy(fpc,pc,hdr-pc);
            fpc += hdr-pc;
            *fpc = '\0';
            pc=hdr;
            /* get some number of bytes */
	    if (imap_get_literal_count(buf, &bytes) < 0)
            {
              imap_error ("imap_read_headers()", buf);
	      fclose (fp);
              return -1;
            }
            imap_read_bytes (fp, CTX_DATA->conn, bytes);
            if (mutt_socket_read_line_d (buf, sizeof (buf), CTX_DATA->conn) < 0)
	    {
	      fclose (fp);
              return -1;
	    }
	    
            pc = buf;
          }
        }
        else if (imap_handle_untagged (CTX_DATA, buf) != 0)
	{
	  fclose (fp);
          return -1;
	}
      }
    }
    while ((msgno + 1) >= fetchlast && mutt_strncmp (seq, buf, SEQLEN) != 0);

    h->content_length = -bytes;
    if (msg_parse_fetch (h, fetchbuf) == -1)
    {
      fclose (fp);
      return -1;
    }
    
    /* FIXME?: subtract the header length; the total message size will be
       added to this */

    /* in case we get new mail while fetching the headers */
    if (((IMAP_DATA *) ctx->data)->status == IMAP_NEW_MAIL)
    {
      msgend = ((IMAP_DATA *) ctx->data)->newMailCount - 1;
      while ((msgend + 1) > ctx->hdrmax)
        mx_alloc_memory (ctx);
      ((IMAP_DATA *) ctx->data)->status = 0;
    }

    h->next = msg_new_header ();
    h = h->next;
  }

  rewind (fp);
  h = h0;

  /*
   * Now that we have all the header information, we can tell mutt about
   * it.
   */
  ploc=0;
  for (msgno = msgbegin; msgno <= msgend;msgno++)
  {
    ctx->hdrs[ctx->msgcount] = mutt_new_header ();
    ctx->hdrs[ctx->msgcount]->index = ctx->msgcount;

    ctx->hdrs[msgno]->env = mutt_read_rfc822_header (fp, ctx->hdrs[msgno], 0, 0);
    ploc = ftell (fp);
    ctx->hdrs[msgno]->read = h->read;
    ctx->hdrs[msgno]->old = h->old;
    ctx->hdrs[msgno]->deleted = h->deleted;
    ctx->hdrs[msgno]->flagged = h->flagged;
    ctx->hdrs[msgno]->replied = h->replied;
    ctx->hdrs[msgno]->changed = h->changed;
    ctx->hdrs[msgno]->received = h->received;
    ctx->hdrs[msgno]->content->length = h->content_length;
    ctx->hdrs[msgno]->data = (void *) (h->data);

    mx_update_context (ctx); /* increments ->msgcount */

    h0 = h;
    h = h->next;
    /* hdata is freed later */
    safe_free ((void **) &h0);
  }
  
  fclose(fp);
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
#if 0
  snprintf (buf, sizeof (buf), "%s FETCH %d RFC822\r\n", seq,
	    ctx->hdrs[msgno]->index + 1);
#else
  snprintf (buf, sizeof (buf), "%s UID FETCH %d RFC822\r\n", seq,
	    HEADER_DATA(ctx->hdrs[msgno])->uid);
#endif
  mutt_socket_write (CTX_DATA->conn, buf);
  do
  {
    if (mutt_socket_read_line_d (buf, sizeof (buf), CTX_DATA->conn) < 0)
      goto bail;

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
	      goto bail;
	    }
	    for (pos = 0; pos < bytes; )
	    {
	      len = mutt_socket_read_line (buf, sizeof (buf), CTX_DATA->conn);
	      if (len < 0)
		goto bail;
	      pos += len;
	      fputs (buf, msg->fp);
	      fputs ("\n", msg->fp);
	    }
	    if (mutt_socket_read_line (buf, sizeof (buf), CTX_DATA->conn) < 0)
	      goto bail;
	    pc = buf;
	  }
          /* UW-IMAP will provide a FLAGS update here if the FETCH causes a
           * change (eg from \Unseen to \Seen).
           * Uncommitted changes in mutt take precedence. If we decide to
           * incrementally update flags later, this won't stop us syncing */
          else if ((strncasecmp ("FLAGS", pc, 5) == 0) &&
            !ctx->hdrs[msgno]->changed)
          {
	    IMAP_HEADER* newh;
            HEADER* h = ctx->hdrs[msgno];

            newh = msg_new_header ();

            dprint (2, (debugfile, "imap_fetch_message: parsing FLAGS\n"));
            if ((pc = msg_parse_flags (newh, pc)) == NULL)
	      goto bail;
	      
	    /* this is less efficient than the code which used to be here,
	     * but (1) this is only invoked when fetching messages, and (2)
	     * this way, we can make sure that side effects of flag changes
	     * are taken account of the proper way.
	     */

	    mutt_set_flag (ctx, h, M_NEW, 
		    !(newh->read || newh->old || h->read || h->old));
	    mutt_set_flag (ctx, h, M_OLD, newh->old);
	    mutt_set_flag (ctx, h, M_READ, h->read || newh->read);
	    mutt_set_flag (ctx, h, M_DELETE, h->deleted || newh->deleted);
	    mutt_set_flag (ctx, h, M_FLAG, h->flagged || newh->flagged);
	    mutt_set_flag (ctx, h, M_REPLIED, h->replied || newh->replied);

            /* this message is now definitively *not* changed (mutt_set_flag
             * marks things changed as a side-effect) */
            h->changed = 0;

            mutt_free_list (&(HEADER_DATA(h)->keywords));
            HEADER_DATA(h)->keywords = newh->data->keywords;
            safe_free ((void**) &newh);
          }
	}
      }
      else if (imap_handle_untagged (CTX_DATA, buf) != 0)
	goto bail;
    }
  }
  while (mutt_strncmp (buf, seq, SEQLEN) != 0);

  if (!imap_code (buf))
    goto bail;
    
  
  /* Update the header information.  Previously, we only downloaded a
   * portion of the headers, those required for the main display.
   */
  rewind (msg->fp);
  mutt_free_envelope (&ctx->hdrs[msgno]->env);
  ctx->hdrs[msgno]->env = mutt_read_rfc822_header (msg->fp, ctx->hdrs[msgno],0, 0);
  ctx->hdrs[msgno]->lines = 0;
  fgets (buf, sizeof (buf), msg->fp);
  while (!feof (msg->fp))
  {
    ctx->hdrs[msgno]->lines++;
    fgets (buf, sizeof (buf), msg->fp);
  }

  ctx->hdrs[msgno]->content->length = ftell (msg->fp) - 
                                        ctx->hdrs[msgno]->content->offset;

  /* This needs to be done in case this is a multipart message */
#ifdef HAVE_PGP
  ctx->hdrs[msgno]->pgp = pgp_query (ctx->hdrs[msgno]->content);
#endif /* HAVE_PGP */

  mutt_clear_error();
  rewind (msg->fp);

  return 0;

bail:
  safe_fclose (&msg->fp);
  if (cache->path)
  {
    unlink (cache->path);
    FREE (&cache->path);
  }
  return (-1);
}

int imap_append_message (CONTEXT *ctx, MESSAGE *msg)
{
  FILE *fp;
  char buf[LONG_STRING];
  char mbox[LONG_STRING];
  char mailbox[LONG_STRING]; 
  char seq[16];
  size_t len;
  int c, last;
  IMAP_MBOX mx;

  if (imap_parse_path (ctx->path, &mx))
    return (-1);

  imap_fix_path (CTX_DATA, mx.mbox, mailbox, sizeof (mailbox));
  
  if ((fp = fopen (msg->path, "r")) == NULL)
  {
    mutt_perror (msg->path);
    return (-1);
  }

  for (last = EOF, len = 0; (c = fgetc(fp)) != EOF; last = c)
  {
    if(c == '\n' && last != '\r')
      len++;

    len++;
  }
  rewind (fp);
  
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

  for (last = EOF, len = 0; (c = fgetc(fp)) != EOF; last = c)
  {
    if (c == '\n' && last != '\r')
      buf[len++] = '\r';

    buf[len++] = c;

    if (len > sizeof(buf) - 3)
      flush_buffer(buf, &len, CTX_DATA->conn);
  }
  
  if (len)
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

/* imap_copy_messages: use server COPY command to copy messages to another
 *   folder.
 *   Return codes:
 *      -1: error
 *       0: success
 *       1: non-fatal error - try fetch/append */
int imap_copy_messages (CONTEXT* ctx, HEADER* h, char* dest, int delete)
{
  char buf[HUGE_STRING];
  char cmd[LONG_STRING];
  char mbox[LONG_STRING];
  int rc;
  int n;
  IMAP_MBOX mx;

  if (imap_parse_path (dest, &mx))
  {
    dprint (1, (debugfile, "imap_copy_message: bad destination %s\n", dest));
    return -1;
  }

  /* check that the save-to folder is on the same server */
  if (mutt_socket_select_connection (&mx, 0) != CTX_DATA->conn)
  {
    dprint (3, (debugfile, "imap_copy_message: %s not same server as %s\n",
      dest, ctx->path));
    return 1;
  }

  imap_fix_path (CTX_DATA, mx.mbox, cmd, sizeof (cmd));

  /* Null HEADER* means copy tagged messages */
  if (!h)
  {
    rc = imap_make_msg_set (buf, sizeof (buf), ctx, M_TAG, 0);
    if (!rc)
    {
      dprint (1, (debugfile, "imap_copy_messages: No messages tagged\n"));
      return -1;
    }
    mutt_message (_("Copying %d messages to %s..."), rc, cmd);
  }
  else
  {
    mutt_message (_("Copying message %d to %s..."), h->index+1, cmd);
    snprintf (buf, sizeof (buf), "%d", h->index+1);
  }

  /* let's get it on */
  strncpy (mbox, cmd, sizeof (mbox));
  snprintf (cmd, sizeof (cmd), "COPY %s \"%s\"", buf, mbox);
  rc = imap_exec (buf, sizeof (buf), CTX_DATA, cmd, IMAP_OK_FAIL);
  if (rc == -2)
  {
    /* bail out if command failed for reasons other than nonexistent target */
    if (strncmp (imap_get_qualifier (buf), "[TRYCREATE]", 11))
    {
      imap_error ("imap_copy_messages", buf);
      return -1;
    }
    dprint (2, (debugfile, "imap_copy_messages: server suggests TRYCREATE\n"));
    snprintf (buf, sizeof (buf), _("Create %s?"), mbox);
    if (option (OPTCONFIRMCREATE) && mutt_yesorno (buf, 1) < 1)
    {
      mutt_clear_error ();
      return -1;
    }
    if (imap_create_mailbox (ctx, mbox) < 0)
      return -1;

    /* try again */
    rc = imap_exec (buf, sizeof (buf), CTX_DATA, cmd, 0);
  }
  if (rc != 0)
  {
    imap_error ("imap_copy_messages", buf);
    return -1;
  }

  /* cleanup */
  if (delete)
  {
    if (!h)
      for (n = 0; n < ctx->msgcount; n++)
      {
        if (ctx->hdrs[n]->tagged)
        {
          mutt_set_flag (ctx, ctx->hdrs[n], M_DELETE, 1);
          if (option (OPTDELETEUNTAG))
            mutt_set_flag (ctx, ctx->hdrs[n], M_TAG, 0);
        }
      }
    else
    {
      mutt_set_flag (ctx, h, M_DELETE, 1);
      if (option (OPTDELETEUNTAG))
        mutt_set_flag (ctx, h, M_TAG, 0);
    }
  }

  return 0;
}

/* imap_add_keywords: concatenate custom IMAP tags to list, if they
 *   appear in the folder flags list. Why wouldn't they? */
void imap_add_keywords (char* s, HEADER* h, LIST* mailbox_flags)
{
  LIST *keywords;

  if (!mailbox_flags || !HEADER_DATA(h) || !HEADER_DATA(h)->keywords)
    return;

  keywords = HEADER_DATA(h)->keywords->next;

  while (keywords)
  {
    if (msg_has_flag (mailbox_flags, keywords->data))
    {
      strcat (s, keywords->data);
      strcat (s, " ");
    }
    keywords = keywords->next;
  }
}

/* imap_free_header_data: free IMAP_HEADER structure */
void imap_free_header_data (void** data)
{
  /* this should be safe even if the list wasn't used */
  mutt_free_list (&(((IMAP_HEADER_DATA*) *data)->keywords));

  safe_free (data);
}

/* msg_new_header: allocate and initialise a new IMAP_HEADER structure */
static IMAP_HEADER* msg_new_header (void)
{
  IMAP_HEADER* h;

  h = (IMAP_HEADER*) safe_malloc (sizeof (IMAP_HEADER));
  h->data = (IMAP_HEADER_DATA*) safe_malloc (sizeof (IMAP_HEADER_DATA));

  /* lists aren't allocated unless they're used */
  h->data->keywords = NULL;

  h->deleted = 0;
  h->flagged = 0;
  h->replied = 0;
  h->read = 0;
  h->old = 0;
  h->changed = 0;

  return h;
}

/* msg_has_flag: do a caseless comparison of the flag against a flag list,
 *   return 1 if found or flag list has '\*', 0 otherwise */
static int msg_has_flag (LIST* flag_list, const char* flag)
{
  if (!flag_list)
    return 0;

  flag_list = flag_list->next;
  while (flag_list)
  {
    if (!mutt_strncasecmp (flag_list->data, flag, strlen (flag_list->data)))
      return 1;

    flag_list = flag_list->next;
  }

  return 0;
}

/* msg_parse_fetch: handle headers returned from header fetch */
static int msg_parse_fetch (IMAP_HEADER *h, char *s)
{
  char tmp[SHORT_STRING];
  char *ptmp;

  if (!s)
    return (-1);

  while (*s)
  {
    SKIPWS (s);

    if (mutt_strncasecmp ("FLAGS", s, 5) == 0)
    {
      if ((s = msg_parse_flags (h, s)) == NULL)
        return -1;
    }
    else if (mutt_strncasecmp ("UID", s, 3) == 0)
    {
      s += 3;
      SKIPWS (s);
      h->data->uid = (unsigned int) atoi (s);

      s = imap_next_word (s);
    }
    else if (mutt_strncasecmp ("INTERNALDATE", s, 12) == 0)
    {
      s += 12;
      SKIPWS (s);
      if (*s != '\"')
      {
        dprint (1, (debugfile, "msg_parse_fetch(): bogus INTERNALDATE entry: %s\n", s));
        return -1;
      }
      s++;
      ptmp = tmp;
      while (*s && *s != '\"')
        *ptmp++ = *s++;
      if (*s != '\"')
        return -1;
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
      imap_error ("msg_parse_fetch", s);
      return (-1);
    }
  }

  return 0;
}

/* msg_parse_flags: fill out the message header according to the flags from the
 *   server. Expects a flags line of the form "FLAGS (flag flag ...)" */
static char* msg_parse_flags (IMAP_HEADER* h, char* s)
{
  int recent = 0;

  /* sanity-check string */
  if (mutt_strncasecmp ("FLAGS", s, 5) != 0)
  {
    dprint (1, (debugfile, "msg_parse_flags: not a FLAGS response: %s\n",
      s));
    return NULL;
  }
  s += 5;
  SKIPWS(s);
  if (*s != '(')
  {
    dprint (1, (debugfile, "msg_parse_flags: bogus FLAGS response: %s\n",
      s));
    return NULL;
  }
  s++;

  /* start parsing */
  while (*s && *s != ')')
  {
    if (mutt_strncasecmp ("\\deleted", s, 8) == 0)
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
      /* store custom flags as well */
      char ctmp;
      char* flag_word = s;

      if (!h->data->keywords)
        h->data->keywords = mutt_new_list ();

      while (*s && !ISSPACE (*s) && *s != ')')
        s++;
      ctmp = *s;
      *s = '\0';
      mutt_add_list (h->data->keywords, flag_word);
      *s = ctmp;
    }
    SKIPWS(s);
  }

  /* wrap up, or note bad flags response */
  if (*s == ')')
  {
    /* if a message is neither seen nor recent, it is OLD. */
    if (option (OPTMARKOLD) && !recent && !(h->read))
      h->old = 1;
    s++;
  }
  else
  {
    dprint (1, (debugfile,
      "msg_parse_flags: Unterminated FLAGS response: %s\n", s));
    return NULL;
  }

  return s;
}

static void flush_buffer(char *buf, size_t *len, CONNECTION *conn)
{
  buf[*len] = '\0';
  mutt_socket_write(conn, buf);
  *len = 0;
}
