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

/*
 * This file contains routines specific to MH and ``maildir'' style mailboxes
 */

#include "mutt.h"
#include "mx.h"
#include "mailbox.h"
#include "copy.h"
#include "buffy.h"
#include "sort.h"

#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>

struct maildir
{
  HEADER *h;
  char *canon_fname;
  struct maildir *next;
};

static void maildir_free_entry(struct maildir **md)
{
  if(!md || !*md)
    return;

  safe_free((void **) &(*md)->canon_fname);
  if((*md)->h)
    mutt_free_header(&(*md)->h);

  safe_free((void **) md);
}
  
static void maildir_free_maildir(struct maildir **md)
{
  struct maildir *p, *q;
  
  if(!md || !*md)
    return;
  
  for(p = *md; p; p = q)
  {
    q = p->next;
    maildir_free_entry(&p);
  }
}

static void maildir_parse_flags(HEADER *h, const char *path)
{
  char *p;

  h->flagged = 0;
  h->read = 0;
  h->replied = 0;
  
  if ((p = strrchr (path, ':')) != NULL && strncmp (p + 1, "2,", 2) == 0)
  {
    p += 3;
    while (*p)
    {
      switch (*p)
      {
	case 'F':
	
	h->flagged = 1;
	break;
	
	case 'S': /* seen */
	
	h->read = 1;
	break;
	
	case 'R': /* replied */
	
	h->replied = 1;
	break;
      }
      p++;
    }
  }
}

static void maildir_update_mtime(CONTEXT *ctx)
{
  char buf[_POSIX_PATH_MAX];
  struct stat st;
  
  if(ctx->magic == M_MAILDIR)
  {
    snprintf(buf, sizeof(buf), "%s/%s", ctx->path, "cur");
    if(stat (buf, &st) == 0)
      ctx->mtime_cur = st.st_mtime;
    snprintf(buf, sizeof(buf), "%s/%s", ctx->path, "new");
  }
  else
    strfcpy(buf, ctx->path, sizeof(buf));
  
  if(stat(buf, &st) == 0)
    ctx->mtime = st.st_mtime;
}

static HEADER *maildir_parse_message(int magic, const char *fname, int is_old)
{
  FILE *f;
  HEADER *h = NULL;
  struct stat st;
  
  if ((f = fopen (fname, "r")) != NULL)
  {
    h = mutt_new_header();
    h->env = mutt_read_rfc822_header (f, h);

    fstat (fileno (f), &st);
    fclose (f);

    if (!h->received)
      h->received = h->date_sent;

    if (h->content->length <= 0)
      h->content->length = st.st_size - h->content->offset;

    h->index = -1;

    if (magic == M_MAILDIR)
    {
      /* maildir stores its flags in the filename, so ignore the flags in
       * the header of the message
       */

      h->old = is_old;
      maildir_parse_flags(h, fname);
    }
  }
  return h;
}

/* note that this routine will _not_ modify the context given by ctx. */

static int maildir_parse_entry(CONTEXT *ctx, struct maildir ***last,
				const char *subdir, const char *fname,
				int *count, int is_old)
{
  struct maildir *entry;
  HEADER *h;
  char buf[_POSIX_PATH_MAX];

  if(subdir)
    snprintf(buf, sizeof(buf), "%s/%s/%s", ctx->path, subdir, fname);
  else
    snprintf(buf, sizeof(buf), "%s/%s", ctx->path, fname);
  
  if((h = maildir_parse_message(ctx->magic, buf, is_old)) != NULL)
  {
    if(count)
    {
      (*count)++;  
      if (!ctx->quiet && ReadInc && ((*count % ReadInc) == 0 || *count == 1))
	mutt_message ("Reading %s... %d", ctx->path, *count);
    }

    if (subdir)
    {
      snprintf (buf, sizeof (buf), "%s/%s", subdir, fname);
      h->path = safe_strdup (buf);
    }
    else
      h->path = safe_strdup (fname);
    
    entry = safe_calloc(sizeof(struct maildir), 1);
    entry->h = h;
    **last = entry;
    *last = &entry->next;
    
    return 0;
  }
  
  return -1;
}

