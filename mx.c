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
#include "mx.h"
#include "rfc2047.h"
#include "sort.h"
#include "mailbox.h"
#include "copy.h"
#include "keymap.h"


#ifdef _PGPPATH
#include "pgp.h"
#endif

#ifdef USE_IMAP
#include "imap.h"
#endif

#ifdef BUFFY_SIZE
#include "buffy.h"
#endif

#ifdef USE_DOTLOCK
#include "dotlock.h"
#endif

#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifndef BUFFY_SIZE
#include <utime.h>
#endif


#define mutt_is_spool(s)  (strcmp (NONULL(Spoolfile), s) == 0)

#ifdef USE_DOTLOCK
/* parameters: 
 * path - file to lock
 * retry - should retry if unable to lock?
 */

#ifdef DL_STANDALONE

static int invoke_dotlock(const char *path, int flags, int retry)
{
  char cmd[LONG_STRING + _POSIX_PATH_MAX];
  char r[SHORT_STRING];
  char *f;
  
  if(flags & DL_FL_RETRY)
    snprintf(r, sizeof(r), "-r %d ", retry ? MAXLOCKATTEMPT : 0);
  
  f = mutt_quote_filename(path);
  
  snprintf(cmd, sizeof(cmd),
	   "%s %s%s%s%s%s%s",
	   DOTLOCK,
	   flags & DL_FL_TRY ? "-t " : "",
	   flags & DL_FL_UNLOCK ? "-u " : "",
	   flags & DL_FL_USEPRIV ? "-p " : "",
	   flags & DL_FL_FORCE ? "-f " : "",
	   flags & DL_FL_RETRY ? r : "",
	   f);
  
  FREE(&f);

  return mutt_system(cmd);
}

#else 

#define invoke_dotlock dotlock_invoke

#endif

static int dotlock_file (const char *path, int retry)
{
  int r;
  int flags = DL_FL_USEPRIV | DL_FL_RETRY;
  
  if(retry) retry = 1;

retry_lock:
  mutt_clear_error();
  if((r = invoke_dotlock(path, flags, retry)) == DL_EX_EXIST)
  {
    char msg[LONG_STRING];

    snprintf(msg, sizeof(msg), "Lock count exceeded, remove lock for %s?",
	     path);
    if(retry && mutt_yesorno(msg, 1) == 1)
    {
      flags |= DL_FL_FORCE;
      retry--;
      goto retry_lock;
    }
  }
  return (r == DL_EX_OK ? 0 : -1);
}

static int undotlock_file (const char *path)
{
  return (invoke_dotlock(path, DL_FL_USEPRIV | DL_FL_UNLOCK, 0) == DL_EX_OK ? 
	  0 : -1);
}

#endif /* USE_DOTLOCK */

/* Args:
 *	excl		if excl != 0, request an exclusive lock
 *	dot		if dot != 0, try to dotlock the file
 *	timeout 	should retry locking?
 */
int mx_lock_file (const char *path, int fd, int excl, int dot, int timeout)
{
#if defined (USE_FCNTL) || defined (USE_FLOCK)
  int count;
  int attempt;
  struct stat prev_sb;
#endif
  int r = 0;

#ifdef USE_FCNTL
  struct flock lck;
  

  memset (&lck, 0, sizeof (struct flock));
  lck.l_type = excl ? F_WRLCK : F_RDLCK;
  lck.l_whence = SEEK_SET;

  count = 0;
  attempt = 0;
  while (fcntl (fd, F_SETLK, &lck) == -1)
  {
    struct stat sb;
    dprint(1,(debugfile, "mx_lock_file(): fcntl errno %d.\n", errno));
    if (errno != EAGAIN && errno != EACCES)
    {
      mutt_perror ("fcntl");
      return (-1);
    }

    if (fstat (fd, &sb) != 0)
     sb.st_size = 0;
    
    if (count == 0)
      prev_sb = sb;

    /* only unlock file if it is unchanged */
    if (prev_sb.st_size == sb.st_size && ++count >= (timeout?MAXLOCKATTEMPT:0))
    {
      if (timeout)
	mutt_error ("Timeout exceeded while attempting fcntl lock!");
      return (-1);
    }

    prev_sb = sb;

    mutt_message ("Waiting for fcntl lock... %d", ++attempt);
    sleep (1);
  }
#endif /* USE_FCNTL */

#ifdef USE_FLOCK
  count = 0;
  attempt = 0;
  while (flock (fd, (excl ? LOCK_EX : LOCK_SH) | LOCK_NB) == -1)
  {
    struct stat sb;
    if (errno != EWOULDBLOCK)
    {
      mutt_perror ("flock");
      r = -1;
      break;
    }

    if (fstat(fd,&sb) != 0 )
     sb.st_size=0;
    
    if (count == 0)
      prev_sb=sb;

    /* only unlock file if it is unchanged */
    if (prev_sb.st_size == sb.st_size && ++count >= (timeout?MAXLOCKATTEMPT:0))
    {
      if (timeout)
	mutt_error ("Timeout exceeded while attempting flock lock!");
      r = -1;
      break;
    }

    prev_sb = sb;

    mutt_message ("Waiting for flock attempt... %d", ++attempt);
    sleep (1);
  }
#endif /* USE_FLOCK */

#ifdef USE_DOTLOCK
  if (r == 0 && dot)
    r = dotlock_file (path,timeout);
#endif /* USE_DOTLOCK */

  if (r == -1)
  {
    /* release any other locks obtained in this routine */

#ifdef USE_FCNTL
    lck.l_type = F_UNLCK;
    fcntl (fd, F_SETLK, &lck);
#endif /* USE_FCNTL */

#ifdef USE_FLOCK
    flock (fd, LOCK_UN);
#endif /* USE_FLOCK */

    return (-1);
  }

  return 0;
}

