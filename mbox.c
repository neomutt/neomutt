/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@cs.hmc.edu>
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

/* This file contains code to parse ``mbox'' and ``mmdf'' style mailboxes */

/* OS/2's "kendra" mail folder format is also supported.  It's a slightly
 * modified version of MMDF.
 */

#include "mutt.h"
#include "mailbox.h"
#include "mx.h"
#include "sort.h"
#include "copy.h"

#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <utime.h>
#include <sys/file.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

/* struct used by mutt_sync_mailbox() to store new offsets */
struct m_update_t
{
  long hdr;
  long body;
};

/* parameters:
 * ctx - context to lock
 * excl - exclusive lock?
 * retry - should retry if unable to lock?
 */
int mbox_lock_mailbox (CONTEXT *ctx, int excl, int retry)
{
  int r;

  if ((r = mx_lock_file (ctx->path, fileno (ctx->fp), excl, 1, retry)) == 0)
    ctx->locked = 1;
  else if (retry && !excl)
  {
    ctx->readonly = 1;
    return 0;
  }
  
  return (r);
}

void mbox_unlock_mailbox (CONTEXT *ctx)
{
  if (ctx->locked)
  {
    fflush (ctx->fp);

    mx_unlock_file (ctx->path, fileno (ctx->fp), 1);
    ctx->locked = 0;
  }
}

int mmdf_parse_mailbox (CONTEXT *ctx)
{
  char buf[HUGE_STRING];
  char return_path[LONG_STRING];
  int count = 0;
  int lines;
  time_t t, tz;
  long loc, tmploc;
  HEADER *hdr;
  struct stat sb;
#ifdef NFS_ATTRIBUTE_HACK
  struct utimbuf newtime;
#endif

  if (stat (ctx->path, &sb) == -1)
  {
    mutt_perror (ctx->path);
    return (-1);
  }
  ctx->mtime = sb.st_mtime;
  ctx->size = sb.st_size;

#ifdef NFS_ATTRIBUTE_HACK
  if (sb.st_mtime > sb.st_atime)
  {
    newtime.modtime = sb.st_mtime;
    newtime.actime = time (NULL);
    utime (ctx->path, &newtime);
  }
#endif

  /* precompute the local timezone to speed up calculation of the
     received time */
  tz = mutt_local_tz (0);

  buf[sizeof (buf) - 1] = 0;
  
  FOREVER
  {
    if (fgets (buf, sizeof (buf) - 1, ctx->fp) == NULL)
      break;

    if (mutt_strcmp (buf, ctx->magic == M_MMDF ? MMDF_SEP : KENDRA_SEP) == 0)
    {
      loc = ftell (ctx->fp);
      
      count++;
      if (!ctx->quiet && ReadInc && ((count % ReadInc == 0) || count == 1))
	mutt_message (_("Reading %s... %d (%d%%)"), ctx->path, count,
		      loc / (ctx->size / 100 + 1));


      if (ctx->msgcount == ctx->hdrmax)
	mx_alloc_memory (ctx);
      ctx->hdrs[ctx->msgcount] = hdr = mutt_new_header ();
      hdr->offset = loc;
      hdr->index = ctx->msgcount;

      if (fgets (buf, sizeof (buf) - 1, ctx->fp) == NULL)
      {
	dprint (1, (debugfile, "mmdf_parse_mailbox: unexpected EOF\n"));
	break;
      }

      return_path[0] = 0;
      t = is_from (buf, return_path, sizeof (return_path));

      if (!t)
      {
	if (fseek (ctx->fp, loc, SEEK_SET) != 0)
	{
	  dprint (1, (debugfile, "mmdf_parse_mailbox: fseek() failed\n"));
	  mutt_error _("Mailbox is corrupt!");
	  return (-1);
	}
      } 
      else
	hdr->received = t - tz;

      hdr->env = mutt_read_rfc822_header (ctx->fp, hdr, 0, 0);

      loc = ftell (ctx->fp);

      if (hdr->content->length > 0 && hdr->lines > 0)
      {
	tmploc = loc + hdr->content->length;

	if (0 < tmploc && tmploc < ctx->size)
	{
	  if (fseek (ctx->fp, tmploc, SEEK_SET) != 0 ||
	      fgets (buf, sizeof (buf) - 1, ctx->fp) == NULL ||
	      mutt_strcmp (ctx->magic == M_MMDF ? MMDF_SEP : KENDRA_SEP, buf) != 0)
	  {
	    if (fseek (ctx->fp, loc, SEEK_SET) != 0)
	      dprint (1, (debugfile, "mmdf_parse_mailbox: fseek() failed\n"));
	    hdr->content->length = -1;
	  }
	}
	else
	  hdr->content->length = -1;
      }
      else
	hdr->content->length = -1;

      if (hdr->content->length < 0)
      {
	lines = -1;
	do {
	  loc = ftell (ctx->fp);
	  if (fgets (buf, sizeof (buf) - 1, ctx->fp) == NULL)
	    break;
	  lines++;
	} while (mutt_strcmp (buf, ctx->magic == M_MMDF ? MMDF_SEP : KENDRA_SEP) != 0);

	hdr->lines = lines;
	hdr->content->length = loc - hdr->content->offset;
      }

      if (!hdr->env->return_path && return_path[0])
	hdr->env->return_path = rfc822_parse_adrlist (hdr->env->return_path, return_path);

      if (!hdr->env->from)
	hdr->env->from = rfc822_cpy_adr (hdr->env->return_path);

      mx_update_context (ctx);
      if(ctx->magic == M_KENDRA && feof(ctx->fp))
	break;
    }
    else
    {
      dprint (1, (debugfile, "mmdf_parse_mailbox: corrupt mailbox!\n"));
      mutt_error _("Mailbox is corrupt!");
      return (-1);
    }
  }

  return 0;
}