/* Ignore the garbage files.  A valid MH message consists of only
 * digits.  Deleted message get moved to a filename with a comma before
 * it.
 */

int mh_valid_message (const char *s)
{
  for (; *s; s++)
  {
    if (!isdigit ((unsigned char) *s))
      return 0;
  }
  return 1;
}

static int maildir_parse_dir(CONTEXT *ctx, struct maildir ***last,
			     const char *subdir, int *count)
{
  DIR *dirp;
  struct dirent *de;
  char buf[_POSIX_PATH_MAX];
  int is_old = 0;
  
  if(subdir)
  {
    snprintf(buf, sizeof(buf), "%s/%s", ctx->path, subdir);
    is_old = (strcmp("cur", subdir) == 0) && option(OPTMARKOLD);
  }
  else
    strfcpy(buf, ctx->path, sizeof(buf));
  
  if((dirp = opendir(buf)) == NULL)
    return -1;

  while ((de = readdir (dirp)) != NULL)
  {
    if ((ctx->magic == M_MH && !mh_valid_message(de->d_name)) || (ctx->magic == M_MAILDIR && *de->d_name == '.'))
      continue;
    
    /* FOO - really ignore the return value? */
    
    maildir_parse_entry(ctx, last, subdir, de->d_name, count, is_old);
  }

  closedir(dirp);
  return 0;
}

static void maildir_add_to_context(CONTEXT *ctx, struct maildir *md)
{
  while(md)
  {
    if(md->h)
    {
      if(ctx->msgcount == ctx->hdrmax)
	mx_alloc_memory(ctx);
      
      ctx->hdrs[ctx->msgcount] = md->h;
      ctx->hdrs[ctx->msgcount]->index = ctx->msgcount;
      ctx->size +=
	md->h->content->length + md->h->content->offset - md->h->content->hdr_offset;
      
      md->h = NULL;
      mx_update_context(ctx);
    }
    md = md->next;
  }
}

static void maildir_move_to_context(CONTEXT *ctx, struct maildir **md)
{
  maildir_add_to_context(ctx, *md);
  maildir_free_maildir(md);
}

/* Read a MH/maildir style mailbox.
 *
 * args:
 *	ctx [IN/OUT]	context for this mailbox
 *	subdir [IN]	NULL for MH mailboxes, otherwise the subdir of the
 *			maildir mailbox to read from
 */
int mh_read_dir (CONTEXT *ctx, const char *subdir)
{
  struct maildir *md;
  struct maildir **last;
  int count;
  
  md = NULL;
  last = &md;
  count = 0;

  maildir_update_mtime(ctx);

  if(maildir_parse_dir(ctx, &last, subdir, &count) == -1)
    return -1;
  
  maildir_move_to_context(ctx, &md);
  return 0;
}
  
/* read a maildir style mailbox */
int maildir_read_dir (CONTEXT * ctx)
{
  /* maildir looks sort of like MH, except that there are two subdirectories
   * of the main folder path from which to read messages
   */
  if (mh_read_dir (ctx, "new") == -1 || mh_read_dir (ctx, "cur") == -1)
    return (-1);

  return 0;
}

/* Open a new (unique) message in a maildir mailbox.  In order to avoid the
 * need for locks, the filename is generated in such a way that it is unique,
 * even over NFS: <time>.<pid>_<count>.<hostname>.  The _<count> part is
 * optional, but required for programs like Mutt which do not change PID for
 * each message that is created in the mailbox (otherwise you could end up
 * creating only a single file per second).
 */