int mx_unlock_file (const char *path, int fd)
{
#ifdef USE_FCNTL
  struct flock unlockit = { F_UNLCK, 0, 0, 0 };

  memset (&unlockit, 0, sizeof (struct flock));
  unlockit.l_type = F_UNLCK;
  unlockit.l_whence = SEEK_SET;
  fcntl (fd, F_SETLK, &unlockit);
#endif

#ifdef USE_FLOCK
  flock (fd, LOCK_UN);
#endif

#ifdef USE_DOTLOCK
  undotlock_file (path);
#endif
  
  return 0;
}

/* open a file and lock it */
FILE *mx_open_file_lock (const char *path, const char *mode)
{
  FILE *f;

  if ((f = safe_fopen (path, mode)) != NULL)
  {
    if (mx_lock_file (path, fileno (f), *mode != 'r', 1, 1) != 0)
    {
      fclose (f);
      f = NULL;
    }
  }

  return (f);
}

/* try to figure out what type of mailbox ``path'' is
 *
 * return values:
 *	M_*	mailbox type
 *	0	not a mailbox
 *	-1	error
 */

#ifdef USE_IMAP

int mx_is_imap(const char *p)
{
  return p && (*p == '{');
}

#endif

int mx_get_magic (const char *path)
{
  struct stat st;
  int magic = 0;
  char tmp[_POSIX_PATH_MAX];
  FILE *f;

#ifdef USE_IMAP
  if(mx_is_imap(path))
    return M_IMAP;
#endif /* USE_IMAP */

  if (stat (path, &st) == -1)
  {
    dprint (1, (debugfile, "mx_get_magic(): unable to stat %s: %s (errno %d).\n",
		path, strerror (errno), errno));
    return (-1);
  }

  if (S_ISDIR (st.st_mode))
  {
    /* check for maildir-style mailbox */

    snprintf (tmp, sizeof (tmp), "%s/cur", path);
    if (stat (tmp, &st) == 0 && S_ISDIR (st.st_mode))
      return (M_MAILDIR);

    /* check for mh-style mailbox */

    snprintf (tmp, sizeof (tmp), "%s/.mh_sequences", path);
    if (access (tmp, F_OK) == 0)
      return (M_MH);

    snprintf (tmp, sizeof (tmp), "%s/.xmhcache", path);
    if (access (tmp, F_OK) == 0)
      return (M_MH);
  }
  else if (st.st_size == 0)
  {
    /* hard to tell what zero-length files are, so assume the default magic */
    if (DefaultMagic == M_MBOX || DefaultMagic == M_MMDF)
      return (DefaultMagic);
    else
      return (M_MBOX);
  }
  else if ((f = fopen (path, "r")) != NULL)
  {
#ifndef BUFFY_SIZE
    struct utimbuf times;
#endif

    fgets (tmp, sizeof (tmp), f);
    if (strncmp ("From ", tmp, 5) == 0)
      magic = M_MBOX;
    else if (strcmp (MMDF_SEP, tmp) == 0)
      magic = M_MMDF;
    fclose (f);
#ifndef BUFFY_SIZE
    /* need to restore the times here, the file was not really accessed,
     * only the type was accessed.  This is important, because detection
     * of "new mail" depends on those times set correctly.
     */
    times.actime = st.st_atime;
    times.modtime = st.st_mtime;
    utime (path, &times);
#endif
  }
  else
  {
    dprint (1, (debugfile, "mx_get_magic(): unable to open file %s for reading.\n",
		path));
    mutt_perror (path);
    return (-1);
  }

  return (magic);
}

