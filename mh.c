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

#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>

void mh_parse_message (CONTEXT *ctx,
		       const char *subdir,
		       const char *fname,
		       int *count,
		       int isOld)
{
  char path[_POSIX_PATH_MAX];
  char *p;
  FILE *f;
  HEADER *h;
  struct stat st;

  if (subdir)
    snprintf (path, sizeof (path), "%s/%s/%s", ctx->path, subdir, fname);
  else
    snprintf (path, sizeof (path), "%s/%s", ctx->path, fname);

  if ((f = fopen (path, "r")) != NULL)
  {
    (*count)++;

    if (!ctx->quiet && ReadInc && ((*count % ReadInc) == 0 || *count == 1))
      mutt_message ("Reading %s... %d", ctx->path, *count);

    if (ctx->msgcount == ctx->hdrmax)
      mx_alloc_memory (ctx);

    h = ctx->hdrs[ctx->msgcount] = mutt_new_header ();

    if (subdir)
    {
      snprintf (path, sizeof (path), "%s/%s", subdir, fname);
      h->path = safe_strdup (path);
    }
    else
      h->path = safe_strdup (fname);

    h->env = mutt_read_rfc822_header (f, h);

    fstat (fileno (f), &st);
    fclose (f);

    if (!h->received)
      h->received = h->date_sent;

    if (h->content->length <= 0)
      h->content->length = st.st_size - h->content->offset;

    /* index doesn't have a whole lot of meaning for MH and maildir mailboxes,
     * but this is used to find the current message after a resort in 
     * the `index' event loop.
     */
    h->index = ctx->msgcount;

    if (ctx->magic == M_MAILDIR)
    {
      /* maildir stores its flags in the filename, so ignore the flags in
       * the header of the message
       */

      h->old = isOld;

      if ((p = strchr (h->path, ':')) != NULL && strncmp (p + 1, "2,", 2) == 0)
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

    /* set flags and update context info */
    mx_update_context (ctx);
  }
}

/*
 * Mark all the mails in ctx read.
 */
static void tag_all_read (CONTEXT * ctx)
{
  int i;

  ctx->new = 0;
  if (ctx->hdrs == NULL)
    return;
  for (i = 0; i < ctx->msgcount; i++)
  {
    ctx->hdrs[i]->read = 1;
    ctx->hdrs[i]->old = 1;
  }
}

/*
 * Mark one mail as unread
 */
static int tag_unread (CONTEXT * ctx, char *name)
{
  int i;

  if (ctx->hdrs == NULL)
    return (1);
  for (i = ctx->msgcount - 1; i >= 0; i--)
    if (!strcmp (name, ctx->hdrs[i]->path))
    {
      ctx->hdrs[i]->read = 0;
      ctx->hdrs[i]->old = 0;
      return (1);
    }
  return (0);
}

/* Parse a .mh_sequences file.  Only supports "unseen:" field.
 */

#define SKIP_BLANK(p) 							\
    while ((*(p) == ' ') || (*(p) == '\t')) p++;

#define SKIP_EOL(p)                                                     \
{							                \
    while ((*p != '\0') && (*(p) != '\n') && (*(p) != '\r')) p++;	\
    while ((*(p) == '\n') || (*(p) == '\r')) p++;			\
}

/* Parses and returns the next unsigned number in the string <cur>, 
   advancing the pointer to the character after the number ended.
 */
static unsigned parse_number (const char **cur)
{
  unsigned i = 0;
  for (; (**cur >= '0') && (**cur <= '9'); (*cur)++)
  {
    i *= 10;
    i += **cur - '0';
  }
  return i;
}

/* if context is NULL, it uses <folder> as the mailbox path, otherwise,
   uses ctx->path.  If context is not null, tags all the unread messages 
   and sets the position correctly.
   returns the number of unread messages in the mailbox.
 */
int mh_parse_sequences (CONTEXT * ctx, const char *folder)
{
  FILE *mh_sequences;
  struct stat sb;
  char *content;
  const char *cur;
  int len;
  int base, last;
  char buf[_POSIX_PATH_MAX];
  char filename[100];
  int unread = 0;
  static HASH *cache = 0;
  int *h;

  if (!cache) {
    cache = hash_create (100);	/* If this ain't enough, read less mail! */
    if (!cache)
    {
      mutt_error ("mh_parse_sequences: Unable to allocate hash table!\n");
      return -1;
    }
  }

  if (ctx)
    folder = ctx->path;

  snprintf (buf, sizeof (buf), "%s/.mh_sequences", folder);
  if (stat (buf, &sb) != 0)
    return (-1);

  if (ctx)
    tag_all_read (ctx);

  if (!ctx && sb.st_mtime < BuffyDoneTime && (h = hash_find (cache, folder))) /* woo!  we win! */
    return *h;

  /*
   * Parse the .mh_sequence to find the unread ones.
   */

  content = safe_malloc (sb.st_size + 100);
  if (content == NULL)
  {
    mutt_perror ("malloc");
    return (-1);
  }
  mh_sequences = fopen (buf, "r");
  if (mh_sequences == NULL)
  {
    mutt_message ("Cannot open %s", buf);
    safe_free ((void **) &content);
    return (-1);
  }

  len = fread (content, 1, sb.st_size, mh_sequences);
  content[len] = '\0';

  fclose (mh_sequences);

  cur = content;
  SKIP_BLANK (cur);
  while (*cur != '\0')
  {
    if (!strncmp (cur, "unseen", 6))
    {
      cur += 6;
      SKIP_BLANK (cur);
      if (*cur == ':')
      {
	cur++;
	SKIP_BLANK (cur);
      }
      while ((*cur >= '0') && (*cur <= '9'))
      {
	base = parse_number (&cur);
	SKIP_BLANK (cur);
	if (*cur == '-')
	{
	  cur++;
	  SKIP_BLANK (cur);
	  last = parse_number (&cur);
	}
	else
	  last = base;
	if (ctx)
	  for (; base <= last; base++)
	  {
	    sprintf (filename, "%d", base);
	    if (tag_unread (ctx, filename))
	      unread++;
	  }
	else
	  unread += last - base + 1;
	SKIP_BLANK (cur);
      }
    }
    SKIP_EOL (cur);
    SKIP_BLANK (cur);
  }
  if (unread != 0)
  {
    mutt_message ("Folder %s : %d unread", folder, unread);
  }
  if (ctx)
    ctx->new = unread;

  safe_free ((void **) &content);

  /* Cache the data so we only have to do 'stat's in the future */
  if (!(h = hash_find (cache, folder)))
  {
    h = safe_malloc (sizeof (int));
    if (!h)			        /* dear god, I hope not... */
      return -1;
    hash_insert (cache, folder, h, 1);  /* every other time we just adjust the pointer */
  }
  *h = unread;

  return unread;
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

/* Read a MH/maildir style mailbox.
 *
 * args:
 *	ctx [IN/OUT]	context for this mailbox
 *	subdir [IN]	NULL for MH mailboxes, otherwise the subdir of the
 *			maildir mailbox to read from
 */
int mh_read_dir (CONTEXT *ctx, const char *subdir)
{
  DIR *dirp;
  struct dirent *de;
  char buf[_POSIX_PATH_MAX];
  int isOld = 0;
  int count = 0;
  struct stat st;
  int has_mh_sequences = 0;

  if (subdir)
  {
    snprintf (buf, sizeof (buf), "%s/%s", ctx->path, subdir);
    isOld = (strcmp ("cur", subdir) == 0) && option (OPTMARKOLD);
  }
  else
    strfcpy (buf, ctx->path, sizeof (buf));

  if (stat (buf, &st) == -1)
    return (-1);

  if ((dirp = opendir (buf)) == NULL)
    return (-1);

  if (!subdir || (subdir && strcmp (subdir, "new") == 0))
    ctx->mtime = st.st_mtime;

  while ((de = readdir (dirp)) != NULL)
  {
    if (ctx->magic == M_MH)
    {
      if (!mh_valid_message (de->d_name))
      {
	if (!strcmp (de->d_name, ".mh_sequences"))
	  has_mh_sequences++;
	continue;
      }
    }
    else if (*de->d_name == '.')
    {
      /* Skip files that begin with a dot.  This currently isn't documented
       * anywhere, but it was a suggestion from the author of QMail on the
       * mailing list.
       */
      continue;
    }

    mh_parse_message (ctx, subdir, de->d_name, &count, isOld);
  }
  {
    int i;
#define this_body ctx->hdrs[i]->content
    ctx->size = 0;
    for (i = 0; i < ctx->msgcount; i++)
      ctx->size += this_body->length + this_body->offset -
	this_body->hdr_offset;
#undef this_body
  }

  /*
   * MH style .
   */
  if (has_mh_sequences)
  {
    mh_parse_sequences (ctx, NULL);
  }

  closedir (dirp);
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
  FILE *mh_sequences;


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

  if(ctx->magic == M_MH)
  {
    snprintf (path, sizeof (path), "%s/%s", ctx->path, ".mh_sequences");
    mh_sequences = fopen (path, "w");
    if (mh_sequences == NULL)
    {
      mutt_message ("fopen %s failed", path);
    }
    else
    {
      fprintf (mh_sequences, "unseen: ");
      for (i = 0; i < ctx->msgcount; i++)
	if ((ctx->hdrs[i]->read == 0) && !(ctx->hdrs[i]->deleted))
	  fprintf (mh_sequences, "%s ", ctx->hdrs[i]->path);
      fprintf (mh_sequences, "\n");
      fclose (mh_sequences);
    }
  }

  return (rc);
}

/* check for new mail */
int mh_check_mailbox (CONTEXT * ctx, int *index_hint)
{
  DIR *dirp;
  struct dirent *de;
  char buf[_POSIX_PATH_MAX];
  struct stat st;
  LIST *lst = NULL, *tmp = NULL, *prev;
  int i, lng = 0;
  int count = 0;

  /* MH users might not like the behavior of this function because it could
   * take awhile if there are many messages in the mailbox.
   */
  if (!option (OPTCHECKNEW))
    return (0); /* disabled */

  if (ctx->magic == M_MH)
    strfcpy (buf, ctx->path, sizeof (buf));
  else
    snprintf (buf, sizeof (buf), "%s/new", ctx->path);

  if (stat (buf, &st) == -1)
    return (-1);

  if (st.st_mtime == ctx->mtime)
    return (0); /* unchanged */

  if ((dirp = opendir (buf)) == NULL)
    return (-1);

  while ((de = readdir (dirp)) != NULL)
  {
    if (ctx->magic == M_MH)
    {
      if (!mh_valid_message (de->d_name))
	continue;
    }
    else /* maildir */
    {
      if (*de->d_name == '.')
	continue;
    }

    if (tmp)
    {
      tmp->next = mutt_new_list ();
      tmp = tmp->next;
    }
    else
      lst = tmp = mutt_new_list ();
    tmp->data = safe_strdup (de->d_name);
  }
  closedir (dirp);

  ctx->mtime = st.st_mtime; /* save the time we scanned at */

  if (!lst)
    return 0;

  /* if maildir, skip the leading "new/" */
  lng = (ctx->magic == M_MAILDIR) ? 4 : 0;

  for (i = 0; i < ctx->msgcount; i++)
  {
    if (ctx->magic == M_MAILDIR && ctx->hdrs[i]->old)
      continue; /* only look at NEW messages */

    prev = NULL;
    tmp = lst;
    while (tmp)
    {
      if (strcmp (tmp->data, ctx->hdrs[i]->path + lng) == 0)
      {
	if (prev)
	  prev->next = tmp->next;
	else
	  lst = lst->next;
	safe_free ((void **) &tmp->data);
	safe_free ((void **) &tmp);
	break;
      }
      else
      {
	prev = tmp;
	tmp = tmp->next;
      }
    }
  }

  for (tmp = lst; tmp; tmp = lst)
  {
    mh_parse_message (ctx, (ctx->magic == M_MH ? NULL : "new"), tmp->data, &count, 0);

    lst = tmp->next;
    safe_free ((void **) &tmp->data);
    safe_free ((void **) &tmp);
  }

  return (1);
}
