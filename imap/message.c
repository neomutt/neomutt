/*
 * Copyright (C) 1996-1999 Brandon Long <blong@fiction.net>
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
 *
 */

/* message parsing/updating functions */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <ctype.h>

#include "mutt.h"
#include "imap_private.h"
#include "mx.h"

#ifdef HAVE_PGP
#include "pgp.h"
#endif

#if USE_HCACHE
#include "hcache.h"
#endif

#include "bcache.h"

static FILE* msg_cache_get (IMAP_DATA* idata, HEADER* h);
static FILE* msg_cache_put (IMAP_DATA* idata, HEADER* h);
static int msg_cache_commit (IMAP_DATA* idata, HEADER* h);

static void flush_buffer(char* buf, size_t* len, CONNECTION* conn);
static int msg_fetch_header (CONTEXT* ctx, IMAP_HEADER* h, char* buf,
  FILE* fp);
static int msg_parse_fetch (IMAP_HEADER* h, char* s);
static char* msg_parse_flags (IMAP_HEADER* h, char* s);

/* imap_read_headers:
 * Changed to read many headers instead of just one. It will return the
 * msgno of the last message read. It will return a value other than
 * msgend if mail comes in while downloading headers (in theory).
 */
int imap_read_headers (IMAP_DATA* idata, int msgbegin, int msgend)
{
  CONTEXT* ctx;
  char *hdrreq = NULL;
  FILE *fp;
  char tempfile[_POSIX_PATH_MAX];
  int msgno, idx = msgbegin - 1;
  IMAP_HEADER h;
  IMAP_STATUS* status;
  int rc, mfhrc, oldmsgcount;
  int fetchlast = 0;
  int maxuid = 0;
  static const char * const want_headers = "DATE FROM SUBJECT TO CC MESSAGE-ID REFERENCES CONTENT-TYPE CONTENT-DESCRIPTION IN-REPLY-TO REPLY-TO LINES LIST-POST X-LABEL";
  progress_t progress;
  int retval = -1;

#if USE_HCACHE
  char buf[LONG_STRING];
  unsigned int *uid_validity = NULL;
  unsigned int *puidnext = NULL;
  unsigned int uidnext = 0;
  int evalhc = 0;
#endif /* USE_HCACHE */

  ctx = idata->ctx;

  if (mutt_bit_isset (idata->capabilities,IMAP4REV1))
  {
    safe_asprintf (&hdrreq, "BODY.PEEK[HEADER.FIELDS (%s%s%s)]",
                           want_headers, ImapHeaders ? " " : "", NONULL (ImapHeaders));
  }
  else if (mutt_bit_isset (idata->capabilities,IMAP4))
  {
    safe_asprintf (&hdrreq, "RFC822.HEADER.LINES (%s%s%s)",
                           want_headers, ImapHeaders ? " " : "", NONULL (ImapHeaders));
  }
  else
  {	/* Unable to fetch headers for lower versions */
    mutt_error _("Unable to fetch headers from this IMAP server version.");
    mutt_sleep (2);	/* pause a moment to let the user see the error */
    goto error_out_0;
  }

  /* instead of downloading all headers and then parsing them, we parse them
   * as they come in. */
  mutt_mktemp (tempfile, sizeof (tempfile));
  if (!(fp = safe_fopen (tempfile, "w+")))
  {
    mutt_error (_("Could not create temporary file %s"), tempfile);
    mutt_sleep (2);
    goto error_out_0;
  }
  unlink (tempfile);

  /* make sure context has room to hold the mailbox */
  while ((msgend) >= idata->ctx->hdrmax)
    mx_alloc_memory (idata->ctx);

  oldmsgcount = ctx->msgcount;
  idata->reopen &= ~(IMAP_REOPEN_ALLOW|IMAP_NEWMAIL_PENDING);
  idata->newMailCount = 0;

#if USE_HCACHE
  idata->hcache = imap_hcache_open (idata, NULL);

  if (idata->hcache && !msgbegin)
  {
    uid_validity = mutt_hcache_fetch_raw (idata->hcache, "/UIDVALIDITY", imap_hcache_keylen);
    puidnext = mutt_hcache_fetch_raw (idata->hcache, "/UIDNEXT", imap_hcache_keylen);
    if (puidnext)
    {
      uidnext = *puidnext;
      FREE (&puidnext);
    }
    if (uid_validity && uidnext && *uid_validity == idata->uid_validity)
      evalhc = 1;
    FREE (&uid_validity);
  }
  if (evalhc)
  {
    /* L10N:
       Comparing the cached data with the IMAP server's data */
    mutt_progress_init (&progress, _("Evaluating cache..."),
			MUTT_PROGRESS_MSG, ReadInc, msgend + 1);

    snprintf (buf, sizeof (buf),
      "UID FETCH 1:%u (UID FLAGS)", uidnext - 1);

    imap_cmd_start (idata, buf);

    rc = IMAP_CMD_CONTINUE;
    for (msgno = msgbegin; rc == IMAP_CMD_CONTINUE; msgno++)
    {
      mutt_progress_update (&progress, msgno + 1, -1);

      memset (&h, 0, sizeof (h));
      h.data = safe_calloc (1, sizeof (IMAP_HEADER_DATA));
      do
      {
        mfhrc = 0;

        rc = imap_cmd_step (idata);
        if (rc != IMAP_CMD_CONTINUE)
	{
	  imap_free_header_data (&h.data);
          break;
	}

        /* hole in the header cache */
        if (!evalhc)
          continue;

        if ((mfhrc = msg_fetch_header (ctx, &h, idata->buf, NULL)) == -1)
          continue;
        else if (mfhrc < 0)
	{
	  imap_free_header_data (&h.data);
          break;
	}

        if (!h.data->uid)
        {
          dprint (2, (debugfile, "imap_read_headers: skipping hcache FETCH "
                      "response for unknown message number %d\n", h.sid));
          mfhrc = -1;
          continue;
        }

        idx++;
        ctx->hdrs[idx] = imap_hcache_get (idata, h.data->uid);
        if (ctx->hdrs[idx])
        {
  	  ctx->hdrs[idx]->index = idx;
  	  /* messages which have not been expunged are ACTIVE (borrowed from mh
  	   * folders) */
  	  ctx->hdrs[idx]->active = 1;
          ctx->hdrs[idx]->read = h.data->read;
          ctx->hdrs[idx]->old = h.data->old;
          ctx->hdrs[idx]->deleted = h.data->deleted;
          ctx->hdrs[idx]->flagged = h.data->flagged;
          ctx->hdrs[idx]->replied = h.data->replied;
          ctx->hdrs[idx]->changed = h.data->changed;
          /*  ctx->hdrs[msgno]->received is restored from mutt_hcache_restore */
          ctx->hdrs[idx]->data = (void *) (h.data);

          ctx->msgcount++;
          ctx->size += ctx->hdrs[idx]->content->length;
        }
	else
        {
	  /* bad header in the cache, we'll have to refetch. */
          dprint (3, (debugfile, "bad cache entry at %d, giving up\n", h.sid - 1));
          imap_free_header_data(&h.data);
          evalhc = 0;
          idx--;
        }
      }
      while (rc != IMAP_CMD_OK && mfhrc == -1);
      if (rc == IMAP_CMD_OK)
        break;
      if ((mfhrc < -1) || ((rc != IMAP_CMD_CONTINUE) && (rc != IMAP_CMD_OK)))
      {
        imap_free_header_data (&h.data);
        imap_hcache_close (idata);
	goto error_out_1;
      }
    }
    /* could also look for first null header in case hcache is holey */
    msgbegin = ctx->msgcount;
  }
#endif /* USE_HCACHE */

  mutt_progress_init (&progress, _("Fetching message headers..."),
		      MUTT_PROGRESS_MSG, ReadInc, msgend + 1);

  for (msgno = msgbegin; msgno <= msgend ; msgno++)
  {
    mutt_progress_update (&progress, msgno + 1, -1);

    /* we may get notification of new mail while fetching headers */
    if (msgno + 1 > fetchlast)
    {
      char *cmd;

      fetchlast = msgend + 1;
      safe_asprintf (&cmd, "FETCH %d:%d (UID FLAGS INTERNALDATE RFC822.SIZE %s)",
                     msgno + 1, fetchlast, hdrreq);
      imap_cmd_start (idata, cmd);
      FREE (&cmd);
    }

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

      if ((mfhrc = msg_fetch_header (ctx, &h, idata->buf, fp)) == -1)
	continue;
      else if (mfhrc < 0)
	break;

      if (!ftello (fp))
      {
        dprint (2, (debugfile, "msg_fetch_header: ignoring fetch response with no body\n"));
        mfhrc = -1;
        msgend--;
        continue;
      }

      /* make sure we don't get remnants from older larger message headers */
      fputs ("\n\n", fp);

      idx++;
      if (idx > msgend)
      {
        dprint (1, (debugfile, "imap_read_headers: skipping FETCH response for "
                    "unknown message number %d\n", h.sid));
        mfhrc = -1;
        idx--;
        continue;
      }
      /* May receive FLAGS updates in a separate untagged response (#2935) */
      if (idx < ctx->msgcount)
      {
	dprint (2, (debugfile, "imap_read_headers: message %d is not new\n",
		    h.sid));
        idx--;
	continue;
      }

      ctx->hdrs[idx] = mutt_new_header ();

      ctx->hdrs[idx]->index = h.sid - 1;
      /* messages which have not been expunged are ACTIVE (borrowed from mh
       * folders) */
      ctx->hdrs[idx]->active = 1;
      ctx->hdrs[idx]->read = h.data->read;
      ctx->hdrs[idx]->old = h.data->old;
      ctx->hdrs[idx]->deleted = h.data->deleted;
      ctx->hdrs[idx]->flagged = h.data->flagged;
      ctx->hdrs[idx]->replied = h.data->replied;
      ctx->hdrs[idx]->changed = h.data->changed;
      ctx->hdrs[idx]->received = h.received;
      ctx->hdrs[idx]->data = (void *) (h.data);

      if (maxuid < h.data->uid)
        maxuid = h.data->uid;

      rewind (fp);
      /* NOTE: if Date: header is missing, mutt_read_rfc822_header depends
       *   on h.received being set */
      ctx->hdrs[idx]->env = mutt_read_rfc822_header (fp, ctx->hdrs[idx],
        0, 0);
      /* content built as a side-effect of mutt_read_rfc822_header */
      ctx->hdrs[idx]->content->length = h.content_length;
      ctx->size += h.content_length;

#if USE_HCACHE
      imap_hcache_put (idata, ctx->hdrs[idx]);
#endif /* USE_HCACHE */

      ctx->msgcount++;
    }
    while ((rc != IMAP_CMD_OK) && ((mfhrc == -1) ||
      ((msgno + 1) >= fetchlast)));

    if ((mfhrc < -1) || ((rc != IMAP_CMD_CONTINUE) && (rc != IMAP_CMD_OK)))
    {
      imap_free_header_data (&h.data);
#if USE_HCACHE
      imap_hcache_close (idata);
#endif
      goto error_out_1;
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

  if (maxuid && (status = imap_mboxcache_get (idata, idata->mailbox, 0)) &&
      (status->uidnext < maxuid + 1))
    status->uidnext = maxuid + 1;

#if USE_HCACHE
  mutt_hcache_store_raw (idata->hcache, "/UIDVALIDITY", &idata->uid_validity,
                         sizeof (idata->uid_validity), imap_hcache_keylen);
  if (maxuid && idata->uidnext < maxuid + 1)
  {
    dprint (2, (debugfile, "Overriding UIDNEXT: %u -> %u\n", idata->uidnext, maxuid + 1));
    idata->uidnext = maxuid + 1;
  }
  if (idata->uidnext > 1)
    mutt_hcache_store_raw (idata->hcache, "/UIDNEXT", &idata->uidnext,
			   sizeof (idata->uidnext), imap_hcache_keylen);

  imap_hcache_close (idata);
#endif /* USE_HCACHE */

  if (ctx->msgcount > oldmsgcount)
  {
    mx_alloc_memory(ctx);
    mx_update_context (ctx, ctx->msgcount - oldmsgcount);
  }

  idata->reopen |= IMAP_REOPEN_ALLOW;

  retval = msgend;

error_out_1:
  safe_fclose (&fp);

error_out_0:
  FREE (&hdrreq);

  return retval;
}

int imap_fetch_message (CONTEXT *ctx, MESSAGE *msg, int msgno)
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

  if ((msg->fp = msg_cache_get (idata, h)))
  {
    if (HEADER_DATA(h)->parsed)
      return 0;
    else
      goto parsemsg;
  }

  /* we still do some caching even if imap_cachedir is unset */
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

  if (!(msg->fp = msg_cache_put (idata, h)))
  {
    cache->uid = HEADER_DATA(h)->uid;
    mutt_mktemp (path, sizeof (path));
    cache->path = safe_strdup (path);
    if (!(msg->fp = safe_fopen (path, "w+")))
    {
      FREE (&cache->path);
      return -1;
    }
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
	  mutt_progress_init (&progressbar, _("Fetching message..."),
			      MUTT_PROGRESS_SIZE, NetInc, bytes);
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

  msg_cache_commit (idata, h);

  parsemsg:
  /* Update the header information.  Previously, we only downloaded a
   * portion of the headers, those required for the main display.
   */
  rewind (msg->fp);
  /* It may be that the Status header indicates a message is read, but the
   * IMAP server doesn't know the message has been \Seen. So we capture
   * the server's notion of 'read' and if it differs from the message info
   * picked up in mutt_read_rfc822_header, we mark the message (and context
   * changed). Another possibility: ignore Status on IMAP?*/
  read = h->read;
  newenv = mutt_read_rfc822_header (msg->fp, h, 0, 0);
  mutt_merge_envelopes(h->env, &newenv);

  /* see above. We want the new status in h->read, so we unset it manually
   * and let mutt_set_flag set it correctly, updating context. */
  if (read != h->read)
  {
    h->read = read;
    mutt_set_flag (ctx, h, MUTT_NEW, read);
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
  HEADER_DATA(h)->parsed = 1;

  return 0;

bail:
  safe_fclose (&msg->fp);
  imap_cache_del (idata, h);
  if (cache->path)
  {
    unlink (cache->path);
    FREE (&cache->path);
  }

  return -1;
}

int imap_close_message (CONTEXT *ctx, MESSAGE *msg)
{
  return safe_fclose (&msg->fp);
}

int imap_commit_message (CONTEXT *ctx, MESSAGE *msg)
{
  int r = safe_fclose (&msg->fp);

  if (r)
    return r;

  return imap_append_message (ctx, msg);
}

int imap_append_message (CONTEXT *ctx, MESSAGE *msg)
{
  IMAP_DATA* idata;
  FILE *fp;
  char buf[LONG_STRING];
  char mbox[LONG_STRING];
  char mailbox[LONG_STRING];
  char internaldate[IMAP_DATELEN];
  char imap_flags[SHORT_STRING];
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
  if (!*mailbox)
    strfcpy (mailbox, "INBOX", sizeof (mailbox));

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

  mutt_progress_init (&progressbar, _("Uploading message..."),
		      MUTT_PROGRESS_SIZE, NetInc, len);

  imap_munge_mbox_name (idata, mbox, sizeof (mbox), mailbox);
  imap_make_date (internaldate, msg->received);

  imap_flags[0] = imap_flags[1] = 0;
  if (msg->flags.read)
    safe_strcat (imap_flags, sizeof (imap_flags), " \\Seen");
  if (msg->flags.replied)
    safe_strcat (imap_flags, sizeof (imap_flags), " \\Answered");
  if (msg->flags.flagged)
    safe_strcat (imap_flags, sizeof (imap_flags), " \\Flagged");
  if (msg->flags.draft)
    safe_strcat (imap_flags, sizeof (imap_flags), " \\Draft");

  snprintf (buf, sizeof (buf), "APPEND %s (%s) \"%s\" {%lu}", mbox,
            imap_flags + 1,
	    internaldate,
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
    safe_fclose (&fp);
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
      mutt_progress_update (&progressbar, sent, -1);
    }
  }

  if (len)
    flush_buffer(buf, &len, idata->conn);

  mutt_socket_write (idata->conn, "\r\n");
  safe_fclose (&fp);

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
  char mbox[LONG_STRING];
  char mmbox[LONG_STRING];
  char prompt[LONG_STRING];
  int rc;
  int n;
  IMAP_MBOX mx;
  int err_continue = MUTT_NO;
  int triedcreate = 0;

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
  if (!*mbox)
    strfcpy (mbox, "INBOX", sizeof (mbox));
  imap_munge_mbox_name (idata, mmbox, sizeof (mmbox), mbox);

  /* loop in case of TRYCREATE */
  do
  {
    mutt_buffer_init (&sync_cmd);
    mutt_buffer_init (&cmd);

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
            goto out;
          }
        }
      }

      rc = imap_exec_msgset (idata, "UID COPY", mmbox, MUTT_TAG, 0, 0);
      if (!rc)
      {
        dprint (1, (debugfile, "imap_copy_messages: No messages tagged\n"));
        rc = -1;
        goto out;
      }
      else if (rc < 0)
      {
        dprint (1, (debugfile, "could not queue copy\n"));
        goto out;
      }
      else
        mutt_message (_("Copying %d messages to %s..."), rc, mbox);
    }
    else
    {
      mutt_message (_("Copying message %d to %s..."), h->index+1, mbox);
      mutt_buffer_printf (&cmd, "UID COPY %u %s", HEADER_DATA (h)->uid, mmbox);

      if (h->active && h->changed)
      {
        rc = imap_sync_message (idata, h, &sync_cmd, &err_continue);
        if (rc < 0)
        {
          dprint (1, (debugfile, "imap_copy_messages: could not sync\n"));
          goto out;
        }
      }
      if ((rc = imap_exec (idata, cmd.data, IMAP_CMD_QUEUE)) < 0)
      {
        dprint (1, (debugfile, "could not queue copy\n"));
        goto out;
      }
    }

    /* let's get it on */
    rc = imap_exec (idata, NULL, IMAP_CMD_FAIL_OK);
    if (rc == -2)
    {
      if (triedcreate)
      {
        dprint (1, (debugfile, "Already tried to create mailbox %s\n", mbox));
        break;
      }
      /* bail out if command failed for reasons other than nonexistent target */
      if (ascii_strncasecmp (imap_get_qualifier (idata->buf), "[TRYCREATE]", 11))
        break;
      dprint (3, (debugfile, "imap_copy_messages: server suggests TRYCREATE\n"));
      snprintf (prompt, sizeof (prompt), _("Create %s?"), mbox);
      if (option (OPTCONFIRMCREATE) && mutt_yesorno (prompt, 1) < 1)
      {
        mutt_clear_error ();
        goto out;
      }
      if (imap_create_mailbox (idata, mbox) < 0)
        break;
      triedcreate = 1;
    }
  }
  while (rc == -2);

  if (rc != 0)
  {
    imap_error ("imap_copy_messages", idata->buf);
    goto out;
  }

  /* cleanup */
  if (delete)
  {
    if (!h)
      for (n = 0; n < ctx->msgcount; n++)
      {
        if (ctx->hdrs[n]->tagged)
        {
          mutt_set_flag (ctx, ctx->hdrs[n], MUTT_DELETE, 1);
          mutt_set_flag (ctx, ctx->hdrs[n], MUTT_PURGE, 1);
          if (option (OPTDELETEUNTAG))
            mutt_set_flag (ctx, ctx->hdrs[n], MUTT_TAG, 0);
        }
      }
    else
    {
      mutt_set_flag (ctx, h, MUTT_DELETE, 1);
      mutt_set_flag (ctx, h, MUTT_PURGE, 1);
      if (option (OPTDELETEUNTAG))
        mutt_set_flag (ctx, h, MUTT_TAG, 0);
    }
  }

  rc = 0;

 out:
  if (cmd.data)
    FREE (&cmd.data);
  if (sync_cmd.data)
    FREE (&sync_cmd.data);
  FREE (&mx.mbox);

  return rc < 0 ? -1 : rc;
}