/*
 * set DefaultMagic to the given value
 */
int mx_set_magic (const char *s)
{
  if (strcasecmp (s, "mbox") == 0)
    DefaultMagic = M_MBOX;
  else if (strcasecmp (s, "mmdf") == 0)
    DefaultMagic = M_MMDF;
  else if (strcasecmp (s, "mh") == 0)
    DefaultMagic = M_MH;
  else if (strcasecmp (s, "maildir") == 0)
    DefaultMagic = M_MAILDIR;
  else
    return (-1);

  return 0;
}

static int mx_open_mailbox_append (CONTEXT *ctx)
{
  struct stat sb;

  ctx->append = 1;

#ifdef USE_IMAP
  
  if(mx_is_imap(ctx->path))  
    return imap_open_mailbox_append (ctx);

#endif
  
  if(stat(ctx->path, &sb) == 0)
  {
    ctx->magic = mx_get_magic (ctx->path);
    
    switch (ctx->magic)
    {
      case 0:
	mutt_error ("%s is not a mailbox.", ctx->path);
	/* fall through */
      case -1:
	return (-1);
    }
  }
  else if (errno == ENOENT)
  {
    ctx->magic = DefaultMagic;

    if (ctx->magic == M_MH || ctx->magic == M_MAILDIR)
    {
      char tmp[_POSIX_PATH_MAX];

      if (mkdir (ctx->path, S_IRWXU))
      {
	mutt_perror (tmp);
	return (-1);
      }

      if (ctx->magic == M_MAILDIR)
      {
	snprintf (tmp, sizeof (tmp), "%s/cur", ctx->path);
	if (mkdir (tmp, S_IRWXU))
	{
	  mutt_perror (tmp);
	  rmdir (ctx->path);
	  return (-1);
	}

	snprintf (tmp, sizeof (tmp), "%s/new", ctx->path);
	if (mkdir (tmp, S_IRWXU))
	{
	  mutt_perror (tmp);
	  snprintf (tmp, sizeof (tmp), "%s/cur", ctx->path);
	  rmdir (tmp);
	  rmdir (ctx->path);
	  return (-1);
	}
	snprintf (tmp, sizeof (tmp), "%s/tmp", ctx->path);
	if (mkdir (tmp, S_IRWXU))
	{
	  mutt_perror (tmp);
	  snprintf (tmp, sizeof (tmp), "%s/cur", ctx->path);
	  rmdir (tmp);
	  snprintf (tmp, sizeof (tmp), "%s/new", ctx->path);
	  rmdir (tmp);
	  rmdir (ctx->path);
	  return (-1);
	}
      }
      else
      {
	int i;

	snprintf (tmp, sizeof (tmp), "%s/.mh_sequences", ctx->path);
	if ((i = creat (tmp, S_IRWXU)) == -1)
	{
	  mutt_perror (tmp);
	  rmdir (ctx->path);
	  return (-1);
	}
	close (i);
      }
    }
  }
  else
  {
    mutt_perror (ctx->path);
    return (-1);
  }

  switch (ctx->magic)
  {
    case M_MBOX:
    case M_MMDF:
      if ((ctx->fp = fopen (ctx->path, "a")) == NULL ||
	  mbox_lock_mailbox (ctx, 1, 1) != 0)
      {
	if (!ctx->fp)
	  mutt_perror (ctx->path);
	return (-1);
      }
      fseek (ctx->fp, 0, 2);
      break;

    case M_MH:
    case M_MAILDIR:
      /* nothing to do */
      break;

    default:
      return (-1);
  }

  return 0;
}

/*
 * open a mailbox and parse it
 *
 * Args:
 *	flags	M_NOSORT	do not sort mailbox
 *		M_APPEND	open mailbox for appending
 *		M_READONLY	open mailbox in read-only mode
 *		M_QUIET		only print error messages
 *	ctx	if non-null, context struct to use
 */
