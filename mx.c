/*
 * Copyright (C) 1996-2002 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2003 Thomas Roessler <roessler@does-not-exist.org>
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
 */ 

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mx.h"
#include "rfc2047.h"
#include "sort.h"
#include "mailbox.h"
#include "copy.h"
#include "keymap.h"
#include "url.h"

#ifdef USE_IMAP
#include "imap.h"
#endif

#ifdef USE_POP
#include "pop.h"
#endif

#include "buffy.h"

#ifdef USE_DOTLOCK
#include "dotlock.h"
#endif

#include "mutt_crypt.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <utime.h>


#define mutt_is_spool(s)  (mutt_strcmp (Spoolfile, s) == 0)

#ifdef USE_DOTLOCK
/* parameters: 
 * path - file to lock
 * retry - should retry if unable to lock?
 */

#ifdef DL_STANDALONE

static int invoke_dotlock (const char *path, int dummy, int flags, int retry)
{
  char cmd[LONG_STRING + _POSIX_PATH_MAX];
  char f[SHORT_STRING + _POSIX_PATH_MAX];
  char r[SHORT_STRING];
  
  if (flags & DL_FL_RETRY)
    snprintf (r, sizeof (r), "-r %d ", retry ? MAXLOCKATTEMPT : 0);
  
  mutt_quote_filename (f, sizeof (f), path);
  
  snprintf (cmd, sizeof (cmd),
	    "%s %s%s%s%s%s%s%s",
	    NONULL (MuttDotlock),
	    flags & DL_FL_TRY ? "-t " : "",
	    flags & DL_FL_UNLOCK ? "-u " : "",
	    flags & DL_FL_USEPRIV ? "-p " : "",
	    flags & DL_FL_FORCE ? "-f " : "",
	    flags & DL_FL_UNLINK ? "-d " : "",
	    flags & DL_FL_RETRY ? r : "",
	    f);
  
  return mutt_system (cmd);
}

#else 

#define invoke_dotlock dotlock_invoke

#endif

static int dotlock_file (const char *path, int fd, int retry)
{
  int r;
  int flags = DL_FL_USEPRIV | DL_FL_RETRY;

  if (retry) retry = 1;

retry_lock:
  if ((r = invoke_dotlock(path, fd, flags, retry)) == DL_EX_EXIST)
  {
    if (!option (OPTNOCURSES))
    {
      char msg[LONG_STRING];
      
      snprintf(msg, sizeof(msg), _("Lock count exceeded, remove lock for %s?"),
	       path);
      if(retry && mutt_yesorno(msg, M_YES) == M_YES)
      {
	flags |= DL_FL_FORCE;
	retry--;
	mutt_clear_error ();
	goto retry_lock;
      }
    } 
    else
    {
      mutt_error ( _("Can't dotlock %s.\n"), path);
    }
  }
  return (r == DL_EX_OK ? 0 : -1);
}