void maildir_create_filename (const char *path, HEADER *hdr, char *msg, char *full)
{
  char subdir[_POSIX_PATH_MAX];
  char suffix[16];
  struct stat sb;

  /* the maildir format stores the status flags in the filename */
  suffix[0] = 0;

  if (hdr && (hdr->flagged || hdr->replied || hdr->read))
  {
    sprintf (suffix, ":2,%s%s%s",
	     hdr->flagged ? "F" : "",
	     hdr->replied ? "R" : "",
	     hdr->read ? "S" : "");
  }

  if (hdr && (hdr->read || hdr->old))
    strfcpy (subdir, "cur", sizeof (subdir));
  else
    strfcpy (subdir, "new", sizeof (subdir));

  FOREVER
  {
    snprintf (msg, _POSIX_PATH_MAX, "%s/%ld.%d_%d.%s%s",
	      subdir, time (NULL), getpid (), Counter++, NONULL(Hostname), suffix);
    snprintf (full, _POSIX_PATH_MAX, "%s/%s", path, msg);
    if (stat (full, &sb) == -1 && errno == ENOENT) return;
  }
}

/* save changes to a message to disk */
static int mh_sync_message (CONTEXT *ctx, int msgno)
{
  HEADER *h = ctx->hdrs[msgno];
  FILE *f;
  FILE *d;
  int rc = -1;
  char oldpath[_POSIX_PATH_MAX];
  char newpath[_POSIX_PATH_MAX];
  
  snprintf (oldpath, sizeof (oldpath), "%s/%s", ctx->path, h->path);
  
  mutt_mktemp (newpath);
  if ((f = safe_fopen (newpath, "w")) == NULL)
  {
    dprint (1, (debugfile, "mh_sync_message: %s: %s (errno %d).\n",
		newpath, strerror (errno), errno));
    return (-1);
  }

  rc = mutt_copy_message (f, ctx, h, M_CM_UPDATE, CH_UPDATE | CH_UPDATE_LEN);

  if (rc == 0)
  {
    /* replace the original version of the message with the new one */
    if ((f = freopen (newpath, "r", f)) != NULL)
    {
      unlink (newpath);
      if ((d = mx_open_file_lock (oldpath, "w")) != NULL)
      {
	mutt_copy_stream (f, d);
	mx_unlock_file (oldpath, fileno (d));
	fclose (d);
      }
      else
      {
	fclose (f);
	return (-1);
      }
      fclose (f);
    }
    else
    {
      mutt_perror (newpath);
      return (-1);
    }

    /*
     * if the status of this message changed, the offsets for the body parts
     * will be wrong, so free up the memory.  This is ok since it will get
     * parsed again the next time the user tries to view it.
     */
    mutt_free_body (&h->content->parts);
  }
  else
  {
    fclose (f);
    unlink (newpath);
  }

  return (rc);
}