CONTEXT *mx_open_mailbox (const char *path, int flags, CONTEXT *pctx)
{
  CONTEXT *ctx = pctx;
  int rc;

  if (!ctx)
    ctx = safe_malloc (sizeof (CONTEXT));
  memset (ctx, 0, sizeof (CONTEXT));
  ctx->path = safe_strdup (path);

  ctx->msgnotreadyet = -1;
  ctx->collapsed = 0;
  
  if (flags & M_QUIET)
    ctx->quiet = 1;
  if (flags & M_READONLY)
    ctx->readonly = 1;

  if (flags & M_APPEND)
  {
    if (mx_open_mailbox_append (ctx) != 0)
    {
      mx_fastclose_mailbox (ctx);
      if (!pctx)
	safe_free ((void **) &ctx);
      return NULL;
    }
    return ctx;
  }

  ctx->magic = mx_get_magic (path);
  
  if(ctx->magic == 0)
    mutt_error ("%s is not a mailbox.", path);

  if(ctx->magic == -1)
    mutt_perror(path);
  
  if(ctx->magic <= 0)
  {
    mx_fastclose_mailbox (ctx);
    if (!pctx)
      FREE (&ctx);
    return (NULL);
  }
  
  /* if the user has a `push' command in their .muttrc, or in a folder-hook,
   * it will cause the progress messages not to be displayed because
   * mutt_refresh() will think we are in the middle of a macro.  so set a
   * flag to indicate that we should really refresh the screen.
   */
  set_option (OPTFORCEREFRESH);

  /* create hash tables */
  ctx->id_hash = hash_create (257);
  ctx->subj_hash = hash_create (257);

  if (!ctx->quiet)
    mutt_message ("Reading %s...", ctx->path);

  switch (ctx->magic)
  {
    case M_MH:
      rc = mh_read_dir (ctx, NULL);
      break;

    case M_MAILDIR:
      rc = maildir_read_dir (ctx);
      break;

    case M_MMDF:
    case M_MBOX:
      rc = mbox_open_mailbox (ctx);
      break;

#ifdef USE_IMAP
    case M_IMAP:
      rc = imap_open_mailbox (ctx);
      break;
#endif /* USE_IMAP */

    default:
      rc = -1;
      break;
  }

  if (rc == 0)
  {
    if ((flags & M_NOSORT) == 0)
    {
      /* avoid unnecessary work since the mailbox is completely unthreaded
	 to begin with */
      unset_option (OPTSORTSUBTHREADS);
      unset_option (OPTNEEDRESCORE);
      mutt_sort_headers (ctx, 1);
    }
    if (!ctx->quiet)
      mutt_clear_error ();
  }
  else
  {
    mx_fastclose_mailbox (ctx);
    if (!pctx)
      safe_free ((void **) &ctx);
  }

  unset_option (OPTFORCEREFRESH);
  return (ctx);
}

/* free up memory associated with the mailbox context */
void mx_fastclose_mailbox (CONTEXT *ctx)
{
  int i;
  
#ifdef USE_IMAP
  if (ctx->magic == M_IMAP)
    imap_fastclose_mailbox (ctx);
#endif /* USE_IMAP */
  if (ctx->subj_hash)
    hash_destroy (&ctx->subj_hash, NULL);
  if (ctx->id_hash)
    hash_destroy (&ctx->id_hash, NULL);
  for (i = 0; i < ctx->msgcount; i++)
    mutt_free_header (&ctx->hdrs[i]);
  safe_free ((void **) &ctx->hdrs);
  safe_free ((void **) &ctx->v2r);
  safe_free ((void **) &ctx->path);
  safe_free ((void **) &ctx->pattern);
  if (ctx->limit_pattern) 
    mutt_pattern_free (&ctx->limit_pattern);
  if (ctx->fp)
    fclose (ctx->fp);
  memset (ctx, 0, sizeof (CONTEXT));
}

/* save changes to disk */
static int sync_mailbox (CONTEXT *ctx)
{
#ifdef BUFFY_SIZE
  BUFFY *tmp = NULL;
#endif
  int rc = -1;

  if (!ctx->quiet)
    mutt_message ("Writing %s...", ctx->path);
  switch (ctx->magic)
  {
    case M_MBOX:
    case M_MMDF:
      rc = mbox_sync_mailbox (ctx);
#ifdef BUFFY_SIZE
      tmp = mutt_find_mailbox (ctx->path);
#endif
      break;
      
    case M_MH:
    case M_MAILDIR:
      rc = mh_sync_mailbox (ctx);
      break;
      
#ifdef USE_IMAP
    case M_IMAP:
      rc = imap_sync_mailbox (ctx);
      break;
#endif /* USE_IMAP */
  }

#ifdef BUFFY_SIZE
  if (tmp && tmp->new == 0)
    mutt_update_mailbox (tmp);
#endif
  return rc;
}