/* Note that this function is also called when new mail is appended to the
 * currently open folder, and NOT just when the mailbox is initially read.
 *
 * NOTE: it is assumed that the mailbox being read has been locked before
 * this routine gets called.  Strange things could happen if it's not!
 */
int mbox_parse_mailbox (CONTEXT *ctx)
{
  struct stat sb;
  char buf[HUGE_STRING], return_path[STRING];
  HEADER *curhdr;
  time_t t, tz;
  int count = 0, lines = 0;
  long loc;
#ifdef NFS_ATTRIBUTE_HACK
  struct utimbuf newtime;
#endif

  /* Save information about the folder at the time we opened it. */
  if (stat (ctx->path, &sb) == -1)
  {
    mutt_perror (ctx->path);
    return (-1);
  }

  ctx->size = sb.st_size;
  ctx->mtime = sb.st_mtime;

#ifdef NFS_ATTRIBUTE_HACK
  if (sb.st_mtime > sb.st_atime)
  {
    newtime.modtime = sb.st_mtime;
    newtime.actime = time (NULL);
    utime (ctx->path, &newtime);
  }
#endif

  if (!ctx->readonly)
    ctx->readonly = access (ctx->path, W_OK) ? 1 : 0;

  /* precompute the local timezone to speed up calculation of the
     date received */
  tz = mutt_local_tz (0);

  loc = ftell (ctx->fp);
  while (fgets (buf, sizeof (buf), ctx->fp) != NULL)
  {
    if ((t = is_from (buf, return_path, sizeof (return_path))))
    {
      /* Save the Content-Length of the previous message */
      if (count > 0)
      {
#define PREV ctx->hdrs[ctx->msgcount-1]

	if (PREV->content->length < 0)
	{
	  PREV->content->length = loc - PREV->content->offset - 1;
	  if (PREV->content->length < 0)
	    PREV->content->length = 0;
	}
	if (!PREV->lines)
	  PREV->lines = lines ? lines - 1 : 0;
      }

      count++;

      if (!ctx->quiet && ReadInc && ((count % ReadInc == 0) || count == 1))
	mutt_message (_("Reading %s... %d (%d%%)"), ctx->path, count,
		      ftell (ctx->fp) / (ctx->size / 100 + 1));

      if (ctx->msgcount == ctx->hdrmax)
	mx_alloc_memory (ctx);
      
      curhdr = ctx->hdrs[ctx->msgcount] = mutt_new_header ();
      curhdr->received = t - tz;
      curhdr->offset = loc;
      curhdr->index = ctx->msgcount;
	
      curhdr->env = mutt_read_rfc822_header (ctx->fp, curhdr, 0, 0);

      /* if we know how long this message is, either just skip over the body,
       * or if we don't know how many lines there are, count them now (this will
       * save time by not having to search for the next message marker).
       */
      if (curhdr->content->length > 0)
      {
	long tmploc;

	loc = ftell (ctx->fp);
	tmploc = loc + curhdr->content->length + 1;

	if (0 < tmploc && tmploc < ctx->size)
	{
	  /*
	   * check to see if the content-length looks valid.  we expect to
	   * to see a valid message separator at this point in the stream
	   */
	  if (fseek (ctx->fp, tmploc, SEEK_SET) != 0 ||
	      fgets (buf, sizeof (buf), ctx->fp) == NULL ||
	      mutt_strncmp ("From ", buf, 5) != 0)
	  {
	    dprint (1, (debugfile, "mbox_parse_mailbox: bad content-length in message %d (cl=%ld)\n", curhdr->index, curhdr->content->length));
	    dprint (1, (debugfile, "\tLINE: %s", buf));
	    if (fseek (ctx->fp, loc, SEEK_SET) != 0) /* nope, return the previous position */
	    {
	      dprint (1, (debugfile, "mbox_parse_mailbox: fseek() failed\n"));
	    }
	    curhdr->content->length = -1;
	  }
	}
	else if (tmploc != ctx->size)
	{
	  /* content-length would put us past the end of the file, so it
	   * must be wrong
	   */
	  curhdr->content->length = -1;
	}

	if (curhdr->content->length != -1)
	{
	  /* good content-length.  check to see if we know how many lines
	   * are in this message.
	   */
	  if (curhdr->lines == 0)
	  {
	    int cl = curhdr->content->length;

	    /* count the number of lines in this message */
	    if (fseek (ctx->fp, loc, SEEK_SET) != 0)
	      dprint (1, (debugfile, "mbox_parse_mailbox: fseek() failed\n"));
	    while (cl-- > 0)
	    {
	      if (fgetc (ctx->fp) == '\n')
		curhdr->lines++;
	    }
	  }

	  /* return to the offset of the next message separator */
	  if (fseek (ctx->fp, tmploc, SEEK_SET) != 0)
	    dprint (1, (debugfile, "mbox_parse_mailbox: fseek() failed\n"));
	}
      }

      mx_update_context (ctx);

      if (!curhdr->env->return_path && return_path[0])
	curhdr->env->return_path = rfc822_parse_adrlist (curhdr->env->return_path, return_path);

      if (!curhdr->env->from)
	curhdr->env->from = rfc822_cpy_adr (curhdr->env->return_path);

      lines = 0;
    }
    else
      lines++;
    
    loc = ftell (ctx->fp);
  }
  
  /*
   * Only set the content-length of the previous message if we have read more
   * than one message during _this_ invocation.  If this routine is called
   * when new mail is received, we need to make sure not to clobber what
   * previously was the last message since the headers may be sorted.
   */
  if (count > 0)
  {
    if (PREV->content->length < 0)
    {
      PREV->content->length = ftell (ctx->fp) - PREV->content->offset - 1;
      if (PREV->content->length < 0)
	PREV->content->length = 0;
    }

    if (!PREV->lines)
      PREV->lines = lines ? lines - 1 : 0;
  }

  return (0);
}