static body_cache_t *msg_cache_open (IMAP_DATA *idata)
{
  char mailbox[_POSIX_PATH_MAX];

  if (idata->bcache)
    return idata->bcache;

  imap_cachepath (idata, idata->mailbox, mailbox, sizeof (mailbox));

  return mutt_bcache_open (&idata->conn->account, mailbox);
}

static FILE* msg_cache_get (IMAP_DATA* idata, HEADER* h)
{
  char id[_POSIX_PATH_MAX];

  if (!idata || !h)
    return NULL;

  idata->bcache = msg_cache_open (idata);
  snprintf (id, sizeof (id), "%u-%u", idata->uid_validity, HEADER_DATA(h)->uid);
  return mutt_bcache_get (idata->bcache, id);
}

static FILE* msg_cache_put (IMAP_DATA* idata, HEADER* h)
{
  char id[_POSIX_PATH_MAX];

  if (!idata || !h)
    return NULL;

  idata->bcache = msg_cache_open (idata);
  snprintf (id, sizeof (id), "%u-%u", idata->uid_validity, HEADER_DATA(h)->uid);
  return mutt_bcache_put (idata->bcache, id, 1);
}

static int msg_cache_commit (IMAP_DATA* idata, HEADER* h)
{
  char id[_POSIX_PATH_MAX];

  if (!idata || !h)
    return -1;

  idata->bcache = msg_cache_open (idata);
  snprintf (id, sizeof (id), "%u-%u", idata->uid_validity, HEADER_DATA(h)->uid);

  return mutt_bcache_commit (idata->bcache, id);
}