/* save changes and close mailbox */
int mx_close_mailbox (CONTEXT *ctx)
{
  int i, move_messages = 0, purge = 1, read_msgs = 0;
  int isSpool = 0;
  CONTEXT f;
  char mbox[_POSIX_PATH_MAX];
  char buf[SHORT_STRING];

  if (ctx->readonly || ctx->dontwrite)
  {
    /* mailbox is readonly or we don't want to write */
    mx_fastclose_mailbox (ctx);
    return 0;
  }

  if (ctx->append)
  {
    /* mailbox was opened in write-mode */
    if (ctx->magic == M_MBOX || ctx->magic == M_MMDF)
      mbox_close_mailbox (ctx);
    else
      mx_fastclose_mailbox (ctx);
    return 0;
  }

  for (i = 0; i < ctx->msgcount; i++)
  {
    if (!ctx->hdrs[i]->deleted && ctx->hdrs[i]->read)
      read_msgs++;
  }

  if (read_msgs && quadoption (OPT_MOVE) != M_NO)
  {
    char *p;

    if ((p = mutt_find_hook (M_MBOXHOOK, ctx->path)))
    {
      isSpool = 1;
      strfcpy (mbox, p, sizeof (mbox));
    }
    else
    {
      strfcpy (mbox, NONULL(Inbox), sizeof (mbox));
      isSpool = mutt_is_spool (ctx->path) && !mutt_is_spool (mbox);
    }
    mutt_expand_path (mbox, sizeof (mbox));

    if (isSpool)
    {
      snprintf (buf, sizeof (buf), "Move read messages to %s?", mbox);
      if ((move_messages = query_quadoption (OPT_MOVE, buf)) == -1)
	return (-1);
    }
  }

  if (ctx->deleted)
  {
    snprintf (buf, sizeof (buf), "Purge %d deleted message%s?",
	      ctx->deleted, ctx->deleted == 1 ? "" : "s");
    if ((purge = query_quadoption (OPT_DELETE, buf)) < 0)
      return (-1);
  }

  if (option (OPTMARKOLD))
  {
    for (i = 0; i < ctx->msgcount; i++)
    {
      if (!ctx->hdrs[i]->deleted && !ctx->hdrs[i]->old)
	mutt_set_flag (ctx, ctx->hdrs[i], M_OLD, 1);
    }
  }

  if (move_messages)
  {
    if (mx_open_mailbox (mbox, M_APPEND, &f) == NULL)
      return (-1);

    mutt_message ("Moving read messages to %s...", mbox);

    for (i = 0; i < ctx->msgcount; i++)
    {
      if (ctx->hdrs[i]->read && !ctx->hdrs[i]->deleted)
      {
	mutt_append_message (&f, ctx, ctx->hdrs[i], 0, CH_UPDATE_LEN);
	ctx->hdrs[i]->deleted = 1;
	ctx->deleted++;
      }
    }

    mx_close_mailbox (&f);
  }
  else if (!ctx->changed && ctx->deleted == 0)
  {
    mutt_message ("Mailbox is unchanged.");
    mx_fastclose_mailbox (ctx);
    return 0;
  }
  
  if (!purge)
  {
    for (i = 0; i < ctx->msgcount; i++)
      ctx->hdrs[i]->deleted = 0;
    ctx->deleted = 0;
  }

  if (ctx->changed || ctx->deleted)
  {
    if (sync_mailbox (ctx) == -1)
      return (-1);
  }

  if (move_messages)
    mutt_message ("%d kept, %d moved, %d deleted.",
		  ctx->msgcount - ctx->deleted, read_msgs, ctx->deleted);
  else
    mutt_message ("%d kept, %d deleted.",
		  ctx->msgcount - ctx->deleted, ctx->deleted);

  if (ctx->msgcount == ctx->deleted &&
      (ctx->magic == M_MMDF || ctx->magic == M_MBOX) &&
      !mutt_is_spool(ctx->path) && !option (OPTSAVEEMPTY))
    unlink (ctx->path);

  mx_fastclose_mailbox (ctx);

  return 0;
}

/* save changes to mailbox
 *
 * return values:
 *	0		success
 *	-1		error
 */