static int undotlock_file (const char *path, int fd)
{
  return (invoke_dotlock(path, fd, DL_FL_USEPRIV | DL_FL_UNLOCK, 0) == DL_EX_OK ? 
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
  struct stat sb = { 0 }, prev_sb = { 0 }; /* silence gcc warnings */
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
    dprint(1,(debugfile, "mx_lock_file(): fcntl errno %d.\n", errno));
    if (errno != EAGAIN && errno != EACCES)
    {
      mutt_perror ("fcntl");
      return -1;
    }

    if (fstat (fd, &sb) != 0)
      sb.st_size = 0;
    
    if (count == 0)
      prev_sb = sb;

    /* only unlock file if it is unchanged */
    if (prev_sb.st_size == sb.st_size && ++count >= (timeout?MAXLOCKATTEMPT:0))
    {
      if (timeout)
	mutt_error _("Timeout exceeded while attempting fcntl lock!");
      return -1;
    }

    prev_sb = sb;

    mutt_message (_("Waiting for fcntl lock... %d"), ++attempt);
    sleep (1);
  }
#endif /* USE_FCNTL */

#ifdef USE_FLOCK
  count = 0;
  attempt = 0;
  while (flock (fd, (excl ? LOCK_EX : LOCK_SH) | LOCK_NB) == -1)
  {
    if (errno != EWOULDBLOCK)
    {
      mutt_perror ("flock");
      r = -1;
      break;
    }

    if (fstat(fd, &sb) != 0)
      sb.st_size = 0;
    
    if (count == 0)
      prev_sb = sb;

    /* only unlock file if it is unchanged */
    if (prev_sb.st_size == sb.st_size && ++count >= (timeout?MAXLOCKATTEMPT:0))
    {
      if (timeout)
	mutt_error _("Timeout exceeded while attempting flock lock!");
      r = -1;
      break;
    }

    prev_sb = sb;

    mutt_message (_("Waiting for flock attempt... %d"), ++attempt);
    sleep (1);
  }
#endif /* USE_FLOCK */

#ifdef USE_DOTLOCK
  if (r == 0 && dot)
    r = dotlock_file (path, fd, timeout);
#endif /* USE_DOTLOCK */

  if (r != 0)
  {
    /* release any other locks obtained in this routine */

#ifdef USE_FCNTL
    lck.l_type = F_UNLCK;
    fcntl (fd, F_SETLK, &lck);
#endif /* USE_FCNTL */

#ifdef USE_FLOCK
    flock (fd, LOCK_UN);
#endif /* USE_FLOCK */
  }

  return r;
}

int mx_unlock_file (const char *path, int fd, int dot)
{
#ifdef USE_FCNTL
  struct flock unlockit = { F_UNLCK, 0, 0, 0, 0 };

  memset (&unlockit, 0, sizeof (struct flock));
  unlockit.l_type = F_UNLCK;
  unlockit.l_whence = SEEK_SET;
  fcntl (fd, F_SETLK, &unlockit);
#endif

#ifdef USE_FLOCK
  flock (fd, LOCK_UN);
#endif

#ifdef USE_DOTLOCK
  if (dot)
    undotlock_file (path, fd);
#endif
  
  return 0;
}

static void mx_unlink_empty (const char *path)
{
  int fd;
#ifndef USE_DOTLOCK
  struct stat sb;
#endif

  if ((fd = open (path, O_RDWR)) == -1)
    return;

  if (mx_lock_file (path, fd, 1, 0, 1) == -1)
  {
    close (fd);
    return;
  }

#ifdef USE_DOTLOCK
  invoke_dotlock (path, fd, DL_FL_UNLINK, 1);
#else
  if (fstat (fd, &sb) == 0 && sb.st_size == 0)
    unlink (path);
#endif

  mx_unlock_file (path, fd, 0);
  close (fd);
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
  url_scheme_t scheme;

  if (!p)
    return 0;

  if (*p == '{')
    return 1;

  scheme = url_check_scheme (p);
  if (scheme == U_IMAP || scheme == U_IMAPS)
    return 1;

  return 0;
}

#endif