#undef PREV

/* open a mbox or mmdf style mailbox */
int mbox_open_mailbox (CONTEXT *ctx)
{
  int rc;

  if ((ctx->fp = fopen (ctx->path, "r")) == NULL)
  {
    mutt_perror (ctx->path);
    return (-1);
  }
  mutt_block_signals ();
  if (mbox_lock_mailbox (ctx, 0, 1) == -1)
  {
    mutt_unblock_signals ();
    return (-1);
  }

  if (ctx->magic == M_MBOX)
    rc = mbox_parse_mailbox (ctx);
  else if (ctx->magic == M_MMDF || ctx->magic == M_KENDRA)
    rc = mmdf_parse_mailbox (ctx);
  else
    rc = -1;

  mbox_unlock_mailbox (ctx);
  mutt_unblock_signals ();
  return (rc);
}

/* return 1 if address lists are strictly identical */
static int strict_addrcmp (const ADDRESS *a, const ADDRESS *b)
{
  while (a && b)
  {
    if (mutt_strcmp (a->mailbox, b->mailbox) ||
	mutt_strcmp (a->personal, b->personal))
      return (0);

    a = a->next;
    b = b->next;
  }
  if (a || b)
    return (0);

  return (1);
}

static int strict_cmp_lists (const LIST *a, const LIST *b)
{
  while (a && b)
  {
    if (mutt_strcmp (a->data, b->data))
      return (0);

    a = a->next;
    b = b->next;
  }
  if (a || b)
    return (0);

  return (1);
}