static int maildir_sync_message (CONTEXT *ctx, int msgno)
{
  HEADER *h = ctx->hdrs[msgno];
  char newpath[_POSIX_PATH_MAX];
  char partpath[_POSIX_PATH_MAX];
  char fullpath[_POSIX_PATH_MAX];
  char oldpath[_POSIX_PATH_MAX];
  char *p;

  if ((p = strchr (h->path, '/')) == NULL)
  {
    dprint (1, (debugfile, "maildir_sync_message: %s: unable to find subdir!\n",
		h->path));
    return (-1);
  }
  p++;
  strfcpy (newpath, p, sizeof (newpath));

  /* kill the previous flags */
  if ((p = strchr (newpath, ':')) != NULL) *p = 0;

  if (h->replied || h->read || h->flagged)
  {
    strcat (newpath, ":2,");
    if (h->flagged) strcat (newpath, "F");
    if (h->replied) strcat (newpath, "R");
    if (h->read) strcat (newpath, "S");
  }

  /* decide which subdir this message belongs in */
  strfcpy (partpath, (h->read || h->old) ? "cur" : "new", sizeof (partpath));
  strcat (partpath, "/"); 
  strcat (partpath, newpath); 
  snprintf (fullpath, sizeof (fullpath), "%s/%s", ctx->path, partpath);
  snprintf (oldpath, sizeof (oldpath), "%s/%s", ctx->path, h->path);

  if (strcmp (fullpath, oldpath) == 0 && !h->attach_del)
  {
    /* message hasn't really changed */
    return 0;
  }

  /*
   * At this point, this should work to actually delete the attachment
   * without breaking anything else (mh_sync_message doesn't appear to
   * make any mailbox assumptions except the one file per message, which
   * is compatible with maildir
   */
  if (h->attach_del)
  {
    char tmppath[_POSIX_PATH_MAX];
    char ftpath[_POSIX_PATH_MAX];

    strfcpy (tmppath, "tmp/", sizeof (tmppath));
    strcat (tmppath, newpath);

    snprintf (ftpath, sizeof (ftpath), "%s/%s", ctx->path, tmppath);
    dprint (1, (debugfile, "maildir_sync_message: deleting attachment!\n"));

    if (rename (oldpath, ftpath) != 0)
    {
      mutt_perror ("rename");
      return (-1);
    }
    safe_free ((void **)&h->path);
    h->path = safe_strdup (tmppath);
    dprint (1, (debugfile, "maildir_sync_message: deleting attachment2!\n"));
    mh_sync_message (ctx, msgno);
    dprint (1, (debugfile, "maildir_sync_message: deleting attachment3!\n"));
    if (rename (ftpath, fullpath) != 0)
    {
      mutt_perror ("rename");
      return (-1);
    }
  }
  else
  {
    if (rename (oldpath, fullpath) != 0)
    {
      mutt_perror ("rename");
      return (-1);
    }
  }
  safe_free ((void **) &h->path);
  h->path = safe_strdup (partpath);
  return (0);
}

int mh_sync_mailbox (CONTEXT * ctx)
{
  char path[_POSIX_PATH_MAX], tmp[_POSIX_PATH_MAX];
  int i, rc = 0;
  
  i = mh_check_mailbox(ctx, NULL);
  if(i == M_REOPENED)
  {
    set_option(OPTSORTCOLLAPSE);
    mutt_sort_headers(ctx, 1);
    unset_option(OPTSORTCOLLAPSE);
  }
  
  if(i == M_REOPENED || i == M_NEW_MAIL || i < 0)
    return i;

  for (i = 0; i < ctx->msgcount; i++)
  {
    if (ctx->hdrs[i]->deleted)
    {
      snprintf (path, sizeof (path), "%s/%s", ctx->path, ctx->hdrs[i]->path);
      if (ctx->magic == M_MAILDIR || (option (OPTMHPURGE) && ctx->magic == M_MH))
	unlink (path);
      else if (ctx->magic == M_MH)
      {
	/* MH just moves files out of the way when you delete them */
	if(*ctx->hdrs[i]->path != ',')
	{
	  snprintf (tmp, sizeof (tmp), "%s/,%s", ctx->path, ctx->hdrs[i]->path);
	  unlink (tmp);
	  rename (path, tmp);
	}
	  
      }
    }
    else if (ctx->hdrs[i]->changed || ctx->hdrs[i]->attach_del)
    {
      if (ctx->magic == M_MAILDIR)
	maildir_sync_message (ctx, i);
      else
      {
	/* FOO - seems ok to ignore errors, but might want to warn... */
	mh_sync_message (ctx, i);
      }
    }
  }
  maildir_update_mtime(ctx);
  return (rc);
}

static char *maildir_canon_filename(char *dest, char *src, size_t l)
{
  char *t, *u;
  
  if((t = strchr(src, '/')))
    src = t + 1;

  strfcpy(dest, src, l);
  if((u = strchr(dest, ';')))
    *u = '\0';
  
  return dest;
}


/* This function handles arrival of new mail and reopening of mh/maildir folders.
 * Things are getting rather complex because we don't have a
 * well-defined "mailbox order", so the tricks from mbox.c and mx.c
 * won't work here.
 */