#ifdef USE_POP
int mx_is_pop (const char *p)
{
  url_scheme_t scheme;

  if (!p)
    return 0;

  scheme = url_check_scheme (p);
  if (scheme == U_POP || scheme == U_POPS)
    return 1;

  return 0;
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

#ifdef USE_POP
  if (mx_is_pop (path))
    return M_POP;
#endif /* USE_POP */

  if (stat (path, &st) == -1)
  {
    dprint (1, (debugfile, "mx_get_magic(): unable to stat %s: %s (errno %d).\n",
		path, strerror (errno), errno));
    return (-1);
  }

  if (S_ISDIR (st.st_mode))
  {
    /* check for maildir-style mailbox */
    if (mx_is_maildir (path))
      return M_MAILDIR;

    /* check for mh-style mailbox */
    if (mx_is_mh (path))
      return M_MH;
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
    struct utimbuf times;

    fgets (tmp, sizeof (tmp), f);
    if (mutt_strncmp ("From ", tmp, 5) == 0)
      magic = M_MBOX;
    else if (mutt_strcmp (MMDF_SEP, tmp) == 0)
      magic = M_MMDF;
    safe_fclose (&f);

    if (!option(OPTCHECKMBOXSIZE))
    {
      /* need to restore the times here, the file was not really accessed,
       * only the type was accessed.  This is important, because detection
       * of "new mail" depends on those times set correctly.
       */
      times.actime = st.st_atime;
      times.modtime = st.st_mtime;
      utime (path, &times);
    }
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
  if (ascii_strcasecmp (s, "mbox") == 0)
    DefaultMagic = M_MBOX;
  else if (ascii_strcasecmp (s, "mmdf") == 0)
    DefaultMagic = M_MMDF;
  else if (ascii_strcasecmp (s, "mh") == 0)
    DefaultMagic = M_MH;
  else if (ascii_strcasecmp (s, "maildir") == 0)
    DefaultMagic = M_MAILDIR;
  else
    return (-1);

  return 0;
}

/* mx_access: Wrapper for access, checks permissions on a given mailbox.
 *   We may be interested in using ACL-style flags at some point, currently
 *   we use the normal access() flags. */
int mx_access (const char* path, int flags)
{
#ifdef USE_IMAP
  if (mx_is_imap (path))
    return imap_access (path, flags);
#endif

  return access (path, flags);
}

static int mx_open_mailbox_append (CONTEXT *ctx, int flags)
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
	mutt_error (_("%s is not a mailbox."), ctx->path);
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
	mutt_perror (ctx->path);
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
    if ((ctx->fp = safe_fopen (ctx->path, flags & M_NEWFOLDER ? "w" : "a")) == NULL ||
	  mbox_lock_mailbox (ctx, 1, 1) != 0)
      {
	if (!ctx->fp)
	  mutt_perror (ctx->path);
	else
	{
	  mutt_error (_("Couldn't lock %s\n"), ctx->path);
	  safe_fclose (&ctx->fp);
	}
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

  for (rc=0; rc < RIGHTSMAX; rc++)
    mutt_bit_set(ctx->rights,rc);

  if (flags & M_QUIET)
    ctx->quiet = 1;
  if (flags & M_READONLY)
    ctx->readonly = 1;

  if (flags & (M_APPEND|M_NEWFOLDER))
  {
    if (mx_open_mailbox_append (ctx, flags) != 0)
    {
      mx_fastclose_mailbox (ctx);
      if (!pctx)
	FREE (&ctx);
      return NULL;
    }
    return ctx;
  }

  ctx->magic = mx_get_magic (path);
  
  if(ctx->magic == 0)
    mutt_error (_("%s is not a mailbox."), path);

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

  if (!ctx->quiet)
    mutt_message (_("Reading %s..."), ctx->path);

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

#ifdef USE_POP
    case M_POP:
      rc = pop_open_mailbox (ctx);
      break;
#endif /* USE_POP */

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
      FREE (&ctx);
  }

  unset_option (OPTFORCEREFRESH);
  return (ctx);
}

/* free up memory associated with the mailbox context */
void mx_fastclose_mailbox (CONTEXT *ctx)
{
  int i;

  if(!ctx) 
    return;

  if (ctx->mx_close)
    ctx->mx_close (ctx);

  if (ctx->subj_hash)
    hash_destroy (&ctx->subj_hash, NULL);
  if (ctx->id_hash)
    hash_destroy (&ctx->id_hash, NULL);
  mutt_clear_threads (ctx);
  for (i = 0; i < ctx->msgcount; i++)
    mutt_free_header (&ctx->hdrs[i]);
  FREE (&ctx->hdrs);
  FREE (&ctx->v2r);
  FREE (&ctx->path);
  FREE (&ctx->pattern);
  if (ctx->limit_pattern) 
    mutt_pattern_free (&ctx->limit_pattern);
  safe_fclose (&ctx->fp);
  memset (ctx, 0, sizeof (CONTEXT));
}