static int strict_cmp_envelopes (const ENVELOPE *e1, const ENVELOPE *e2)
{
  if (e1 && e2)
  {
    if (mutt_strcmp (e1->message_id, e2->message_id) ||
	mutt_strcmp (e1->subject, e2->subject) ||
	!strict_cmp_lists (e1->references, e2->references) ||
	!strict_addrcmp (e1->from, e2->from) ||
	!strict_addrcmp (e1->sender, e2->sender) ||
	!strict_addrcmp (e1->reply_to, e2->reply_to) ||
	!strict_addrcmp (e1->to, e2->to) ||
	!strict_addrcmp (e1->cc, e2->cc) ||
	!strict_addrcmp (e1->return_path, e2->return_path))
      return (0);
    else
      return (1);
  }
  else
  {
    if (e1 == NULL && e2 == NULL)
      return (1);
    else
      return (0);
  }
}

static int strict_cmp_parameters (const PARAMETER *p1, const PARAMETER *p2)
{
  while (p1 && p2)
  {
    if (mutt_strcmp (p1->attribute, p2->attribute) ||
	mutt_strcmp (p1->value, p2->value))
      return (0);

    p1 = p1->next;
    p2 = p2->next;
  }
  if (p1 || p2)
    return (0);

  return (1);
}

static int strict_cmp_bodies (const BODY *b1, const BODY *b2)
{
  if (b1->type != b2->type ||
      b1->encoding != b2->encoding ||
      mutt_strcmp (b1->subtype, b2->subtype) ||
      mutt_strcmp (b1->description, b2->description) ||
      !strict_cmp_parameters (b1->parameter, b2->parameter) ||
      b1->length != b2->length)
    return (0);
  return (1);
}

/* return 1 if headers are strictly identical */
int mbox_strict_cmp_headers (const HEADER *h1, const HEADER *h2)
{
  if (h1 && h2)
  {
    if (h1->received != h2->received ||
	h1->date_sent != h2->date_sent ||
	h1->content->length != h2->content->length ||
	h1->lines != h2->lines ||
	h1->zhours != h2->zhours ||
	h1->zminutes != h2->zminutes ||
	h1->zoccident != h2->zoccident ||
	h1->mime != h2->mime ||
	!strict_cmp_envelopes (h1->env, h2->env) ||
	!strict_cmp_bodies (h1->content, h2->content))
      return (0);
    else
      return (1);
  }
  else
  {
    if (h1 == NULL && h2 == NULL)
      return (1);
    else
      return (0);
  }
}

/* check to see if the mailbox has changed on disk.
 *
 * return values:
 *	M_REOPENED	mailbox has been reopened
 *	M_NEW_MAIL	new mail has arrived!
 *	M_LOCKED	couldn't lock the file
 *	0		no change
 *	-1		error
 */
