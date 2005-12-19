/*
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
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */ 

/* message parsing/updating functions */

#if HAVE_CONFIG_H
# include "config.h"
#endif

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
static int msg_fetch_header (CONTEXT* ctx, IMAP_HEADER* h, char* buf,
  FILE* fp);
static int msg_parse_fetch (IMAP_HEADER* h, char* s);
static char* msg_parse_flags (IMAP_HEADER* h, char* s);

#if USE_HCACHE
static size_t imap_hcache_keylen (const char *fn);
#endif /* USE_HCACHE */

/* imap_read_headers:
 * Changed to read many headers instead of just one. It will return the
 * msgno of the last message read. It will return a value other than
 * msgend if mail comes in while downloading headers (in theory).
 */
int imap_read_headers (IMAP_DATA* idata, int msgbegin, int msgend)
{
  CONTEXT* ctx;
  char buf[LONG_STRING];
  char hdrreq[STRING];
  FILE *fp;
  char tempfile[_POSIX_PATH_MAX];
  int msgno;
  IMAP_HEADER h;
  IMAP_STATUS* status;
  int rc, mfhrc, oldmsgcount;
  int fetchlast = 0;
  int maxuid = 0;
  const char *want_headers = "DATE FROM SUBJECT TO CC MESSAGE-ID REFERENCES CONTENT-TYPE CONTENT-DESCRIPTION IN-REPLY-TO REPLY-TO LINES LIST-POST X-LABEL";

#if USE_HCACHE
  void *hc   = NULL;
  unsigned long *uid_validity = NULL;
  char uid_buf[64];
#endif /* USE_HCACHE */

  ctx = idata->ctx;

  if (mutt_bit_isset (idata->capabilities,IMAP4REV1))
  {
    snprintf (hdrreq, sizeof (hdrreq), "BODY.PEEK[HEADER.FIELDS (%s%s%s)]", 
	      want_headers, ImapHeaders ? " " : "", ImapHeaders ? ImapHeaders : ""); 
  } 
  else if (mutt_bit_isset (idata->capabilities,IMAP4))
  {
    snprintf (hdrreq, sizeof (hdrreq), "RFC822.HEADER.LINES (%s%s%s)", 
	      want_headers, ImapHeaders ? " " : "", ImapHeaders ? ImapHeaders : "");
  }
  else
  {	/* Unable to fetch headers for lower versions */
    mutt_error _("Unable to fetch headers from this IMAP server version.");
    mutt_sleep (2);	/* pause a moment to let the user see the error */
    return -1;
  }

  /* instead of downloading all headers and then parsing them, we parse them
   * as they come in. */
  mutt_mktemp (tempfile);
  if (!(fp = safe_fopen (tempfile, "w+")))
  {
    mutt_error (_("Could not create temporary file %s"), tempfile);
    mutt_sleep (2);
    return -1;
  }
  unlink (tempfile);

  /* make sure context has room to hold the mailbox */
  while ((msgend) >= idata->ctx->hdrmax)
    mx_alloc_memory (idata->ctx);

  oldmsgcount = ctx->msgcount;
  idata->reopen &= ~IMAP_NEWMAIL_PENDING;
  idata->newMailCount = 0;

#if USE_HCACHE
  hc = mutt_hcache_open (HeaderCache, ctx->path);

  if (hc) {
    snprintf (buf, sizeof (buf),
      "FETCH %d:%d (UID FLAGS)", msgbegin + 1, msgend + 1);
    fetchlast = msgend + 1;
  
    imap_cmd_start (idata, buf);
  
    for (msgno = msgbegin; msgno <= msgend ; msgno++)
    {
      if (ReadInc && (!msgno || ((msgno+1) % ReadInc == 0)))
        mutt_message (_("Evaluating cache... [%d/%d]"), msgno + 1,
          msgend + 1);
  
      memset (&h, 0, sizeof (h));
      h.data = safe_calloc (1, sizeof (IMAP_HEADER_DATA));
      do
      {
        mfhrc = 0;

        rc = imap_cmd_step (idata);
        if (rc != IMAP_CMD_CONTINUE)
          break;

        if ((mfhrc = msg_fetch_header (idata->ctx, &h, idata->buf, NULL)) == -1)
          continue;
        else if (mfhrc < 0)
          break;

        /* make sure we don't get remnants from older larger message headers */
        fputs ("\n\n", fp);

        sprintf(uid_buf, "/%u", h.data->uid); /* XXX --tg 21:41 04-07-11 */
        uid_validity = (unsigned long *) mutt_hcache_fetch (hc, uid_buf, &imap_hcache_keylen);

        if (uid_validity != NULL && *uid_validity == idata->uid_validity)
        {
  	  ctx->hdrs[msgno] = mutt_hcache_restore((unsigned char *) uid_validity, 0);
  	  ctx->hdrs[msgno]->index = h.sid - 1;
  	  if (h.sid != ctx->msgcount + 1)
  	    dprint (1, (debugfile, "imap_read_headers: msgcount and sequence ID are inconsistent!"));
  	  /* messages which have not been expunged are ACTIVE (borrowed from mh 
  	   * folders) */
  	  ctx->hdrs[msgno]->active = 1;
          ctx->hdrs[msgno]->read = h.data->read;
          ctx->hdrs[msgno]->old = h.data->old;
          ctx->hdrs[msgno]->deleted = h.data->deleted;
          ctx->hdrs[msgno]->flagged = h.data->flagged;
          ctx->hdrs[msgno]->replied = h.data->replied;
          ctx->hdrs[msgno]->changed = h.data->changed;
          /*  ctx->hdrs[msgno]->received is restored from mutt_hcache_restore */
          ctx->hdrs[msgno]->data = (void *) (h.data);

          ctx->msgcount++;
        }
  
        FREE(&uid_validity);
      }
      while ((rc != IMAP_CMD_OK) && ((mfhrc == -1) ||
        ((msgno + 1) >= fetchlast)));
  
      if ((mfhrc < -1) || ((rc != IMAP_CMD_CONTINUE) && (rc != IMAP_CMD_OK)))
      {
        imap_free_header_data ((void**) &h.data);
        fclose (fp);
        mutt_hcache_close (hc);
        return -1;
      }
    }
  
    fetchlast = msgbegin;
  }
#endif /* USE_HCACHE */

  for (msgno = msgbegin; msgno <= msgend ; msgno++)
  {
    if (ReadInc && (!msgno || ((msgno+1) % ReadInc == 0)))
      mutt_message (_("Fetching message headers... [%d/%d]"), msgno + 1,
        msgend + 1);

    if (ctx->hdrs[msgno])
      continue;

    if (msgno + 1 > fetchlast)
    {
      fetchlast = msgno + 1;
      while((fetchlast <= msgend) && (! ctx->hdrs[fetchlast]))
        fetchlast++;

      /*
       * Make one request for everything. This makes fetching headers an
       * order of magnitude faster if you have a large mailbox.
       *
       * If we get more messages while doing this, we make another
       * request for all the new messages.
       */
      snprintf (buf, sizeof (buf),
        "FETCH %d:%d (UID FLAGS INTERNALDATE RFC822.SIZE %s)", msgno + 1,
        fetchlast, hdrreq);

      imap_cmd_start (idata, buf);
    }

    /* freshen fp, h */
    rewind (fp);
    memset (&h, 0, sizeof (h));
    h.data = safe_calloc (1, sizeof (IMAP_HEADER_DATA));

    /* this DO loop does two things:
     * 1. handles untagged messages, so we can try again on the same msg
     * 2. fetches the tagged response at the end of the last message.
     */
    do
    {
      mfhrc = 0;

      rc = imap_cmd_step (idata);
      if (rc != IMAP_CMD_CONTINUE)
	break;

      if ((mfhrc = msg_fetch_header (idata->ctx, &h, idata->buf, fp)) == -1)
	continue;
      else if (mfhrc < 0)
	break;

      /* make sure we don't get remnants from older larger message headers */
      fputs ("\n\n", fp);

      /* update context with message header */
      ctx->hdrs[msgno] = mutt_new_header ();

      ctx->hdrs[msgno]->index = h.sid - 1;
      if (h.sid != ctx->msgcount + 1)
	dprint (1, (debugfile, "imap_read_headers: msgcount and sequence ID are inconsistent!"));
      /* messages which have not been expunged are ACTIVE (borrowed from mh 
       * folders) */
      ctx->hdrs[msgno]->active = 1;
      ctx->hdrs[msgno]->read = h.data->read;
      ctx->hdrs[msgno]->old = h.data->old;
      ctx->hdrs[msgno]->deleted = h.data->deleted;
      ctx->hdrs[msgno]->flagged = h.data->flagged;
      ctx->hdrs[msgno]->replied = h.data->replied;
      ctx->hdrs[msgno]->changed = h.data->changed;
      ctx->hdrs[msgno]->received = h.received;
      ctx->hdrs[msgno]->data = (void *) (h.data);

      if (maxuid < h.data->uid)
        maxuid = h.data->uid;

      rewind (fp);
      /* NOTE: if Date: header is missing, mutt_read_rfc822_header depends
       *   on h.received being set */
      ctx->hdrs[msgno]->env = mutt_read_rfc822_header (fp, ctx->hdrs[msgno],
        0, 0);
      /* content built as a side-effect of mutt_read_rfc822_header */
      ctx->hdrs[msgno]->content->length = h.content_length;

#if USE_HCACHE
      sprintf(uid_buf, "/%u", h.data->uid);
      mutt_hcache_store(hc, uid_buf, ctx->hdrs[msgno], idata->uid_validity, &imap_hcache_keylen);
#endif /* USE_HCACHE */

      ctx->msgcount++;
    }
    while ((rc != IMAP_CMD_OK) && ((mfhrc == -1) ||
      ((msgno + 1) >= fetchlast)));

    if ((mfhrc < -1) || ((rc != IMAP_CMD_CONTINUE) && (rc != IMAP_CMD_OK)))
    {
      imap_free_header_data ((void**) &h.data);
      fclose (fp);
#if USE_HCACHE
      mutt_hcache_close (hc);
#endif /* USE_HCACHE */
      return -1;
    }

    /* in case we get new mail while fetching the headers */
    if (idata->reopen & IMAP_NEWMAIL_PENDING)
    {
      msgend = idata->newMailCount - 1;
      while ((msgend) >= ctx->hdrmax)
	mx_alloc_memory (ctx);
      idata->reopen &= ~IMAP_NEWMAIL_PENDING;
      idata->newMailCount = 0;
    }
  }

#if USE_HCACHE
  mutt_hcache_close (hc);
#endif /* USE_HCACHE */

  fclose(fp);

  if (ctx->msgcount > oldmsgcount)
  {
    mx_alloc_memory(ctx);
    mx_update_context (ctx, ctx->msgcount - oldmsgcount);
  }

  if (maxuid && (status = imap_mboxcache_get (idata, idata->mailbox)))
    status->uidnext = maxuid + 1;

  return msgend;
}