/* save changes to disk */
static int sync_mailbox (CONTEXT *ctx, int *index_hint)
{
  BUFFY *tmp = NULL;
  int rc = -1;

  if (!ctx->quiet)
    mutt_message (_("Writing %s..."), ctx->path);

  switch (ctx->magic)
  {
    case M_MBOX:
    case M_MMDF:
      rc = mbox_sync_mailbox (ctx, index_hint);
      if (option(OPTCHECKMBOXSIZE))
	tmp = mutt_find_mailbox (ctx->path);
      break;
      
    case M_MH:
    case M_MAILDIR:
      rc = mh_sync_mailbox (ctx, index_hint);
      break;
      
#ifdef USE_IMAP
    case M_IMAP:
      /* extra argument means EXPUNGE */
      rc = imap_sync_mailbox (ctx, 1, index_hint);
      break;
#endif /* USE_IMAP */

#ifdef USE_POP
    case M_POP:
      rc = pop_sync_mailbox (ctx, index_hint);
      break;
#endif /* USE_POP */
  }

#if 0
  if (!ctx->quiet && !ctx->shutup && rc == -1)
    mutt_error ( _("Could not synchronize mailbox %s!"), ctx->path);
#endif
  
  if (tmp && tmp->new == 0)
    mutt_update_mailbox (tmp);
  return rc;
}

/* save changes and close mailbox */
int mx_close_mailbox (CONTEXT *ctx, int *index_hint)
{
  int i, move_messages = 0, purge = 1, read_msgs = 0;
  int check;
  int isSpool = 0;
  CONTEXT f;
  char mbox[_POSIX_PATH_MAX];
  char buf[SHORT_STRING];

  if (!ctx) return 0;

  ctx->closing = 1;

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
    if (!ctx->hdrs[i]->deleted && ctx->hdrs[i]->read 
        && !(ctx->hdrs[i]->flagged && option (OPTKEEPFLAGGED)))
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

    if (isSpool && *mbox)
    {
      mutt_expand_path (mbox, sizeof (mbox));
      snprintf (buf, sizeof (buf), _("Move read messages to %s?"), mbox);
      if ((move_messages = query_quadoption (OPT_MOVE, buf)) == -1)
      {
	ctx->closing = 0;
	return (-1);
      }
    }
  }

  /* 
   * There is no point in asking whether or not to purge if we are
   * just marking messages as "trash".
   */
  if (ctx->deleted && !(ctx->magic == M_MAILDIR && option (OPTMAILDIRTRASH)))
  {
    snprintf (buf, sizeof (buf), ctx->deleted == 1
	     ? _("Purge %d deleted message?") : _("Purge %d deleted messages?"),
	      ctx->deleted);
    if ((purge = query_quadoption (OPT_DELETE, buf)) < 0)
    {
      ctx->closing = 0;
      return (-1);
    }
  }

  if (option (OPTMARKOLD))
  {
    for (i = 0; i < ctx->msgcount; i++)
    {
      if (!ctx->hdrs[i]->deleted && !ctx->hdrs[i]->old && !ctx->hdrs[i]->read)
	mutt_set_flag (ctx, ctx->hdrs[i], M_OLD, 1);
    }
  }

  if (move_messages)
  {
    if (!ctx->quiet)
      mutt_message (_("Moving read messages to %s..."), mbox);

#ifdef USE_IMAP
    /* try to use server-side copy first */
    i = 1;
    
    if (ctx->magic == M_IMAP && mx_is_imap (mbox))
    {
      /* tag messages for moving, and clear old tags, if any */
      for (i = 0; i < ctx->msgcount; i++)
	if (ctx->hdrs[i]->read && !ctx->hdrs[i]->deleted
            && !(ctx->hdrs[i]->flagged && option (OPTKEEPFLAGGED))) 
	  ctx->hdrs[i]->tagged = 1;
	else
	  ctx->hdrs[i]->tagged = 0;
      
      i = imap_copy_messages (ctx, NULL, mbox, 1);
    }
    
    if (i == 0) /* success */
      mutt_clear_error ();
    else if (i == -1) /* horrible error, bail */
    {
      ctx->closing=0;
      return -1;
    }
    else /* use regular append-copy mode */
#endif
    {
      if (mx_open_mailbox (mbox, M_APPEND, &f) == NULL)
      {
	ctx->closing = 0;
	return -1;
      }

      for (i = 0; i < ctx->msgcount; i++)
      {
	if (ctx->hdrs[i]->read && !ctx->hdrs[i]->deleted
            && !(ctx->hdrs[i]->flagged && option (OPTKEEPFLAGGED)))
        {
	  if (mutt_append_message (&f, ctx, ctx->hdrs[i], 0, CH_UPDATE_LEN) == 0)
	  {
	    mutt_set_flag (ctx, ctx->hdrs[i], M_DELETE, 1);
	  }
	  else
	  {
	    mx_close_mailbox (&f, NULL);
	    ctx->closing = 0;
	    return -1;
	  }
	}
      }
    
      mx_close_mailbox (&f, NULL);
    }
    
  }
  else if (!ctx->changed && ctx->deleted == 0)
  {
    if (!ctx->quiet)
      mutt_message _("Mailbox is unchanged.");
    mx_fastclose_mailbox (ctx);
    return 0;
  }
  