int mbox_check_mailbox (CONTEXT *ctx, int *index_hint)
{
  struct stat st;
  char buffer[LONG_STRING];
  int unlock = 0;
  int modified = 0;

  if (stat (ctx->path, &st) == 0)
  {
    if (st.st_mtime == ctx->mtime && st.st_size == ctx->size)
      return (0);

    if (st.st_size == ctx->size)
    {
      /* the file was touched, but it is still the same length, so just exit */
      ctx->mtime = st.st_mtime;
      return (0);
    }

    if (st.st_size > ctx->size)
    {
      /* lock the file if it isn't already */
      if (!ctx->locked)
      {
	mutt_block_signals ();
	if (mbox_lock_mailbox (ctx, 0, 0) == -1)
	{
	  mutt_unblock_signals ();
	  /* we couldn't lock the mailbox, but nothing serious happened:
	   * probably the new mail arrived: no reason to wait till we can
	   * parse it: we'll get it on the next pass
	   */
	  return (M_LOCKED);
	}
	unlock = 1;
      }

      /*
       * Check to make sure that the only change to the mailbox is that 
       * message(s) were appended to this file.  My heuristic is that we should
       * see the message separator at *exactly* what used to be the end of the
       * folder.
       */
      if (fseek (ctx->fp, ctx->size, SEEK_SET) != 0)
	dprint (1, (debugfile, "mbox_check_mailbox: fseek() failed\n"));
      if (fgets (buffer, sizeof (buffer), ctx->fp) != NULL)
      {
	if ((ctx->magic == M_MBOX && mutt_strncmp ("From ", buffer, 5) == 0) ||
	    (ctx->magic == M_MMDF && mutt_strcmp (MMDF_SEP, buffer) == 0) ||
	    (ctx->magic == M_KENDRA && mutt_strcmp(KENDRA_SEP, buffer) == 0))
	{
	  if (fseek (ctx->fp, ctx->size, SEEK_SET) != 0)
	    dprint (1, (debugfile, "mbox_check_mailbox: fseek() failed\n"));
	  if (ctx->magic == M_MBOX)
	    mbox_parse_mailbox (ctx);
	  else
	    mmdf_parse_mailbox (ctx);

	  /* Only unlock the folder if it was locked inside of this routine.
	   * It may have been locked elsewhere, like in
	   * mutt_checkpoint_mailbox().
	   */

	  if (unlock)
	  {
	    mbox_unlock_mailbox (ctx);
	    mutt_unblock_signals ();
	  }

	  return (M_NEW_MAIL); /* signal that new mail arrived */
	}
	else
	  modified = 1;
      }
      else
      {
	dprint (1, (debugfile, "mbox_check_mailbox: fgets returned NULL.\n"));
	modified = 1;
      }
    }
    else
      modified = 1;
  }

  if (modified)
  {
    if (mutt_reopen_mailbox (ctx, index_hint) != -1)
    {
      if (unlock)
      {
	mbox_unlock_mailbox (ctx);
	mutt_unblock_signals ();
      }
      return (M_REOPENED);
    }
  }

  /* fatal error */

  mbox_unlock_mailbox (ctx);
  mx_fastclose_mailbox (ctx);
  mutt_unblock_signals ();
  mutt_error _("Mailbox was corrupted!");
  return (-1);
}

/* return values:
 *	0	success
 *	-1	failure
 */