int imap_fetch_message (MESSAGE *msg, CONTEXT *ctx, int msgno)
{
  IMAP_DATA* idata;
  HEADER* h;
  ENVELOPE* newenv;
  char buf[LONG_STRING];
  char path[_POSIX_PATH_MAX];
  char *pc;
  long bytes;
  progress_t progressbar;
  int uid;
  int cacheno;
  IMAP_CACHE *cache;
  int read;
  int rc;
  /* Sam's weird courier server returns an OK response even when FETCH
   * fails. Thanks Sam. */
  short fetched = 0;

  idata = (IMAP_DATA*) ctx->data;
  h = ctx->hdrs[msgno];

  /* see if we already have the message in our cache */
  cacheno = HEADER_DATA(h)->uid % IMAP_CACHE_LEN;
  cache = &idata->cache[cacheno];

  if (cache->path)
  {
    /* don't treat cache errors as fatal, just fall back. */
    if (cache->uid == HEADER_DATA(h)->uid &&
        (msg->fp = fopen (cache->path, "r")))
      return 0;
    else
    {
      unlink (cache->path);
      FREE (&cache->path);
    }
  }

  if (!isendwin())
    mutt_message _("Fetching message...");

  cache->uid = HEADER_DATA(h)->uid;
  mutt_mktemp (path);
  cache->path = safe_strdup (path);
  if (!(msg->fp = safe_fopen (path, "w+")))
  {
    FREE (&cache->path);
    return -1;
  }

  /* mark this header as currently inactive so the command handler won't
   * also try to update it. HACK until all this code can be moved into the
   * command handler */
  h->active = 0;
  
  snprintf (buf, sizeof (buf), "UID FETCH %u %s", HEADER_DATA(h)->uid,
	    (mutt_bit_isset (idata->capabilities, IMAP4REV1) ?
	     (option (OPTIMAPPEEK) ? "BODY.PEEK[]" : "BODY[]") :
	     "RFC822"));

  imap_cmd_start (idata, buf);
  do
  {
    if ((rc = imap_cmd_step (idata)) != IMAP_CMD_CONTINUE)
      break;

    pc = idata->buf;
    pc = imap_next_word (pc);
    pc = imap_next_word (pc);

    if (!ascii_strncasecmp ("FETCH", pc, 5))
    {
      while (*pc)
      {
	pc = imap_next_word (pc);
	if (pc[0] == '(')
	  pc++;
	if (ascii_strncasecmp ("UID", pc, 3) == 0)
	{
	  pc = imap_next_word (pc);
	  uid = atoi (pc);
	  if (uid != HEADER_DATA(h)->uid)
	    mutt_error (_("The message index is incorrect. Try reopening the mailbox."));
	}
	else if ((ascii_strncasecmp ("RFC822", pc, 6) == 0) ||
		 (ascii_strncasecmp ("BODY[]", pc, 6) == 0))
	{
	  pc = imap_next_word (pc);
	  if (imap_get_literal_count(pc, &bytes) < 0)
	  {
	    imap_error ("imap_fetch_message()", buf);
	    goto bail;
	  }
          progressbar.size = bytes;
          progressbar.msg = _("Fetching message...");
          mutt_progress_bar (&progressbar, 0);
	  if (imap_read_literal (msg->fp, idata, bytes, &progressbar) < 0)
	    goto bail;
	  /* pick up trailing line */
	  if ((rc = imap_cmd_step (idata)) != IMAP_CMD_CONTINUE)
	    goto bail;
	  pc = idata->buf;

	  fetched = 1;
	}
	/* UW-IMAP will provide a FLAGS update here if the FETCH causes a
	 * change (eg from \Unseen to \Seen).
	 * Uncommitted changes in mutt take precedence. If we decide to
	 * incrementally update flags later, this won't stop us syncing */
	else if ((ascii_strncasecmp ("FLAGS", pc, 5) == 0) && !h->changed)
	{
	  if ((pc = imap_set_flags (idata, h, pc)) == NULL)
	    goto bail;
	}
      }
    }
  }
  while (rc == IMAP_CMD_CONTINUE);

  /* see comment before command start. */
  h->active = 1;

  fflush (msg->fp);
  if (ferror (msg->fp))
  {
    mutt_perror (cache->path);
    goto bail;
  }
  
  if (rc != IMAP_CMD_OK)
    goto bail;

  if (!fetched || !imap_code (idata->buf))
    goto bail;
    
  /* Update the header information.  Previously, we only downloaded a
   * portion of the headers, those required for the main display.
   */
  rewind (msg->fp);
  /* It may be that the Status header indicates a message is read, but the
   * IMAP server doesn't know the message has been \Seen. So we capture
   * the server's notion of 'read' and if it differs from the message info
   * picked up in mutt_read_rfc822_header, we mark the message (and context
   * changed). Another possiblity: ignore Status on IMAP?*/
  read = h->read;
  newenv = mutt_read_rfc822_header (msg->fp, h, 0, 0);
  mutt_merge_envelopes(h->env, &newenv);

  /* see above. We want the new status in h->read, so we unset it manually
   * and let mutt_set_flag set it correctly, updating context. */
  if (read != h->read)
  {
    h->read = read;
    mutt_set_flag (ctx, h, M_NEW, read);
  }

  h->lines = 0;
  fgets (buf, sizeof (buf), msg->fp);
  while (!feof (msg->fp))
  {
    h->lines++;
    fgets (buf, sizeof (buf), msg->fp);
  }

  h->content->length = ftell (msg->fp) - h->content->offset;

  /* This needs to be done in case this is a multipart message */
#if defined(HAVE_PGP) || defined(HAVE_SMIME)
  h->security = crypt_query (h->content);
#endif

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

  return -1;
}