#ifdef USE_IMAP
  /* allow IMAP to preserve the deleted flag across sessions */
  if (ctx->magic == M_IMAP)
  {
    if ((check = imap_sync_mailbox (ctx, purge, index_hint)) != 0)
    {
      ctx->closing = 0;
      return check;
    }
  }
  else
#endif
  {
    if (!purge)
    {
      for (i = 0; i < ctx->msgcount; i++)
        ctx->hdrs[i]->deleted = 0;
      ctx->deleted = 0;
    }

    if (ctx->changed || ctx->deleted)
    {
      if ((check = sync_mailbox (ctx, index_hint)) != 0)
      {
	ctx->closing = 0;
	return check;
      }
    }
  }

  if (!ctx->quiet)
  {
    if (move_messages)
      mutt_message (_("%d kept, %d moved, %d deleted."),
	ctx->msgcount - ctx->deleted, read_msgs, ctx->deleted);
    else
      mutt_message (_("%d kept, %d deleted."),
	ctx->msgcount - ctx->deleted, ctx->deleted);
  }

  if (ctx->msgcount == ctx->deleted &&
      (ctx->magic == M_MMDF || ctx->magic == M_MBOX) &&
      !mutt_is_spool(ctx->path) && !option (OPTSAVEEMPTY))
    mx_unlink_empty (ctx->path);

  mx_fastclose_mailbox (ctx);

  return 0;
}


/* update a Context structure's internal tables. */

void mx_update_tables(CONTEXT *ctx, int committing)
{
  int i, j;
  
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
    if ((committing && (!ctx->hdrs[i]->deleted || 
			(ctx->magic == M_MAILDIR && option (OPTMAILDIRTRASH)))) ||
	(!committing && ctx->hdrs[i]->active))
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

      if (committing)
	ctx->hdrs[j]->changed = 0;
      else if (ctx->hdrs[j]->changed)
	ctx->changed++;
      
      if (!committing || (ctx->magic == M_MAILDIR && option (OPTMAILDIRTRASH)))
      {
	if (ctx->hdrs[j]->deleted)
	  ctx->deleted++;
      }

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
      if (ctx->subj_hash && ctx->hdrs[i]->env->real_subj)
	hash_delete (ctx->subj_hash, ctx->hdrs[i]->env->real_subj, ctx->hdrs[i], NULL);
      if (ctx->id_hash && ctx->hdrs[i]->env->message_id)
	hash_delete (ctx->id_hash, ctx->hdrs[i]->env->message_id, ctx->hdrs[i], NULL);
      mutt_free_header (&ctx->hdrs[i]);
    }
  }
#undef this_body
  ctx->msgcount = j;
}


/* save changes to mailbox
 *
 * return values:
 *	0		success
 *	-1		error
 */