int mbox_sync_mailbox (CONTEXT *ctx)
{
  char tempfile[_POSIX_PATH_MAX];
  char buf[32];
  int i, j, save_sort = SORT_ORDER;
  int need_sort = 0; /* flag to resort mailbox if new mail arrives */
  int first;	/* first message to be written */
  long offset;	/* location in mailbox to write changed messages */
  struct stat statbuf;
  struct utimbuf utimebuf;
  struct m_update_t *newOffset = NULL;
  FILE *fp = NULL;

  /* sort message by their position in the mailbox on disk */
  if (Sort != SORT_ORDER)
  {
    save_sort = Sort;
    Sort = SORT_ORDER;
    mutt_sort_headers (ctx, 0);
  }

  /* need to open the file for writing in such a way that it does not truncate
   * the file, so use read-write mode.
   */
  if ((ctx->fp = freopen (ctx->path, "r+", ctx->fp)) == NULL)
  {
    mx_fastclose_mailbox (ctx);
    mutt_error _("Fatal error!  Could not reopen mailbox!");
    return (-1);
  }

  mutt_block_signals ();

  if (mbox_lock_mailbox (ctx, 1, 1) == -1)
  {
    mutt_unblock_signals ();
    mutt_error _("Unable to lock mailbox!");
    goto bail;
  }

  /* Check to make sure that the file hasn't changed on disk */
  if ((i = mbox_check_mailbox (ctx, NULL)) == M_NEW_MAIL ||  i == M_REOPENED)
  {
    /* new mail arrived, or mailbox reopened */
    need_sort = i;
    goto bail;
  }
  else if (i < 0)
  {
    /* fatal error */
    Sort = save_sort;
    return (-1);
  }

  /* Create a temporary file to write the new version of the mailbox in. */
  mutt_mktemp (tempfile);
  if ((i = open (tempfile, O_WRONLY | O_EXCL | O_CREAT, 0600)) == -1 ||
      (fp = fdopen (i, "w")) == NULL)
  {
    if (-1 != i)
    {
      close (i);
      unlink (tempfile);
    }
    mutt_error _("Could not create temporary file!");
    goto bail;
  }

  /* find the first deleted/changed message.  we save a lot of time by only
   * rewriting the mailbox from the point where it has actually changed.
   */
  for (i = 0 ; i < ctx->msgcount && !ctx->hdrs[i]->deleted && 
               !ctx->hdrs[i]->changed && !ctx->hdrs[i]->attach_del; i++)
    ;
  if (i == ctx->msgcount)
  { 
    /* this means ctx->changed or ctx->deleted was set, but no
     * messages were found to be changed or deleted.  This should
     * never happen, is we presume it is a bug in mutt.
     */
    mutt_error _("sync: mbox modified, but no modified messages! (report this bug)");
    sleep(5); /* the mutt_error /will/ get cleared! */
    dprint(1, (debugfile, "mbox_sync_mailbox(): no modified messages.\n"));
    unlink (tempfile);
    goto bail;
  }

    /* save the index of the first changed/deleted message */
  first = i; 
  /* where to start overwriting */
  offset = ctx->hdrs[i]->offset; 

  /* the offset stored in the header does not include the MMDF_SEP, so make
   * sure we seek to the correct location
   */
  if (ctx->magic == M_MMDF)
    offset -= (sizeof MMDF_SEP - 1);
  else if (ctx->magic == M_KENDRA)
    offset -= (sizeof KENDRA_SEP - 1);
  
  /* allocate space for the new offsets */
  newOffset = safe_calloc (ctx->msgcount - first, sizeof (struct m_update_t));

  for (i = first, j = 0; i < ctx->msgcount; i++)
  {
    if (! ctx->hdrs[i]->deleted)
    {
      j++;
      if (!ctx->quiet && WriteInc && ((i % WriteInc) == 0 || j == 1))
	mutt_message (_("Writing messages... %d (%d%%)"), i,
		      ftell (ctx->fp) / (ctx->size / 100 + 1));

      if (ctx->magic == M_MMDF)
      {
	if (fputs (MMDF_SEP, fp) == EOF)
	{
	  unlink (tempfile);
	  goto bail;
	}
	  
      }
      else if (ctx->magic == M_KENDRA)
      {
	if (fputs (KENDRA_SEP, fp) == EOF)
	{
	  unlink (tempfile);
	  goto bail;
	}
      }

      /* save the new offset for this message.  we add `offset' because the
       * temporary file only contains saved message which are located after
       * `offset' in the real mailbox
       */
      newOffset[i - first].hdr = ftell (fp) + offset;

      if (mutt_copy_message (fp, ctx, ctx->hdrs[i], M_CM_UPDATE, CH_FROM | CH_UPDATE | CH_UPDATE_LEN) == -1)
      {
	unlink (tempfile);
	goto bail;
      }

      /* Since messages could have been deleted, the offsets stored in memory
       * will be wrong, so update what we can, which is the offset of this
       * message, and the offset of the body.  If this is a multipart message,
       * we just flush the in memory cache so that the message will be reparsed
       * if the user accesses it later.
       */
      newOffset[i - first].body = ftell (fp) - ctx->hdrs[i]->content->length + offset;
      mutt_free_body (&ctx->hdrs[i]->content->parts);

      switch(ctx->magic)
      {
	case M_MMDF: 
	  if(fputs(MMDF_SEP, fp) == EOF) 
	  {
	    unlink (tempfile);
	    goto bail; 
	  }
	  break;
	case M_KENDRA:
	  if(fputs(KENDRA_SEP, fp) == EOF)
	  {
	    unlink (tempfile);
	    goto bail;
	  }
	  break;
	default:
	  if(fputs("\n", fp) == EOF) 
	  {
	    unlink (tempfile);
	    goto bail;
	  }
      }
    }
  }
  
  if (fclose (fp) != 0)
  {
    fp = NULL;
    dprint(1, (debugfile, "mbox_sync_mailbox: fclose() returned non-zero.\n"));
    unlink (tempfile);
    goto bail;
  }
  fp = NULL;

  /* Save the state of this folder. */
  if (stat (ctx->path, &statbuf) == -1)
  {
    unlink (tempfile);
    goto bail;
  }

  if ((fp = fopen (tempfile, "r")) == NULL)
  {
    mutt_unblock_signals ();
    mx_fastclose_mailbox (ctx);
    dprint (1, (debugfile, "mbox_sync_mailbox: unable to reopen temp copy of mailbox!\n"));
    mutt_perror (tempfile);
    return (-1);
  }

  if (fseek (ctx->fp, offset, SEEK_SET) != 0 ||  /* seek the append location */
      /* do a sanity check to make sure the mailbox looks ok */
      fgets (buf, sizeof (buf), ctx->fp) == NULL ||
      (ctx->magic == M_MBOX && mutt_strncmp ("From ", buf, 5) != 0) ||
      (ctx->magic == M_MMDF && mutt_strcmp (MMDF_SEP, buf) != 0) ||
      (ctx->magic == M_KENDRA && mutt_strcmp (KENDRA_SEP, buf) != 0))
  {
    dprint (1, (debugfile, "mbox_sync_mailbox: message not in expected position."));
    dprint (1, (debugfile, "\tLINE: %s\n", buf));
    i = -1;
  }
  else
  {
    if (fseek (ctx->fp, offset, SEEK_SET) != 0) /* return to proper offset */
    {
      i = -1;
      dprint (1, (debugfile, "mbox_sync_mailbox: fseek() failed\n"));
    }
    else
    {
      /* copy the temp mailbox back into place starting at the first
       * change/deleted message
       */
      i = mutt_copy_stream (fp, ctx->fp);

      if (ferror (ctx->fp))
        i = -1;
    }
    if (i == 0)
    {
      ctx->size = ftell (ctx->fp); /* update the size of the mailbox */
      ftruncate (fileno (ctx->fp), ctx->size);
    }
  }

  fclose (fp);
  fp = NULL;
  mbox_unlock_mailbox (ctx);

  if (fclose (ctx->fp) != 0 || i == -1)
  {
    /* error occured while writing the mailbox back, so keep the temp copy
     * around
     */
    
    char savefile[_POSIX_PATH_MAX];
    
    snprintf (savefile, sizeof (savefile), "%s/mutt.%s-%s-%d",
	      NONULL (Tempdir), NONULL(Username), NONULL(Hostname), getpid ());
    rename (tempfile, savefile);
    mutt_unblock_signals ();
    mx_fastclose_mailbox (ctx);
    mutt_pretty_mailbox (savefile);
    mutt_error (_("Write failed!  Saved partial mailbox to %s"), savefile);
    return (-1);
  }

  /* Restore the previous access/modification times */
  utimebuf.actime = statbuf.st_atime;
  utimebuf.modtime = statbuf.st_mtime;
  utime (ctx->path, &utimebuf);

  /* reopen the mailbox in read-only mode */
  if ((ctx->fp = fopen (ctx->path, "r")) == NULL)
  {
    unlink (tempfile);
    mutt_unblock_signals ();
    mx_fastclose_mailbox (ctx);
    mutt_error _("Fatal error!  Could not reopen mailbox!");
    Sort = save_sort;
    return (-1);
  }

  /* update the offsets of the rewritten messages */
  for (i = first, j = first; i < ctx->msgcount; i++)
  {
    if (!ctx->hdrs[i]->deleted)
    {
      ctx->hdrs[i]->offset = newOffset[i - first].hdr;
      ctx->hdrs[i]->content->hdr_offset = newOffset[i - first].hdr;
      ctx->hdrs[i]->content->offset = newOffset[i - first].body;
      ctx->hdrs[i]->index = j++;
    }
  }
  safe_free ((void **) &newOffset);
  unlink (tempfile); /* remove partial copy of the mailbox */
  mutt_unblock_signals ();
  Sort = save_sort; /* Restore the default value. */

  return (0); /* signal success */

bail:  /* Come here in case of disaster */

  /* Disadvantage of using "goto" */
  if (fp)
    fclose (fp);

  /* this is ok to call even if we haven't locked anything */
  mbox_unlock_mailbox (ctx);

  mutt_unblock_signals ();
  safe_free ((void **) &newOffset);

  if ((ctx->fp = freopen (ctx->path, "r", ctx->fp)) == NULL)
  {
    mutt_error _("Could not reopen mailbox!");
    mx_fastclose_mailbox (ctx);
    return (-1);
  }

  if (need_sort || save_sort != Sort)
  {
    Sort = save_sort;
    /* if the mailbox was reopened, the thread tree will be invalid so make
     * sure to start threading from scratch.  */
    mutt_sort_headers (ctx, (need_sort == M_REOPENED));
  }

  return (-1);
}