int imap_append_message (CONTEXT *ctx, MESSAGE *msg)
{
  IMAP_DATA* idata;
  FILE *fp;
  char buf[LONG_STRING];
  char mbox[LONG_STRING];
  char mailbox[LONG_STRING]; 
  size_t len;
  progress_t progressbar;
  size_t sent;
  int c, last;
  IMAP_MBOX mx;
  int rc;

  idata = (IMAP_DATA*) ctx->data;

  if (imap_parse_path (ctx->path, &mx))
    return -1;

  imap_fix_path (idata, mx.mbox, mailbox, sizeof (mailbox));
  
  if ((fp = fopen (msg->path, "r")) == NULL)
  {
    mutt_perror (msg->path);
    goto fail;
  }

  /* currently we set the \Seen flag on all messages, but probably we
   * should scan the message Status header for flag info. Since we're
   * already rereading the whole file for length it isn't any more
   * expensive (it'd be nice if we had the file size passed in already
   * by the code that writes the file, but that's a lot of changes.
   * Ideally we'd have a HEADER structure with flag info here... */
  for (last = EOF, len = 0; (c = fgetc(fp)) != EOF; last = c)
  {
    if(c == '\n' && last != '\r')
      len++;

    len++;
  }
  rewind (fp);

  progressbar.msg = _("Uploading message...");
  progressbar.size = len;
  mutt_progress_bar (&progressbar, 0);
  
  imap_munge_mbox_name (mbox, sizeof (mbox), mailbox);
  snprintf (buf, sizeof (buf), "APPEND %s (%s%s%s%s%s) {%lu}", mbox,
	    msg->flags.read    ? "\\Seen"      : "",
	    msg->flags.read && (msg->flags.replied || msg->flags.flagged) ? " " : "",
	    msg->flags.replied ? "\\Answered" : "",
	    msg->flags.replied && msg->flags.flagged ? " " : "",
	    msg->flags.flagged ? "\\Flagged"  : "",
	    (unsigned long) len);

  imap_cmd_start (idata, buf);

  do
    rc = imap_cmd_step (idata);
  while (rc == IMAP_CMD_CONTINUE);

  if (rc != IMAP_CMD_RESPOND)
  {
    char *pc;

    dprint (1, (debugfile, "imap_append_message(): command failed: %s\n",
		idata->buf));

    pc = idata->buf + SEQLEN;
    SKIPWS (pc);
    pc = imap_next_word (pc);
    mutt_error ("%s", pc);
    mutt_sleep (1);
    fclose (fp);
    goto fail;
  }

  for (last = EOF, sent = len = 0; (c = fgetc(fp)) != EOF; last = c)
  {
    if (c == '\n' && last != '\r')
      buf[len++] = '\r';

    buf[len++] = c;

    if (len > sizeof(buf) - 3)
    {
      sent += len;
      flush_buffer(buf, &len, idata->conn);
      mutt_progress_bar (&progressbar, sent);
    }
  }
  
  if (len)
    flush_buffer(buf, &len, idata->conn);

  mutt_socket_write (idata->conn, "\r\n");
  fclose (fp);

  do
    rc = imap_cmd_step (idata);
  while (rc == IMAP_CMD_CONTINUE);

  if (!imap_code (idata->buf))
  {
    char *pc;

    dprint (1, (debugfile, "imap_append_message(): command failed: %s\n",
		idata->buf));
    pc = idata->buf + SEQLEN;
    SKIPWS (pc);
    pc = imap_next_word (pc);
    mutt_error ("%s", pc);
    mutt_sleep (1);
    goto fail;
  }

  FREE (&mx.mbox);
  return 0;

 fail:
  FREE (&mx.mbox);
  return -1;
}