int mx_sync_mailbox (CONTEXT *ctx, int *index_hint)
{
  int rc, i;
  int purge = 1;
  int msgcount, deleted;

  if (ctx->dontwrite)
  {
    char buf[STRING], tmp[STRING];
    if (km_expand_key (buf, sizeof(buf),
                       km_find_func (MENU_MAIN, OP_TOGGLE_WRITE)))
      snprintf (tmp, sizeof(tmp), _(" Press '%s' to toggle write"), buf);
    else
      strfcpy (tmp, _("Use 'toggle-write' to re-enable write!"), sizeof(tmp));

    mutt_error (_("Mailbox is marked unwritable. %s"), tmp);
    return -1;
  }
  else if (ctx->readonly)
  {
    mutt_error _("Mailbox is read-only.");
    return -1;
  }

  if (!ctx->changed && !ctx->deleted)
  {
    if (!ctx->quiet)
      mutt_message _("Mailbox is unchanged.");
    return (0);
  }

  if (ctx->deleted)
  {
    char buf[SHORT_STRING];

    snprintf (buf, sizeof (buf), ctx->deleted == 1
	     ? _("Purge %d deleted message?") : _("Purge %d deleted messages?"),
	      ctx->deleted);
    if ((purge = query_quadoption (OPT_DELETE, buf)) < 0)
      return (-1);
    else if (purge == M_NO)
    {
      if (!ctx->changed)
	return 0; /* nothing to do! */
#ifdef USE_IMAP
      /* let IMAP servers hold on to D flags */
      if (ctx->magic != M_IMAP)
#endif
      {
        for (i = 0 ; i < ctx->msgcount ; i++)
          ctx->hdrs[i]->deleted = 0;
        ctx->deleted = 0;
      }
    }
    else if (ctx->last_tag && ctx->last_tag->deleted)
      ctx->last_tag = NULL; /* reset last tagged msg now useless */
  }

  /* really only for IMAP - imap_sync_mailbox results in a call to
   * mx_update_tables, so ctx->deleted is 0 when it comes back */
  msgcount = ctx->msgcount;
  deleted = ctx->deleted;

#ifdef USE_IMAP
  if (ctx->magic == M_IMAP)
    rc = imap_sync_mailbox (ctx, purge, index_hint);
  else
#endif
    rc = sync_mailbox (ctx, index_hint);
  if (rc == 0)
  {
#ifdef USE_IMAP
    if (ctx->magic == M_IMAP && !purge)
    {
      if (!ctx->quiet)
        mutt_message _("Mailbox checkpointed.");
    }
    else
#endif
    {
      if (!ctx->quiet)
	mutt_message (_("%d kept, %d deleted."), msgcount - deleted,
		      deleted);
    }

    mutt_sleep (0);
    
    if (ctx->msgcount == ctx->deleted &&
	(ctx->magic == M_MBOX || ctx->magic == M_MMDF) &&
	!mutt_is_spool (ctx->path) && !option (OPTSAVEEMPTY))
    {
      unlink (ctx->path);
      mx_fastclose_mailbox (ctx);
      return 0;
    }

    /* if we haven't deleted any messages, we don't need to resort */
    /* ... except for certain folder formats which need "unsorted" 
     * sort order in order to synchronize folders.
     * 
     * MH and maildir are safe.  mbox-style seems to need re-sorting,
     * at least with the new threading code.
     */
    if (purge || (ctx->magic != M_MAILDIR && ctx->magic != M_MH))
    {
#ifdef USE_IMAP
      /* IMAP does this automatically after handling EXPUNGE */
      if (ctx->magic != M_IMAP)
#endif
      {
	mx_update_tables (ctx, 1);
	mutt_sort_headers (ctx, 1); /* rethread from scratch */
      }
    }
  }

  return (rc);
}


/* {maildir,mh}_open_new_message are in mh.c. */

static int mbox_open_new_message (MESSAGE *msg, CONTEXT *dest, HEADER *hdr)
{
  msg->fp = dest->fp;
  return 0;
}