int mx_sync_mailbox (CONTEXT *ctx)
{
  int rc, i, j;

  if (ctx->dontwrite)
  {
    char buf[STRING], tmp[STRING];
    if (km_expand_key (buf, sizeof(buf),
                       km_find_func (MENU_MAIN, OP_TOGGLE_WRITE)))
      snprintf (tmp, sizeof(tmp), " Press '%s' to toggle write", buf);
    else
      strfcpy (tmp, "Use 'toggle-write' to re-enable write!", sizeof(tmp));

    mutt_error ("Mailbox is marked unwritable. %s", tmp);
    return -1;
  }
  else if (ctx->readonly)
  {
    mutt_error ("Mailbox is read-only.");
    return -1;
  }

  if (!ctx->changed && !ctx->deleted)
  {
    mutt_message ("Mailbox is unchanged.");
    return (0);
  }

  if (ctx->deleted)
  {
    char buf[SHORT_STRING];

    snprintf (buf, sizeof (buf), "Purge %d deleted message%s?",
	      ctx->deleted, ctx->deleted == 1 ? "" : "s");
    if ((rc = query_quadoption (OPT_DELETE, buf)) < 0)
      return (-1);
    else if (rc == M_NO)
    {
      if (!ctx->changed)
	return 0; /* nothing to do! */
      for (i = 0 ; i < ctx->msgcount ; i++)
	ctx->hdrs[i]->deleted = 0;
      ctx->deleted = 0;
    }
  }

  if ((rc = sync_mailbox (ctx)) == 0)
  {
    mutt_message ("%d kept, %d deleted.", ctx->msgcount - ctx->deleted,
		  ctx->deleted);
    sleep (1); /* allow the user time to read the message */

    if (ctx->msgcount == ctx->deleted &&
	(ctx->magic == M_MBOX || ctx->magic == M_MMDF) &&
	!mutt_is_spool(ctx->path) && !option (OPTSAVEEMPTY))
    {
      unlink (ctx->path);
      mx_fastclose_mailbox (ctx);
      return 0;
    }

    /* update memory to reflect the new state of the mailbox */
    ctx->vcount = 0;
    ctx->vsize = 0;
    ctx->tagged = 0;
    ctx->deleted = 0;
    ctx->new = 0;
    ctx->unread = 0;
    ctx->changed = 0;
    ctx->flagged = 0;
#define this_body ctx->hdrs[j]->content
    for (i = 0, j = 0; i < ctx->msgcount; i++)
    {
      if (!ctx->hdrs[i]->deleted)
      {
	if (i != j)
	{
	  ctx->hdrs[j] = ctx->hdrs[i];
	  ctx->hdrs[i] = NULL;
	}
	ctx->hdrs[j]->msgno = j;
	if (ctx->hdrs[j]->virtual != -1)
	{
	  ctx->v2r[ctx->vcount] = j;
	  ctx->hdrs[j]->virtual = ctx->vcount++;
	  ctx->vsize += this_body->length + this_body->offset -
	                this_body->hdr_offset;
	}
	ctx->hdrs[j]->changed = 0;
	if (ctx->hdrs[j]->tagged)
	  ctx->tagged++;
	if (ctx->hdrs[j]->flagged)
	  ctx->flagged++;
	if (!ctx->hdrs[j]->read)
	{ 
	  ctx->unread++;
	  if (!ctx->hdrs[j]->old)
	    ctx->new++;
	} 
	j++;
      }
      else
      {
	if (ctx->magic == M_MH || ctx->magic == M_MAILDIR)
	 ctx->size -= (ctx->hdrs[i]->content->length +
	               ctx->hdrs[i]->content->offset -
		       ctx->hdrs[i]->content->hdr_offset);
	/* remove message from the hash tables */
	if (ctx->hdrs[i]->env->real_subj)
	  hash_delete (ctx->subj_hash, ctx->hdrs[i]->env->real_subj, ctx->hdrs[i], NULL);
	if (ctx->hdrs[i]->env->message_id)
	  hash_delete (ctx->id_hash, ctx->hdrs[i]->env->message_id, ctx->hdrs[i], NULL);
	mutt_free_header (&ctx->hdrs[i]);
      }
    }
#undef this_body
    ctx->msgcount = j;

    set_option (OPTSORTCOLLAPSE);
    mutt_sort_headers (ctx, 1); /* rethread from scratch */
    unset_option (OPTSORTCOLLAPSE);
  }

  return (rc);
}

int mh_open_new_message (MESSAGE *msg, CONTEXT *dest, HEADER *hdr)
{
  int hi = 1;
  int fd, n;
  char *cp;
  char path[_POSIX_PATH_MAX];
  DIR *dirp;
  struct dirent *de;

  do
  {
    if ((dirp = opendir (dest->path)) == NULL)
    {
      mutt_perror (dest->path);
      return (-1);
    }

    /* figure out what the next message number is */
    while ((de = readdir (dirp)) != NULL)
    {
      cp = de->d_name;
      while (*cp)
      {
	if (!isdigit ((unsigned char) *cp))
	  break;
	cp++;
      }
      if (!*cp)
      {
	n = atoi (de->d_name);
	if (n > hi)
	  hi = n;
      }
    }
    closedir (dirp);
    hi++;
    snprintf (path, sizeof (path), "%s/%d", dest->path, hi);
    if ((fd = open (path, O_WRONLY | O_EXCL | O_CREAT, 0600)) == -1)
    {
      if (errno != EEXIST)
      {
	mutt_perror (path);
	return (-1);
      }
    }
  }
  while (fd < 0);

  if ((msg->fp = fdopen (fd, "w")) == NULL)
    return (-1);

  return 0;
}