int mh_check_mailbox(CONTEXT *ctx, int *index_hint)
{
  char buf[_POSIX_PATH_MAX], b1[LONG_STRING], b2[LONG_STRING];
  struct stat st, st_cur;
  short modified = 0, have_new = 0;
  struct maildir *md, *p;
  struct maildir **last;
  HASH *fnames;
  int i, idx;
  
  if(!option (OPTCHECKNEW))
    return 0;
  
  if(ctx->magic == M_MH)
  {
    strfcpy(buf, ctx->path, sizeof(buf));
    if(stat(buf, &st) == -1)
      return -1;
  }
  else if(ctx->magic == M_MAILDIR)
  {
    snprintf(buf, sizeof(buf), "%s/new", ctx->path);
    if(stat(buf, &st) == -1)
      return -1;
    
    snprintf(buf, sizeof(buf), "%s/cur", ctx->path);
    if(stat(buf, &st_cur) == -1)
      modified = 1;

  }
  
  if(!modified && ctx->magic == M_MAILDIR && st_cur.st_mtime > ctx->mtime_cur)
    modified = 1;
  
  if(!modified && ctx->magic == M_MH && st.st_mtime > ctx->mtime)
    modified = 1;
  
  if(modified || (ctx->magic == M_MAILDIR && st.st_mtime > ctx->mtime))
    have_new = 1;
  
  if(!modified && !have_new)
    return 0;

  if(ctx->magic == M_MAILDIR)
    ctx->mtime_cur = st_cur.st_mtime;
  ctx->mtime = st.st_mtime;
  
  if(Sort != SORT_ORDER)
  {
    short old_sort;
    
    old_sort = Sort;
    Sort = SORT_ORDER;
    mutt_sort_headers(ctx, 1);
    Sort = old_sort;
  }
    
  md = NULL;
  last = &md;
  
  if(ctx->magic == M_MAILDIR)
  {
    if(have_new)
      maildir_parse_dir(ctx, &last, "new", NULL);
    if(modified)
      maildir_parse_dir(ctx, &last, "cur", NULL);
  }
  else if(ctx->magic == M_MH)
    maildir_parse_dir(ctx, &last, NULL, NULL);

  /* check for modifications and adjust flags */

  fnames = hash_create(1024);
  
  for(p = md; p; p = p->next)
  {
    if(ctx->magic == M_MAILDIR)
    {
      maildir_canon_filename(b2, p->h->path, sizeof(b2));
      p->canon_fname = safe_strdup(b2);
    }
    else
      p->canon_fname = safe_strdup(p->h->path);
    
    hash_insert(fnames, p->canon_fname, p, 0);
  }

  if(index_hint) idx = *index_hint;
  
  for(i = 0; i < ctx->msgcount; i++)
  {
    ctx->hdrs[i]->active = 0;

    if(ctx->magic == M_MAILDIR)
      maildir_canon_filename(b1, ctx->hdrs[i]->path, sizeof(b1));
    else
      strfcpy(b1, ctx->hdrs[i]->path, sizeof(b1));

    if((p = hash_find(fnames, b1)) && p->h && 
       mbox_strict_cmp_headers(ctx->hdrs[i], p->h))
    {
      /* found the right message */
      
      if(modified)
      {
	if(!ctx->hdrs[i]->changed)
	{
	  ctx->hdrs[i]->flagged = p->h->flagged;
	  ctx->hdrs[i]->replied = p->h->replied;
	  ctx->hdrs[i]->old     = p->h->old;
	  ctx->hdrs[i]->read    = p->h->read;
	}
      }
      
      ctx->hdrs[i]->active = 1;
      mutt_free_header(&p->h);
    }
    else
    {
      /* the message has disappeared by occult forces, correct
       * the index hint. 
       */
      
      if(index_hint && (i <= *index_hint))
	idx--;
    }
  }
    
  hash_destroy(&fnames, NULL);
    
  if(modified)
    mx_update_tables(ctx, 0);

  maildir_move_to_context(ctx, &md);
  
  if(index_hint)
    *index_hint = idx;
  
  return modified ? M_REOPENED : have_new ? M_NEW_MAIL : 0;
}