#ifdef USE_IMAP
static int imap_open_new_message (MESSAGE *msg, CONTEXT *dest, HEADER *hdr)
{
  char tmp[_POSIX_PATH_MAX];

  mutt_mktemp(tmp);
  if ((msg->fp = safe_fopen (tmp, "w")) == NULL)
  {
    mutt_perror (tmp);
    return (-1);
  }
  msg->path = safe_strdup(tmp);
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

  if (hdr)
  {
    msg->flags.flagged = hdr->flagged;
    msg->flags.replied = hdr->replied;
    msg->flags.read    = hdr->read;
    msg->received = hdr->received;
  }

  if(msg->received == 0)
    time(&msg->received);
  
  if (func (msg, dest, hdr) == 0)
  {
    if (dest->magic == M_MMDF)
      fputs (MMDF_SEP, msg->fp);

    if ((msg->magic == M_MBOX || msg->magic ==  M_MMDF) &&
	flags & M_ADD_FROM)
    {
      if (hdr)
      {
	if (hdr->env->return_path)
	  p = hdr->env->return_path;
	else if (hdr->env->sender)
	  p = hdr->env->sender;
	else
	  p = hdr->env->from;
      }

      fprintf (msg->fp, "From %s %s", p ? p->mailbox : NONULL(Username), ctime (&msg->received));
    }
  }
  else
    FREE (&msg);

  return msg;
}