int maildir_open_new_message (MESSAGE *msg, CONTEXT *dest, HEADER *hdr)
{
  char tmp[_POSIX_PATH_MAX];
  char path[_POSIX_PATH_MAX];

  maildir_create_filename (dest->path, hdr, path, tmp);
  if ((msg->fp = safe_fopen (tmp, "w")) == NULL)
    return (-1);
  return 0;
}

int mbox_open_new_message (MESSAGE *msg, CONTEXT *dest, HEADER *hdr)
{
  msg->fp = dest->fp;
  return 0;
}

#ifdef USE_IMAP
int imap_open_new_message (MESSAGE *msg, CONTEXT *dest, HEADER *hdr)
{
  char tmp[_POSIX_PATH_MAX];

  mutt_mktemp(tmp);
  if ((msg->fp = safe_fopen (tmp, "w")) == NULL)
    return (-1);
  msg->path = safe_strdup(tmp);
  msg->ctx = dest;
  return 0;
}
#endif

/* args:
 *	dest	destintation mailbox
 *	hdr	message being copied (required for maildir support, because
 *		the filename depends on the message flags)
 */
MESSAGE *mx_open_new_message (CONTEXT *dest, HEADER *hdr, int flags)
{
  MESSAGE *msg;
  int (*func) (MESSAGE *, CONTEXT *, HEADER *);
  ADDRESS *p = NULL;
  time_t t;

  switch (dest->magic)
  {
    case M_MMDF:
    case M_MBOX:
      func = mbox_open_new_message;
      break;
    case M_MAILDIR:
      func = maildir_open_new_message;
      break;
    case M_MH:
      func = mh_open_new_message;
      break;
#ifdef USE_IMAP
    case M_IMAP:
      func = imap_open_new_message;
      break;
#endif
    default:
      dprint (1, (debugfile, "mx_open_new_message(): function unimplemented for mailbox type %d.\n",
		  dest->magic));
      return (NULL);
  }

  msg = safe_calloc (1, sizeof (MESSAGE));
  msg->magic = dest->magic;
  msg->write = 1;

  if (func (msg, dest, hdr) == 0)
  {
    if (dest->magic == M_MMDF)
      fputs (MMDF_SEP, msg->fp);

    if ((msg->magic != M_MAILDIR) && ((msg->magic != M_MH) && msg->magic != M_IMAP) && 
	(flags & M_ADD_FROM))
    {
      if (hdr)
      {
	if (hdr->env->return_path)
	  p = hdr->env->return_path;
	else if (hdr->env->sender)
	  p = hdr->env->sender;
	else
	  p = hdr->env->from;

	if (!hdr->received)
	  hdr->received = time (NULL);
	t = hdr->received;
      }
      else
	t = time (NULL);

      fprintf (msg->fp, "From %s %s", p ? p->mailbox : NONULL(Username), ctime (&t));
    }
  }
  else
    safe_free ((void **) &msg);

  return msg;
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
  
  mutt_message ("Reopening mailbox...");
  
  /* our heuristics require the old mailbox to be unsorted */
  if (Sort != SORT_ORDER)
  {
    short old_sort;

    old_sort = Sort;
    Sort = SORT_ORDER;
    mutt_sort_headers (ctx, 1);
    Sort = old_sort;
  }

  /* save the old headers */
  old_msgcount = ctx->msgcount;
  old_hdrs = ctx->hdrs;

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
    ctx->hdrs = NULL;

  ctx->hdrmax = 0;	/* force allocation of new headers */
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

  switch (ctx->magic)
  {
    case M_MBOX:
      fseek (ctx->fp, 0, 0);
      cmp_headers = mbox_strict_cmp_headers;
      rc = mbox_parse_mailbox (ctx);
      break;

    case M_MMDF:
      fseek (ctx->fp, 0, 0);
      cmp_headers = mbox_strict_cmp_headers;
      rc = mmdf_parse_mailbox (ctx);
      break;

    case M_MH:
      /* cmp_headers = mh_strict_cmp_headers; */
      rc = mh_read_dir (ctx, NULL);
      break;

    case M_MAILDIR:
      /* cmp_headers = maildir_strict_cmp_headers; */
      rc = maildir_read_dir (ctx);
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
	for (j = 0; j < i; j++)
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

/* check for new mail */
int mx_check_mailbox (CONTEXT *ctx, int *index_hint)
{
  if (ctx)
  {
    switch (ctx->magic)
    {
      case M_MBOX:
      case M_MMDF:
	return (mbox_check_mailbox (ctx, index_hint));

      case M_MH:
      case M_MAILDIR:
	return (mh_check_mailbox (ctx, index_hint));

#ifdef USE_IMAP
      case M_IMAP:
	return (imap_check_mailbox (ctx, index_hint));
#endif /* USE_IMAP */
    }
  }

  dprint (1, (debugfile, "mx_check_mailbox: null or invalid context.\n"));
  return (-1);
}

/* return a stream pointer for a message */
MESSAGE *mx_open_message (CONTEXT *ctx, int msgno)
{
  MESSAGE *msg;
  
  msg = safe_calloc (1, sizeof (MESSAGE));
  switch (msg->magic = ctx->magic)
  {
    case M_MBOX:
    case M_MMDF:
      msg->fp = ctx->fp;
      break;

    case M_MH:
    case M_MAILDIR:
      {
	HEADER *cur = ctx->hdrs[msgno];
	char path[_POSIX_PATH_MAX];

	snprintf (path, sizeof (path), "%s/%s", ctx->path, cur->path);
	if ((msg->fp = fopen (path, "r")) == NULL)
	{
	  mutt_perror (path);
	  dprint (1, (debugfile, "mx_open_message: fopen: %s: %s (errno %d).\n",
		      path, strerror (errno), errno));
	  FREE (&msg);
	}
      }
      break;

#ifdef USE_IMAP
    case M_IMAP:
      if (imap_fetch_message (msg, ctx, msgno) != 0)
	FREE (&msg);
      break;
#endif /* USE_IMAP */

    default:

      dprint (1, (debugfile, "mx_open_message(): function not implemented for mailbox type %d.\n", ctx->magic));
      FREE (&msg);
      break;
  }
  return (msg);
}

/* close a pointer to a message */
int mx_close_message (MESSAGE **msg)
{
  int r = 0;

  if ((*msg)->write)
  {
    /* add the message terminator */
    switch ((*msg)->magic)
    {
      case M_MMDF:
	fputs (MMDF_SEP, (*msg)->fp);
	break;

      case M_MBOX:
	fputc ('\n', (*msg)->fp);
	break;
    }
  }

  switch ((*msg)->magic) 
  {
    case M_MH:
    case M_MAILDIR:
      r = fclose ((*msg)->fp);
      break;

#ifdef USE_IMAP
    case M_IMAP:
      r = fclose ((*msg)->fp);
      if ((*msg)->write && (*msg)->ctx->append)
      {
	r = imap_append_message ((*msg)->ctx, *msg);
	unlink ((*msg)->path);
      }
#endif

    default:
      break;
  }

  FREE (msg);
  return (r);
}

void mx_alloc_memory (CONTEXT *ctx)
{
  int i;

  if (ctx->hdrs)
  {
    safe_realloc ((void **) &ctx->hdrs, sizeof (HEADER *) * (ctx->hdrmax += 25));
    safe_realloc ((void **) &ctx->v2r, sizeof (int) * ctx->hdrmax);
  }
  else
  {
    ctx->hdrs = safe_malloc (sizeof (HEADER *) * (ctx->hdrmax += 25));
    ctx->v2r = safe_malloc (sizeof (int) * ctx->hdrmax);
  }
  for (i = ctx->msgcount ; i < ctx->hdrmax ; i++)
  {
    ctx->hdrs[i] = NULL;
    ctx->v2r[i] = -1;
  }
}

/* this routine is called to update the counts in the context structure for
 * the last message header parsed.
 */
void mx_update_context (CONTEXT *ctx)
{
  HEADER *h = ctx->hdrs[ctx->msgcount];




#ifdef _PGPPATH
  /* NOTE: this _must_ be done before the check for mailcap! */
  h->pgp = pgp_query (h->content);
  if (!h->pgp)
#endif /* _PGPPATH */



    if (mutt_needs_mailcap (h->content))
      h->mailcap = 1;
  if (h->flagged)
    ctx->flagged++;
  if (h->deleted)
    ctx->deleted++;
  if (!h->read)
  {
    ctx->unread++;
    if (!h->old)
      ctx->new++;
  }
  if (!ctx->pattern)
  {
    ctx->v2r[ctx->vcount] = ctx->msgcount;
    h->virtual = ctx->vcount++;
  }
  else
    h->virtual = -1;
  h->msgno = ctx->msgcount;
  ctx->msgcount++;

  if (h->env->supersedes)
  {
    HEADER *h2 = hash_find (ctx->id_hash, h->env->supersedes);

    /* safe_free (&h->env->supersedes); should I ? */
    if (h2)
    {
      h2->superseded = 1;
      mutt_score_message (h2);
    }
  }

  /* add this message to the hash tables */
  if (h->env->message_id)
    hash_insert (ctx->id_hash, h->env->message_id, h, 0);
  if (h->env->real_subj)
    hash_insert (ctx->subj_hash, h->env->real_subj, h, 1);

  mutt_score_message (h);
}