/* close a mailbox opened in write-mode */
int mbox_close_mailbox (CONTEXT *ctx)
{
  mx_unlock_file (ctx->path, fileno (ctx->fp), 1);
  mutt_unblock_signals ();
  mx_fastclose_mailbox (ctx);
  return 0;
}

int mutt_reopen_mailbox (CONTEXT *ctx, int *index_hint)
{
  int (*cmp_headers) (const HEADER *, const HEADER *) = NULL;
  HEADER **old_hdrs;
  int old_msgcount;
  int msg_mod = 0;
  int index_hint_set;
  int i, j;
  int rc = -1;

  /* silent operations */
  ctx->quiet = 1;
  
  mutt_message _("Reopening mailbox...");
  
  /* our heuristics require the old mailbox to be unsorted */
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

  ctx->hdrmax = 0;	/* force allocation of new headers */
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

  switch (ctx->magic)
  {
    case M_MBOX:
    case M_MMDF:
    case M_KENDRA:
      if (fseek (ctx->fp, 0, SEEK_SET) != 0)
      {
        dprint (1, (debugfile, "mutt_reopen_mailbox: fseek() failed\n"));
        rc = -1;
      } 
      else 
      {
        cmp_headers = mbox_strict_cmp_headers;
        rc = ((ctx->magic == M_MBOX) ? mbox_parse_mailbox
				     : mmdf_parse_mailbox) (ctx);
      }
      break;

    default:
      rc = -1;
      break;
  }
  
  if (rc == -1)
  {
    /* free the old headers */
    for (j = 0; j < old_msgcount; j++)
      mutt_free_header (&(old_hdrs[j]));
    safe_free ((void **) &old_hdrs);

    ctx->quiet = 0;
    return (-1);
  }

  /* now try to recover the old flags */

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
	if (cmp_headers (ctx->hdrs[i], old_hdrs[j]))
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
	  if (cmp_headers (ctx->hdrs[i], old_hdrs[j]))
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
	   * otherwise, the header may have been modified externally,
	   * and we don't want to lose _those_ changes
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

  return ((ctx->changed || msg_mod) ? M_REOPENED : M_NEW_MAIL);
}