/* check for new mail */
int mx_check_mailbox (CONTEXT *ctx, int *index_hint, int lock)
{
  int rc;

  if (ctx)
  {
    if (ctx->locked) lock = 0;

    switch (ctx->magic)
    {
      case M_MBOX:
      case M_MMDF:

	if (lock)
	{
	  mutt_block_signals ();
	  if (mbox_lock_mailbox (ctx, 0, 0) == -1)
	  {
	    mutt_unblock_signals ();
	    return M_LOCKED;
	  }
	}
	
	rc = mbox_check_mailbox (ctx, index_hint);

	if (lock)
	{
	  mutt_unblock_signals ();
	  mbox_unlock_mailbox (ctx);
	}
	
	return rc;


      case M_MH:
	return (mh_check_mailbox (ctx, index_hint));
      case M_MAILDIR:
	return (maildir_check_mailbox (ctx, index_hint));

#ifdef USE_IMAP
      case M_IMAP:
	return (imap_check_mailbox (ctx, index_hint, 0));
#endif /* USE_IMAP */

#ifdef USE_POP
      case M_POP:
	return (pop_check_mailbox (ctx, index_hint));
#endif /* USE_POP */
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
      
      if ((msg->fp = fopen (path, "r")) == NULL && errno == ENOENT &&
	  ctx->magic == M_MAILDIR)
	msg->fp = maildir_open_find_message (ctx->path, cur->path);
      
      if (msg->fp == NULL)
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
    {
      if (imap_fetch_message (msg, ctx, msgno) != 0)
	FREE (&msg);
      break;
    }
#endif /* USE_IMAP */

#ifdef USE_POP
    case M_POP:
    {
      if (pop_fetch_message (msg, ctx, msgno) != 0)
	FREE (&msg);
      break;
    }
#endif /* USE_POP */

    default:
      dprint (1, (debugfile, "mx_open_message(): function not implemented for mailbox type %d.\n", ctx->magic));
      FREE (&msg);
      break;
  }
  return (msg);
}

/* commit a message to a folder */

int mx_commit_message (MESSAGE *msg, CONTEXT *ctx)
{
  int r = 0;

  if (!(msg->write && ctx->append))
  {
    dprint (1, (debugfile, "mx_commit_message(): msg->write = %d, ctx->append = %d\n",
		msg->write, ctx->append));
    return -1;
  }

  switch (msg->magic)
  {
    case M_MMDF:
    {
      if (fputs (MMDF_SEP, msg->fp) == EOF)
	r = -1;
      break;
    }
    
    case M_MBOX:
    {
      if (fputc ('\n', msg->fp) == EOF)
	r = -1;
      break;
    }

#ifdef USE_IMAP
    case M_IMAP:
    {
      if ((r = safe_fclose (&msg->fp)) == 0)
	r = imap_append_message (ctx, msg);
      break;
    }
#endif
    
    case M_MAILDIR:
    {
      r = maildir_commit_message (ctx, msg, NULL);
      break;
    }
    
    case M_MH:
    {
      r = mh_commit_message (ctx, msg, NULL);
      break;
    }
  }
  
  if (r == 0 && (ctx->magic == M_MBOX || ctx->magic == M_MMDF)
      && (fflush (msg->fp) == EOF || fsync (fileno (msg->fp)) == -1))
  {
    mutt_perror _("Can't write message");
    r = -1;
  }
 
  return r;
}

/* close a pointer to a message */
int mx_close_message (MESSAGE **msg)
{
  int r = 0;

  if ((*msg)->magic == M_MH || (*msg)->magic == M_MAILDIR
#ifdef USE_IMAP
      || (*msg)->magic == M_IMAP
#endif
#ifdef USE_POP
      || (*msg)->magic == M_POP
#endif
      )
  {
    r = safe_fclose (&(*msg)->fp);
  }
  else
    (*msg)->fp = NULL;

  if ((*msg)->path)
  {
    dprint (1, (debugfile, "mx_close_message (): unlinking %s\n",
		(*msg)->path));
    unlink ((*msg)->path);
    FREE (&(*msg)->path);
  }

  FREE (msg);		/* __FREE_CHECKED__ */
  return (r);
}

void mx_alloc_memory (CONTEXT *ctx)
{
  int i;
  size_t s = MAX (sizeof (HEADER *), sizeof (int));
  
  if ((ctx->hdrmax + 25) * s < ctx->hdrmax * s)
  {
    mutt_error _("Integer overflow -- can't allocate memory.");
    sleep (1);
    mutt_exit (1);
  }
  
  if (ctx->hdrs)
  {
    safe_realloc (&ctx->hdrs, sizeof (HEADER *) * (ctx->hdrmax += 25));
    safe_realloc (&ctx->v2r, sizeof (int) * ctx->hdrmax);
  }
  else
  {
    ctx->hdrs = safe_calloc ((ctx->hdrmax += 25), sizeof (HEADER *));
    ctx->v2r = safe_calloc (ctx->hdrmax, sizeof (int));
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
void mx_update_context (CONTEXT *ctx, int new_messages)
{
  HEADER *h;
  int msgno;

  for (msgno = ctx->msgcount - new_messages; msgno < ctx->msgcount; msgno++)
  {
    h = ctx->hdrs[msgno];

    if (WithCrypto)
    {
      /* NOTE: this _must_ be done before the check for mailcap! */
      h->security = crypt_query (h->content);
    }

    if (!ctx->pattern)
    {
      ctx->v2r[ctx->vcount] = msgno;
      h->virtual = ctx->vcount++;
    }
    else
      h->virtual = -1;
    h->msgno = msgno;

    if (h->env->supersedes)
    {
      HEADER *h2;

      if (!ctx->id_hash)	
	ctx->id_hash = mutt_make_id_hash (ctx);

      h2 = hash_find (ctx->id_hash, h->env->supersedes);

      /* FREE (&h->env->supersedes); should I ? */
      if (h2)
      {
	h2->superseded = 1;
	if (option (OPTSCORE)) 
	  mutt_score_message (ctx, h2, 1);
      }
    }

    /* add this message to the hash tables */
    if (ctx->id_hash && h->env->message_id)
      hash_insert (ctx->id_hash, h->env->message_id, h, 0);
    if (ctx->subj_hash && h->env->real_subj)
      hash_insert (ctx->subj_hash, h->env->real_subj, h, 1);

    if (option (OPTSCORE)) 
      mutt_score_message (ctx, h, 0);

    if (h->changed)
      ctx->changed = 1;
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
  }
}

/*
 * Return:
 * 1 if the specified mailbox contains 0 messages.
 * 0 if the mailbox contains messages
 * -1 on error
 */
int mx_check_empty (const char *path)
{
  switch (mx_get_magic (path))
  {
    case M_MBOX:
    case M_MMDF:
      return mbox_check_empty (path);
    case M_MH:
      return mh_check_empty (path);
    case M_MAILDIR:
      return maildir_check_empty (path);
    default:
      errno = EINVAL;
      return -1;
  }
  /* not reached */
}

/* vim: set sw=2: */