/* imap_copy_messages: use server COPY command to copy messages to another
 *   folder.
 *   Return codes:
 *      -1: error
 *       0: success
 *       1: non-fatal error - try fetch/append */
int imap_copy_messages (CONTEXT* ctx, HEADER* h, char* dest, int delete)
{
  IMAP_DATA* idata;
  BUFFER cmd, sync_cmd;
  char uid[11];
  char mbox[LONG_STRING];
  char mmbox[LONG_STRING];
  int rc;
  int n;
  IMAP_MBOX mx;
  int err_continue = M_NO;

  idata = (IMAP_DATA*) ctx->data;

  if (imap_parse_path (dest, &mx))
  {
    dprint (1, (debugfile, "imap_copy_messages: bad destination %s\n", dest));
    return -1;
  }

  /* check that the save-to folder is in the same account */
  if (!mutt_account_match (&(CTX_DATA->conn->account), &(mx.account)))
  {
    dprint (3, (debugfile, "imap_copy_messages: %s not same server as %s\n",
      dest, ctx->path));
    return 1;
  }

  if (h && h->attach_del)
  {
    dprint (3, (debugfile, "imap_copy_messages: Message contains attachments to be deleted\n"));
    return 1;
  }
  
  imap_fix_path (idata, mx.mbox, mbox, sizeof (mbox));

  memset (&sync_cmd, 0, sizeof (sync_cmd));
  memset (&cmd, 0, sizeof (cmd));
  mutt_buffer_addstr (&cmd, "UID COPY ");

  /* Null HEADER* means copy tagged messages */
  if (!h)
  {
    /* if any messages have attachments to delete, fall through to FETCH
     * and APPEND. TODO: Copy what we can with COPY, fall through for the
     * remainder. */
    for (n = 0; n < ctx->msgcount; n++)
    {
      if (ctx->hdrs[n]->tagged && ctx->hdrs[n]->attach_del)
      {
	dprint (3, (debugfile, "imap_copy_messages: Message contains attachments to be deleted\n"));
	return 1;
      }

      if (ctx->hdrs[n]->tagged && ctx->hdrs[n]->active &&
	  ctx->hdrs[n]->changed)
      {
	rc = imap_sync_message (idata, ctx->hdrs[n], &sync_cmd, &err_continue);
	if (rc < 0)
	{
	  dprint (1, (debugfile, "imap_copy_messages: could not sync\n"));
	  goto fail;
	}
      }
    }

    rc = imap_make_msg_set (idata, &cmd, M_TAG, 0, 0);
    if (!rc)
    {
      dprint (1, (debugfile, "imap_copy_messages: No messages tagged\n"));
      goto fail;
    }
    mutt_message (_("Copying %d messages to %s..."), rc, mbox);
  }
  else
  {
    mutt_message (_("Copying message %d to %s..."), h->index+1, mbox);
    snprintf (uid, sizeof (uid), "%u", HEADER_DATA (h)->uid);
    mutt_buffer_addstr (&cmd, uid);

    if (h->active && h->changed)
    {
      rc = imap_sync_message (idata, h, &sync_cmd, &err_continue);
      if (rc < 0)
      {
	dprint (1, (debugfile, "imap_copy_messages: could not sync\n"));
	goto fail;
      }
    }
  }

  /* let's get it on */
  mutt_buffer_addstr (&cmd, " ");
  imap_munge_mbox_name (mmbox, sizeof (mmbox), mbox);
  mutt_buffer_addstr (&cmd, mmbox);

  rc = imap_exec (idata, cmd.data, IMAP_CMD_FAIL_OK);
  if (rc == -2)
  {
    /* bail out if command failed for reasons other than nonexistent target */
    if (ascii_strncasecmp (imap_get_qualifier (idata->buf), "[TRYCREATE]", 11))
    {
      imap_error ("imap_copy_messages", idata->buf);
      goto fail;
    }
    dprint (2, (debugfile, "imap_copy_messages: server suggests TRYCREATE\n"));
    snprintf (mmbox, sizeof (mmbox), _("Create %s?"), mbox);
    if (option (OPTCONFIRMCREATE) && mutt_yesorno (mmbox, 1) < 1)
    {
      mutt_clear_error ();
      goto fail;
    }
    if (imap_create_mailbox (idata, mbox) < 0)
      goto fail;

    /* try again */
    rc = imap_exec (idata, cmd.data, 0);
  }
  if (rc != 0)
  {
    imap_error ("imap_copy_messages", idata->buf);
    goto fail;
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

  if (cmd.data)
    FREE (&cmd.data);
  if (sync_cmd.data)
    FREE (&sync_cmd.data);
  FREE (&mx.mbox);
  return 0;

 fail:
  if (cmd.data)
    FREE (&cmd.data);
  if (sync_cmd.data)
    FREE (&sync_cmd.data);
  FREE (&mx.mbox);
  return -1;
}

/* imap_add_keywords: concatenate custom IMAP tags to list, if they
 *   appear in the folder flags list. Why wouldn't they? */
void imap_add_keywords (char* s, HEADER* h, LIST* mailbox_flags, size_t slen)
{
  LIST *keywords;

  if (!mailbox_flags || !HEADER_DATA(h) || !HEADER_DATA(h)->keywords)
    return;

  keywords = HEADER_DATA(h)->keywords->next;

  while (keywords)
  {
    if (imap_has_flag (mailbox_flags, keywords->data))
    {
      safe_strcat (s, slen, keywords->data);
      safe_strcat (s, slen, " ");
    }
    keywords = keywords->next;
  }
}

/* imap_free_header_data: free IMAP_HEADER structure */
void imap_free_header_data (void** data)
{
  /* this should be safe even if the list wasn't used */
  mutt_free_list (&(((IMAP_HEADER_DATA*) *data)->keywords));

  FREE (data);
}

/* imap_set_flags: fill out the message header according to the flags from
 *   the server. Expects a flags line of the form "FLAGS (flag flag ...)" */
char* imap_set_flags (IMAP_DATA* idata, HEADER* h, char* s)
{
  CONTEXT* ctx = idata->ctx;
  IMAP_HEADER newh;
  IMAP_HEADER_DATA* hd;
  unsigned char readonly;

  memset (&newh, 0, sizeof (newh));
  hd = h->data;
  newh.data = hd;

  dprint (2, (debugfile, "imap_fetch_message: parsing FLAGS\n"));
  if ((s = msg_parse_flags (&newh, s)) == NULL)
    return NULL;
  
  /* YAUH (yet another ugly hack): temporarily set context to
   * read-write even if it's read-only, so *server* updates of
   * flags can be processed by mutt_set_flag. ctx->changed must
   * be restored afterwards */
  readonly = ctx->readonly;
  ctx->readonly = 0;
	    
  mutt_set_flag (ctx, h, M_NEW, !(hd->read || hd->old));
  mutt_set_flag (ctx, h, M_OLD, hd->old);
  mutt_set_flag (ctx, h, M_READ, hd->read);
  mutt_set_flag (ctx, h, M_DELETE, hd->deleted);
  mutt_set_flag (ctx, h, M_FLAG, hd->flagged);
  mutt_set_flag (ctx, h, M_REPLIED, hd->replied);

  /* this message is now definitively *not* changed (mutt_set_flag
   * marks things changed as a side-effect) */
  h->changed = 0;
  ctx->changed &= ~readonly;
  ctx->readonly = readonly;

  return s;
}


/* msg_fetch_header: import IMAP FETCH response into an IMAP_HEADER.
 *   Expects string beginning with * n FETCH.
 *   Returns:
 *      0 on success
 *     -1 if the string is not a fetch response
 *     -2 if the string is a corrupt fetch response */
static int msg_fetch_header (CONTEXT* ctx, IMAP_HEADER* h, char* buf, FILE* fp)
{
  IMAP_DATA* idata;
  long bytes;
  int rc = -1; /* default now is that string isn't FETCH response*/

  idata = (IMAP_DATA*) ctx->data;

  if (buf[0] != '*')
    return rc;
  
  /* skip to message number */
  buf = imap_next_word (buf);
  h->sid = atoi (buf);

  /* find FETCH tag */
  buf = imap_next_word (buf);
  if (ascii_strncasecmp ("FETCH", buf, 5))
    return rc;

  rc = -2; /* we've got a FETCH response, for better or worse */
  if (!(buf = strchr (buf, '(')))
    return rc;
  buf++;

  /* FIXME: current implementation - call msg_parse_fetch - if it returns -2,
   *   read header lines and call it again. Silly. */
  if ((rc = msg_parse_fetch (h, buf) != -2) || !fp)
    return rc;
  
  if (imap_get_literal_count (buf, &bytes) == 0)
  {
    imap_read_literal (fp, idata, bytes, NULL);

    /* we may have other fields of the FETCH _after_ the literal
     * (eg Domino puts FLAGS here). Nothing wrong with that, either.
     * This all has to go - we should accept literals and nonliterals
     * interchangeably at any time. */
    if (imap_cmd_step (idata) != IMAP_CMD_CONTINUE)
      return rc;
  
    if (msg_parse_fetch (h, idata->buf) == -1)
      return rc;
  }

  rc = 0; /* success */
  
  /* subtract headers from message size - unfortunately only the subset of
   * headers we've requested. */
  h->content_length -= bytes;

  return rc;
}

#if USE_HCACHE
static size_t imap_hcache_keylen (const char *fn)
{
  return mutt_strlen(fn);
}
#endif /* USE_HCACHE */

/* msg_parse_fetch: handle headers returned from header fetch */
static int msg_parse_fetch (IMAP_HEADER *h, char *s)
{
  char tmp[SHORT_STRING];
  char *ptmp;

  if (!s)
    return -1;

  while (*s)
  {
    SKIPWS (s);

    if (ascii_strncasecmp ("FLAGS", s, 5) == 0)
    {
      if ((s = msg_parse_flags (h, s)) == NULL)
        return -1;
    }
    else if (ascii_strncasecmp ("UID", s, 3) == 0)
    {
      s += 3;
      SKIPWS (s);
      h->data->uid = (unsigned int) atoi (s);

      s = imap_next_word (s);
    }
    else if (ascii_strncasecmp ("INTERNALDATE", s, 12) == 0)
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
    else if (ascii_strncasecmp ("RFC822.SIZE", s, 11) == 0)
    {
      s += 11;
      SKIPWS (s);
      ptmp = tmp;
      while (isdigit ((unsigned char) *s))
        *ptmp++ = *s++;
      *ptmp = 0;
      h->content_length = atoi (tmp);
    }
    else if (!ascii_strncasecmp ("BODY", s, 4) ||
      !ascii_strncasecmp ("RFC822.HEADER", s, 13))
    {
      /* handle above, in msg_fetch_header */
      return -2;
    }
    else if (*s == ')')
      s++; /* end of request */
    else if (*s)
    {
      /* got something i don't understand */
      imap_error ("msg_parse_fetch", s);
      return -1;
    }
  }

  return 0;
}

/* msg_parse_flags: read a FLAGS token into an IMAP_HEADER */
static char* msg_parse_flags (IMAP_HEADER* h, char* s)
{
  IMAP_HEADER_DATA* hd = h->data;

  /* sanity-check string */
  if (ascii_strncasecmp ("FLAGS", s, 5) != 0)
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

  mutt_free_list (&hd->keywords);
  hd->deleted = hd->flagged = hd->replied = hd->read = hd->old = 0;

  /* start parsing */
  while (*s && *s != ')')
  {
    if (ascii_strncasecmp ("\\deleted", s, 8) == 0)
    {
      s += 8;
      hd->deleted = 1;
    }
    else if (ascii_strncasecmp ("\\flagged", s, 8) == 0)
    {
      s += 8;
      hd->flagged = 1;
    }
    else if (ascii_strncasecmp ("\\answered", s, 9) == 0)
    {
      s += 9;
      hd->replied = 1;
    }
    else if (ascii_strncasecmp ("\\seen", s, 5) == 0)
    {
      s += 5;
      hd->read = 1;
    }
    else if (ascii_strncasecmp ("\\recent", s, 5) == 0)
      s += 7;
    else if (ascii_strncasecmp ("old", s, 3) == 0)
    {
      s += 3;
      hd->old = 1;
    }
    else
    {
      /* store custom flags as well */
      char ctmp;
      char* flag_word = s;

      if (!hd->keywords)
        hd->keywords = mutt_new_list ();

      while (*s && !ISSPACE (*s) && *s != ')')
        s++;
      ctmp = *s;
      *s = '\0';
      mutt_add_list (hd->keywords, flag_word);
      *s = ctmp;
    }
    SKIPWS(s);
  }

  /* wrap up, or note bad flags response */
  if (*s == ')')
    s++;
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