int imap_cache_del (IMAP_DATA* idata, HEADER* h)
{
  char id[_POSIX_PATH_MAX];

  if (!idata || !h)
    return -1;

  idata->bcache = msg_cache_open (idata);
  snprintf (id, sizeof (id), "%u-%u", idata->uid_validity, HEADER_DATA(h)->uid);
  return mutt_bcache_del (idata->bcache, id);
}

static int msg_cache_clean_cb (const char* id, body_cache_t* bcache, void* data)
{
  unsigned int uv, uid, n;
  IMAP_DATA* idata = (IMAP_DATA*)data;

  if (sscanf (id, "%u-%u", &uv, &uid) != 2)
    return 0;

  /* bad UID */
  if (uv != idata->uid_validity)
    mutt_bcache_del (bcache, id);

  /* TODO: presort UIDs, walk in order */
  for (n = 0; n < idata->ctx->msgcount; n++)
  {
    if (uid == HEADER_DATA(idata->ctx->hdrs[n])->uid)
      return 0;
  }
  mutt_bcache_del (bcache, id);

  return 0;
}

int imap_cache_clean (IMAP_DATA* idata)
{
  idata->bcache = msg_cache_open (idata);
  mutt_bcache_list (idata->bcache, msg_cache_clean_cb, idata);

  return 0;
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
void imap_free_header_data (IMAP_HEADER_DATA** data)
{
  if (*data)
  {
    /* this should be safe even if the list wasn't used */
    mutt_free_list (&((*data)->keywords));
    FREE (data); /* __FREE_CHECKED__ */
  }
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

  mutt_set_flag (ctx, h, MUTT_NEW, !(hd->read || hd->old));
  mutt_set_flag (ctx, h, MUTT_OLD, hd->old);
  mutt_set_flag (ctx, h, MUTT_READ, hd->read);
  mutt_set_flag (ctx, h, MUTT_DELETE, hd->deleted);
  mutt_set_flag (ctx, h, MUTT_FLAG, hd->flagged);
  mutt_set_flag (ctx, h, MUTT_REPLIED, hd->replied);

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
  if ((rc = msg_parse_fetch (h, buf)) != -2 || !fp)
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
    else if (ascii_strncasecmp ("\\recent", s, 7) == 0)
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
  mutt_socket_write_n(conn, buf, *len);
  *len = 0;
}
