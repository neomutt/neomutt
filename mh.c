/*
 * Copyright (C) 1996-2002,2007,2009 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2005 Thomas Roessler <roessler@does-not-exist.org>
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

/*
 * This file contains routines specific to MH and ``maildir'' style
 * mailboxes.
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mailbox.h"
#include "mx.h"
#include "copy.h"
#include "sort.h"
#if USE_HCACHE
#include "hcache.h"
#endif
#include "mutt_curses.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <utime.h>

#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#define		INS_SORT_THRESHOLD		6

struct maildir
{
  HEADER *h;
  char *canon_fname;
  unsigned header_parsed:1;
#ifdef HAVE_DIRENT_D_INO
  ino_t inode;
#endif /* HAVE_DIRENT_D_INO */
  struct maildir *next;
};

struct mh_sequences
{
  int max;
  short *flags;
};

struct mh_data
{
  time_t mtime_cur;
  mode_t mh_umask;
};

/* mh_sequences support */

#define MH_SEQ_UNSEEN  (1 << 0)
#define MH_SEQ_REPLIED (1 << 1)
#define MH_SEQ_FLAGGED (1 << 2)

static inline struct mh_data *mh_data (CONTEXT *ctx)
{
  return (struct mh_data*)ctx->data;
}

static void mhs_alloc (struct mh_sequences *mhs, int i)
{
  int j;
  int newmax;

  if (i > mhs->max || !mhs->flags)
  {
    newmax = i + 128;
    j = mhs->flags ? mhs->max + 1 : 0;
    safe_realloc (&mhs->flags, sizeof (mhs->flags[0]) * (newmax + 1));
    while (j <= newmax)
      mhs->flags[j++] = 0;

    mhs->max = newmax;
  }
}

static void mhs_free_sequences (struct mh_sequences *mhs)
{
  FREE (&mhs->flags);
}

static short mhs_check (struct mh_sequences *mhs, int i)
{
  if (!mhs->flags || i > mhs->max)
    return 0;
  else
    return mhs->flags[i];
}

static short mhs_set (struct mh_sequences *mhs, int i, short f)
{
  mhs_alloc (mhs, i);
  mhs->flags[i] |= f;
  return mhs->flags[i];
}

#if 0

/* unused */

static short mhs_unset (struct mh_sequences *mhs, int i, short f)
{
  mhs_alloc (mhs, i);
  mhs->flags[i] &= ~f;
  return mhs->flags[i];
}

#endif

static int mh_read_token (char *t, int *first, int *last)
{
  char *p;
  if ((p = strchr (t, '-')))
  {
    *p++ = '\0';
    if (mutt_atoi (t, first) < 0 || mutt_atoi (t, last) < 0)
      return -1;
  }
  else
  {
    if (mutt_atoi (t, first) < 0)
      return -1;
    *last = *first;
  }
  return 0;
}

static int mh_read_sequences (struct mh_sequences *mhs, const char *path)
{
  FILE *fp;
  int line = 1;
  char *buff = NULL;
  char *t;
  size_t sz = 0;

  short f;
  int first, last, rc;

  char pathname[_POSIX_PATH_MAX];
  snprintf (pathname, sizeof (pathname), "%s/.mh_sequences", path);

  if (!(fp = fopen (pathname, "r")))
    return 0; /* yes, ask callers to silently ignore the error */

  while ((buff = mutt_read_line (buff, &sz, fp, &line, 0)))
  {
    if (!(t = strtok (buff, " \t:")))
      continue;

    if (!mutt_strcmp (t, MhUnseen))
      f = MH_SEQ_UNSEEN;
    else if (!mutt_strcmp (t, MhFlagged))
      f = MH_SEQ_FLAGGED;
    else if (!mutt_strcmp (t, MhReplied))
      f = MH_SEQ_REPLIED;
    else			/* unknown sequence */
      continue;

    while ((t = strtok (NULL, " \t:")))
    {
      if (mh_read_token (t, &first, &last) < 0)
      {
	mhs_free_sequences (mhs);
	rc = -1;
	goto out;
      }
      for (; first <= last; first++)
	mhs_set (mhs, first, f);
    }
  }

  rc = 0;

out:
  FREE (&buff);
  safe_fclose (&fp);
  return 0;
}

static inline mode_t mh_umask (CONTEXT* ctx)
{
  struct stat st;
  struct mh_data* data = mh_data (ctx);

  if (data && data->mh_umask)
    return data->mh_umask;

  if (stat (ctx->path, &st))
  {
    dprint (1, (debugfile, "stat failed on %s\n", ctx->path));
    return 077;
  }

  return 0777 & ~st.st_mode;
}

int mh_buffy (const char *path)
{
  int i, r = 0;
  struct mh_sequences mhs;
  memset (&mhs, 0, sizeof (mhs));

  if (mh_read_sequences (&mhs, path) < 0)
    return 0;
  for (i = 0; !r && i <= mhs.max; i++)
    if (mhs_check (&mhs, i) & MH_SEQ_UNSEEN)
      r = 1;
  return r;
}

static int mh_mkstemp (CONTEXT * dest, FILE ** fp, char **tgt)
{
  int fd;
  char path[_POSIX_PATH_MAX];
  mode_t omask;

  omask = umask (mh_umask (dest));
  FOREVER
  {
    snprintf (path, _POSIX_PATH_MAX, "%s/.mutt-%s-%d-%d",
	      dest->path, NONULL (Hostname), (int) getpid (), Counter++);
    if ((fd = open (path, O_WRONLY | O_EXCL | O_CREAT, 0666)) == -1)
    {
      if (errno != EEXIST)
      {
	mutt_perror (path);
	umask (omask);
	return -1;
      }
    }
    else
    {
      *tgt = safe_strdup (path);
      break;
    }
  }
  umask (omask);

  if ((*fp = fdopen (fd, "w")) == NULL)
  {
    FREE (tgt);		/* __FREE_CHECKED__ */
    close (fd);
    unlink (path);
    return (-1);
  }

  return 0;
}

static void mhs_write_one_sequence (FILE * fp, struct mh_sequences *mhs,
				    short f, const char *tag)
{
  int i;
  int first, last;
  fprintf (fp, "%s:", tag);

  first = -1;
  last = -1;

  for (i = 0; i <= mhs->max; i++)
  {
    if ((mhs_check (mhs, i) & f))
    {
      if (first < 0)
	first = i;
      else
	last = i;
    }
    else if (first >= 0)
    {
      if (last < 0)
	fprintf (fp, " %d", first);
      else
	fprintf (fp, " %d-%d", first, last);

      first = -1;
      last = -1;
    }
  }

  if (first >= 0)
  {
    if (last < 0)
      fprintf (fp, " %d", first);
    else
      fprintf (fp, " %d-%d", first, last);
  }

  fputc ('\n', fp);
}

/* XXX - we don't currently remove deleted messages from sequences we don't know.  Should we? */

static void mh_update_sequences (CONTEXT * ctx)
{
  FILE *ofp, *nfp;

  char sequences[_POSIX_PATH_MAX];
  char *tmpfname;
  char *buff = NULL;
  char *p;
  size_t s;
  int l = 0;
  int i;

  int unseen = 0;
  int flagged = 0;
  int replied = 0;

  char seq_unseen[STRING];
  char seq_replied[STRING];
  char seq_flagged[STRING];


  struct mh_sequences mhs;
  memset (&mhs, 0, sizeof (mhs));

  snprintf (seq_unseen, sizeof (seq_unseen), "%s:", NONULL (MhUnseen));
  snprintf (seq_replied, sizeof (seq_replied), "%s:", NONULL (MhReplied));
  snprintf (seq_flagged, sizeof (seq_flagged), "%s:", NONULL (MhFlagged));

  if (mh_mkstemp (ctx, &nfp, &tmpfname) != 0)
  {
    /* error message? */
    return;
  }

  snprintf (sequences, sizeof (sequences), "%s/.mh_sequences", ctx->path);


  /* first, copy unknown sequences */
  if ((ofp = fopen (sequences, "r")))
  {
    while ((buff = mutt_read_line (buff, &s, ofp, &l, 0)))
    {
      if (!mutt_strncmp (buff, seq_unseen, mutt_strlen (seq_unseen)))
	continue;
      if (!mutt_strncmp (buff, seq_flagged, mutt_strlen (seq_flagged)))
	continue;
      if (!mutt_strncmp (buff, seq_replied, mutt_strlen (seq_replied)))
	continue;

      fprintf (nfp, "%s\n", buff);
    }
  }
  safe_fclose (&ofp);

  /* now, update our unseen, flagged, and replied sequences */
  for (l = 0; l < ctx->msgcount; l++)
  {
    if (ctx->hdrs[l]->deleted)
      continue;

    if ((p = strrchr (ctx->hdrs[l]->path, '/')))
      p++;
    else
      p = ctx->hdrs[l]->path;

    if (mutt_atoi (p, &i) < 0)
      continue;

    if (!ctx->hdrs[l]->read)
    {
      mhs_set (&mhs, i, MH_SEQ_UNSEEN);
      unseen++;
    }
    if (ctx->hdrs[l]->flagged)
    {
      mhs_set (&mhs, i, MH_SEQ_FLAGGED);
      flagged++;
    }
    if (ctx->hdrs[l]->replied)
    {
      mhs_set (&mhs, i, MH_SEQ_REPLIED);
      replied++;
    }
  }

  /* write out the new sequences */
  if (unseen)
    mhs_write_one_sequence (nfp, &mhs, MH_SEQ_UNSEEN, NONULL (MhUnseen));
  if (flagged)
    mhs_write_one_sequence (nfp, &mhs, MH_SEQ_FLAGGED, NONULL (MhFlagged));
  if (replied)
    mhs_write_one_sequence (nfp, &mhs, MH_SEQ_REPLIED, NONULL (MhReplied));

  mhs_free_sequences (&mhs);


  /* try to commit the changes - no guarantee here */
  safe_fclose (&nfp);

  unlink (sequences);
  if (safe_rename (tmpfname, sequences) != 0)
  {
    /* report an error? */
    unlink (tmpfname);
  }

  FREE (&tmpfname);
}

static void mh_sequences_add_one (CONTEXT * ctx, int n, short unseen,
				  short flagged, short replied)
{
  short unseen_done = 0;
  short flagged_done = 0;
  short replied_done = 0;

  FILE *ofp = NULL, *nfp = NULL;

  char *tmpfname;
  char sequences[_POSIX_PATH_MAX];

  char seq_unseen[STRING];
  char seq_replied[STRING];
  char seq_flagged[STRING];

  char *buff = NULL;
  int line;
  size_t sz;

  if (mh_mkstemp (ctx, &nfp, &tmpfname) == -1)
    return;

  snprintf (seq_unseen, sizeof (seq_unseen), "%s:", NONULL (MhUnseen));
  snprintf (seq_replied, sizeof (seq_replied), "%s:", NONULL (MhReplied));
  snprintf (seq_flagged, sizeof (seq_flagged), "%s:", NONULL (MhFlagged));

  snprintf (sequences, sizeof (sequences), "%s/.mh_sequences", ctx->path);
  if ((ofp = fopen (sequences, "r")))
  {
    while ((buff = mutt_read_line (buff, &sz, ofp, &line, 0)))
    {
      if (unseen && !strncmp (buff, seq_unseen, mutt_strlen (seq_unseen)))
      {
	fprintf (nfp, "%s %d\n", buff, n);
	unseen_done = 1;
      }
      else if (flagged
	       && !strncmp (buff, seq_flagged, mutt_strlen (seq_flagged)))
      {
	fprintf (nfp, "%s %d\n", buff, n);
	flagged_done = 1;
      }
      else if (replied
	       && !strncmp (buff, seq_replied, mutt_strlen (seq_replied)))
      {
	fprintf (nfp, "%s %d\n", buff, n);
	replied_done = 1;
      }
      else
	fprintf (nfp, "%s\n", buff);
    }
  }
  safe_fclose (&ofp);
  FREE (&buff);

  if (!unseen_done && unseen)
    fprintf (nfp, "%s: %d\n", NONULL (MhUnseen), n);
  if (!flagged_done && flagged)
    fprintf (nfp, "%s: %d\n", NONULL (MhFlagged), n);
  if (!replied_done && replied)
    fprintf (nfp, "%s: %d\n", NONULL (MhReplied), n);

  safe_fclose (&nfp);

  unlink (sequences);
  if (safe_rename (tmpfname, sequences) != 0)
    unlink (tmpfname);

  FREE (&tmpfname);
}

static void mh_update_maildir (struct maildir *md, struct mh_sequences *mhs)
{
  int i;
  short f;
  char *p;

  for (; md; md = md->next)
  {
    if ((p = strrchr (md->h->path, '/')))
      p++;
    else
      p = md->h->path;

    if (mutt_atoi (p, &i) < 0)
      continue;
    f = mhs_check (mhs, i);

    md->h->read = (f & MH_SEQ_UNSEEN) ? 0 : 1;
    md->h->flagged = (f & MH_SEQ_FLAGGED) ? 1 : 0;
    md->h->replied = (f & MH_SEQ_REPLIED) ? 1 : 0;
  }
}

/* maildir support */

static void maildir_free_entry (struct maildir **md)
{
  if (!md || !*md)
    return;

  FREE (&(*md)->canon_fname);
  if ((*md)->h)
    mutt_free_header (&(*md)->h);

  FREE (md);		/* __FREE_CHECKED__ */
}

static void maildir_free_maildir (struct maildir **md)
{
  struct maildir *p, *q;

  if (!md || !*md)
    return;

  for (p = *md; p; p = q)
  {
    q = p->next;
    maildir_free_entry (&p);
  }
}

static void maildir_parse_flags (HEADER * h, const char *path)
{
  char *p, *q = NULL;

  h->flagged = 0;
  h->read = 0;
  h->replied = 0;

  if ((p = strrchr (path, ':')) != NULL && mutt_strncmp (p + 1, "2,", 2) == 0)
  {
    p += 3;
    
    mutt_str_replace (&h->maildir_flags, p);
    q = h->maildir_flags;

    while (*p)
    {
      switch (*p)
      {
      case 'F':

	h->flagged = 1;
	break;

      case 'S':		/* seen */

	h->read = 1;
	break;

      case 'R':		/* replied */

	h->replied = 1;
	break;

      case 'T':		/* trashed */
	h->trash = 1;
	h->deleted = 1;
	break;
      
      default:
	*q++ = *p;
	break;
      }
      p++;
    }
  }
  
  if (q == h->maildir_flags)
    FREE (&h->maildir_flags);
  else if (q)
    *q = '\0';
}

static void maildir_update_mtime (CONTEXT * ctx)
{
  char buf[_POSIX_PATH_MAX];
  struct stat st;
  struct mh_data *data = mh_data (ctx);

  if (ctx->magic == M_MAILDIR)
  {
    snprintf (buf, sizeof (buf), "%s/%s", ctx->path, "cur");
    if (stat (buf, &st) == 0)
      data->mtime_cur = st.st_mtime;
    snprintf (buf, sizeof (buf), "%s/%s", ctx->path, "new");
  }
  else
  {
    snprintf (buf, sizeof (buf), "%s/.mh_sequences", ctx->path);
    if (stat (buf, &st) == 0)
      data->mtime_cur = st.st_mtime;

    strfcpy (buf, ctx->path, sizeof (buf));
  }

  if (stat (buf, &st) == 0)
    ctx->mtime = st.st_mtime;
}

/* 
 * Actually parse a maildir message.  This may also be used to fill
 * out a fake header structure generated by lazy maildir parsing.
 */
static HEADER *maildir_parse_message (int magic, const char *fname,
				      int is_old, HEADER * _h)
{
  FILE *f;
  HEADER *h = _h;
  struct stat st;

  if ((f = fopen (fname, "r")) != NULL)
  {
    if (!h)
      h = mutt_new_header ();
    h->env = mutt_read_rfc822_header (f, h, 0, 0);

    fstat (fileno (f), &st);
    safe_fclose (&f);

    if (!h->received)
      h->received = h->date_sent;

    if (h->content->length <= 0)
      h->content->length = st.st_size - h->content->offset;

    h->index = -1;

    if (magic == M_MAILDIR)
    {
      /* 
       * maildir stores its flags in the filename, so ignore the
       * flags in the header of the message 
       */

      h->old = is_old;
      maildir_parse_flags (h, fname);
    }
    return h;
  }
  return NULL;
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

static int maildir_parse_dir (CONTEXT * ctx, struct maildir ***last,
			      const char *subdir, int *count,
			      progress_t *progress)
{
  DIR *dirp;
  struct dirent *de;
  char buf[_POSIX_PATH_MAX];
  int is_old = 0;
  struct maildir *entry;
  HEADER *h;

  if (subdir)
  {
    snprintf (buf, sizeof (buf), "%s/%s", ctx->path, subdir);
    is_old = (mutt_strcmp ("cur", subdir) == 0);
  }
  else
    strfcpy (buf, ctx->path, sizeof (buf));

  if ((dirp = opendir (buf)) == NULL)
    return -1;

  while ((de = readdir (dirp)) != NULL)
  {
    if ((ctx->magic == M_MH && !mh_valid_message (de->d_name))
	|| (ctx->magic == M_MAILDIR && *de->d_name == '.'))
      continue;

    /* FOO - really ignore the return value? */
    dprint (2,
	    (debugfile, "%s:%d: queueing %s\n", __FILE__, __LINE__,
	     de->d_name));

    h = mutt_new_header ();
    h->old = is_old;
    if (ctx->magic == M_MAILDIR)
      maildir_parse_flags (h, de->d_name);

    if (count)
    {
      (*count)++;
      if (!ctx->quiet && progress)
	mutt_progress_update (progress, *count, -1);
    }

    if (subdir)
    {
      char tmp[_POSIX_PATH_MAX];
      snprintf (tmp, sizeof (tmp), "%s/%s", subdir, de->d_name);
      h->path = safe_strdup (tmp);
    }
    else
      h->path = safe_strdup (de->d_name);

    entry = safe_calloc (sizeof (struct maildir), 1);
    entry->h = h;
#ifdef HAVE_DIRENT_D_INO
    entry->inode = de->d_ino;
#endif /* HAVE_DIRENT_D_INO */
    **last = entry;
    *last = &entry->next;
  }

  closedir (dirp);

  return 0;
}

static int maildir_add_to_context (CONTEXT * ctx, struct maildir *md)
{
  int oldmsgcount = ctx->msgcount;

  while (md)
  {

    dprint (2, (debugfile, "%s:%d maildir_add_to_context(): Considering %s\n",
		__FILE__, __LINE__, NONULL (md->canon_fname)));

    if (md->h)
    {
      dprint (2,
	      (debugfile,
	       "%s:%d Adding header structure. Flags: %s%s%s%s%s\n", __FILE__,
	       __LINE__, md->h->flagged ? "f" : "", md->h->deleted ? "D" : "",
	       md->h->replied ? "r" : "", md->h->old ? "O" : "",
	       md->h->read ? "R" : ""));
      if (ctx->msgcount == ctx->hdrmax)
	mx_alloc_memory (ctx);

      ctx->hdrs[ctx->msgcount] = md->h;
      ctx->hdrs[ctx->msgcount]->index = ctx->msgcount;
      ctx->size +=
	md->h->content->length + md->h->content->offset -
	md->h->content->hdr_offset;

      md->h = NULL;
      ctx->msgcount++;
    }
    md = md->next;
  }

  if (ctx->msgcount > oldmsgcount)
  {
    mx_update_context (ctx, ctx->msgcount - oldmsgcount);
    return 1;
  }
  return 0;
}

static int maildir_move_to_context (CONTEXT * ctx, struct maildir **md)
{
  int r;
  r = maildir_add_to_context (ctx, *md);
  maildir_free_maildir (md);
  return r;
}

#if USE_HCACHE
static size_t maildir_hcache_keylen (const char *fn)
{
  const char * p = strrchr (fn, ':');
  return p ? (size_t) (p - fn) : mutt_strlen(fn);
}
#endif

#if HAVE_DIRENT_D_INO
static int md_cmp_inode (struct maildir *a, struct maildir *b)
{
  return a->inode - b->inode;
}
#endif

static int md_cmp_path (struct maildir *a, struct maildir *b)
{
  return strcmp (a->h->path, b->h->path);
}

/*
 * Merge two maildir lists according to the inode numbers.
 */
static struct maildir*  maildir_merge_lists (struct maildir *left,
					     struct maildir *right,
					     int (*cmp) (struct maildir *,
							 struct maildir *))
{
  struct maildir* head;
  struct maildir* tail;

  if (left && right) 
  {
    if (cmp (left, right) < 0)
    {
      head = left;
      left = left->next;
    }
    else 
    {
      head = right;
      right = right->next;
    }
  } 
  else 
  {
    if (left) 
      return left;
    else 
      return right;
  }
    
  tail = head;

  while (left && right) 
  {
    if (cmp (left, right) < 0)
    {
      tail->next = left;
      left = left->next;
    } 
    else 
    {
      tail->next = right;
      right = right->next;
    }
    tail = tail->next;
  }

  if (left) 
  {
    tail->next = left;
  }
  else
  {
    tail->next = right;
  }

  return head;
}

static struct maildir* maildir_ins_sort (struct maildir* list,
					 int (*cmp) (struct maildir *,
						     struct maildir *))
{
  struct maildir *tmp, *last, *ret = NULL, *back;

  ret = list;
  list = list->next;
  ret->next = NULL;

  while (list)
  {
    last = NULL;
    back = list->next;
    for (tmp = ret; tmp && cmp (tmp, list) <= 0; tmp = tmp->next)
      last = tmp;

    list->next = tmp;
    if (last)
      last->next = list;
    else
      ret = list;

    list = back;
  }

  return ret;
}

/*
 * Sort maildir list according to inode.
 */
static struct maildir* maildir_sort (struct maildir* list, size_t len,
				     int (*cmp) (struct maildir *,
						 struct maildir *))
{
  struct maildir* left = list;
  struct maildir* right = list;
  size_t c = 0;

  if (!list || !list->next) 
  {
    return list;
  }

  if (len != (size_t)(-1) && len <= INS_SORT_THRESHOLD)
    return maildir_ins_sort (list, cmp);

  list = list->next;
  while (list && list->next) 
  {
    right = right->next;
    list = list->next->next;
    c++;
  }

  list = right;
  right = right->next;
  list->next = 0;

  left = maildir_sort (left, c, cmp);
  right = maildir_sort (right, c, cmp);
  return maildir_merge_lists (left, right, cmp);
}

/* Sorts mailbox into it's natural order.
 * Currently only defined for MH where files are numbered.
 */
static void mh_sort_natural (CONTEXT *ctx, struct maildir **md)
{
  if (!ctx || !md || !*md || ctx->magic != M_MH || Sort != SORT_ORDER)
    return;
  dprint (4, (debugfile, "maildir: sorting %s into natural order\n",
	      ctx->path));
  *md = maildir_sort (*md, (size_t) -1, md_cmp_path);
}

#if HAVE_DIRENT_D_INO
static struct maildir *skip_duplicates (struct maildir *p, struct maildir **last)
{
  /*
   * Skip ahead to the next non-duplicate message.
   *
   * p should never reach NULL, because we couldn't have reached this point unless
   * there was a message that needed to be parsed.
   *
   * the check for p->header_parsed is likely unnecessary since the dupes will most
   * likely be at the head of the list.  but it is present for consistency with
   * the check at the top of the for() loop in maildir_delayed_parsing().
   */
  while (!p->h || p->header_parsed) {
    *last = p;
    p = p->next;
  }
  return p;
}
#endif

/* 
 * This function does the second parsing pass
 */
static void maildir_delayed_parsing (CONTEXT * ctx, struct maildir **md,
			      progress_t *progress)
{ 
  struct maildir *p, *last = NULL;
  char fn[_POSIX_PATH_MAX];
  int count;
#if HAVE_DIRENT_D_INO
  int sort = 0;
#endif
#if USE_HCACHE
  header_cache_t *hc = NULL;
  void *data;
  struct timeval *when = NULL;
  struct stat lastchanged;
  int ret;
#endif

#if HAVE_DIRENT_D_INO
#define DO_SORT()	do { \
  if (!sort) \
  { \
    dprint (4, (debugfile, "maildir: need to sort %s by inode\n", ctx->path)); \
    p = maildir_sort (p, (size_t) -1, md_cmp_inode); \
    if (!last) \
      *md = p; \
    else \
      last->next = p; \
    sort = 1; \
    p = skip_duplicates (p, &last); \
    snprintf (fn, sizeof (fn), "%s/%s", ctx->path, p->h->path); \
  } \
} while(0)
#else
#define DO_SORT()	/* nothing */
#endif

#if USE_HCACHE
  hc = mutt_hcache_open (HeaderCache, ctx->path, NULL);
#endif

  for (p = *md, count = 0; p; p = p->next, count++)
   {
    if (! (p && p->h && !p->header_parsed))
     {
      last = p;
      continue;
    }

    if (!ctx->quiet && progress)
      mutt_progress_update (progress, count, -1);

    DO_SORT();

    snprintf (fn, sizeof (fn), "%s/%s", ctx->path, p->h->path);

#if USE_HCACHE
    if (option(OPTHCACHEVERIFY))
    {
       ret = stat(fn, &lastchanged);
    }
    else
    {
      lastchanged.st_mtime = 0;
      ret = 0;
    }

    if (ctx->magic == M_MH)
      data = mutt_hcache_fetch (hc, p->h->path, strlen);
    else
      data = mutt_hcache_fetch (hc, p->h->path + 3, &maildir_hcache_keylen);
    when = (struct timeval *) data;

    if (data != NULL && !ret && lastchanged.st_mtime <= when->tv_sec)
    {
      p->h = mutt_hcache_restore ((unsigned char *)data, &p->h);
      if (ctx->magic == M_MAILDIR)
	maildir_parse_flags (p->h, fn);
    }
    else
    {
#endif /* USE_HCACHE */

    if (maildir_parse_message (ctx->magic, fn, p->h->old, p->h))
    {
      p->header_parsed = 1;
#if USE_HCACHE
      if (ctx->magic == M_MH)
	mutt_hcache_store (hc, p->h->path, p->h, 0, strlen);
      else
	mutt_hcache_store (hc, p->h->path + 3, p->h, 0, &maildir_hcache_keylen);
#endif
    } else
      mutt_free_header (&p->h);
#if USE_HCACHE
    }
    FREE (&data);
#endif
    last = p;
   }
#if USE_HCACHE
  mutt_hcache_close (hc);
#endif

#undef DO_SORT

  mh_sort_natural (ctx, md);
}

static int mh_close_mailbox (CONTEXT *ctx)
{
  FREE (&ctx->data);

  return 0;
}

/* Read a MH/maildir style mailbox.
 *
 * args:
 *	ctx [IN/OUT]	context for this mailbox
 *	subdir [IN]	NULL for MH mailboxes, otherwise the subdir of the
 *			maildir mailbox to read from
 */
int mh_read_dir (CONTEXT * ctx, const char *subdir)
{
  struct maildir *md;
  struct mh_sequences mhs;
  struct maildir **last;
  struct mh_data *data;
  int count;
  char msgbuf[STRING];
  progress_t progress;

  memset (&mhs, 0, sizeof (mhs));
  if (!ctx->quiet)
  {
    snprintf (msgbuf, sizeof (msgbuf), _("Scanning %s..."), ctx->path);
    mutt_progress_init (&progress, msgbuf, M_PROGRESS_MSG, ReadInc, 0);
  }

  if (!ctx->data)
  {
    ctx->data = safe_calloc(sizeof (struct mh_data), 1);
    ctx->mx_close = mh_close_mailbox;
  }
  data = mh_data (ctx);

  maildir_update_mtime (ctx);

  md = NULL;
  last = &md;
  count = 0;
  if (maildir_parse_dir (ctx, &last, subdir, &count, &progress) == -1)
    return -1;

  if (!ctx->quiet)
  {
    snprintf (msgbuf, sizeof (msgbuf), _("Reading %s..."), ctx->path);
    mutt_progress_init (&progress, msgbuf, M_PROGRESS_MSG, ReadInc, count);
  }
  maildir_delayed_parsing (ctx, &md, &progress);

  if (ctx->magic == M_MH)
  {
    if (mh_read_sequences (&mhs, ctx->path) >= 0)
      return -1;
    mh_update_maildir (md, &mhs);
    mhs_free_sequences (&mhs);
  }

  maildir_move_to_context (ctx, &md);

  if (!data->mh_umask)
    data->mh_umask = mh_umask (ctx);

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

/*
 * Open a new (temporary) message in an MH folder.
 */

int mh_open_new_message (MESSAGE * msg, CONTEXT * dest, HEADER * hdr)
{
  return mh_mkstemp (dest, &msg->fp, &msg->path);
}

static int ch_compar (const void *a, const void *b)
{
  return (int)( *((const char *) a) - *((const char *) b));
}

static void maildir_flags (char *dest, size_t destlen, HEADER * hdr)
{
  *dest = '\0';

  /*
   * The maildir specification requires that all files in the cur
   * subdirectory have the :unique string appeneded, regardless of whether
   * or not there are any flags.  If .old is set, we know that this message
   * will end up in the cur directory, so we include it in the following
   * test even though there is no associated flag.
   */
  
  if (hdr && (hdr->flagged || hdr->replied || hdr->read || hdr->deleted || hdr->old || hdr->maildir_flags))
  {
    char tmp[LONG_STRING];
    snprintf (tmp, sizeof (tmp),
	      "%s%s%s%s%s",
	      hdr->flagged ? "F" : "",
	      hdr->replied ? "R" : "",
	      hdr->read ? "S" : "", hdr->deleted ? "T" : "",
	      NONULL(hdr->maildir_flags));
    if (hdr->maildir_flags)
      qsort (tmp, strlen (tmp), 1, ch_compar);
    snprintf (dest, destlen, ":2,%s", tmp);
  }
}


/*
 * Open a new (temporary) message in a maildir folder.
 * 
 * Note that this uses _almost_ the maildir file name format, but
 * with a {cur,new} prefix.
 *
 */

int maildir_open_new_message (MESSAGE * msg, CONTEXT * dest, HEADER * hdr)
{
  int fd;
  char path[_POSIX_PATH_MAX];
  char suffix[16];
  char subdir[16];
  mode_t omask;

  if (hdr)
  {
    short deleted = hdr->deleted;
    hdr->deleted = 0;

    maildir_flags (suffix, sizeof (suffix), hdr);

    hdr->deleted = deleted;
  }
  else
    *suffix = '\0';

  if (hdr && (hdr->read || hdr->old))
    strfcpy (subdir, "cur", sizeof (subdir));
  else
    strfcpy (subdir, "new", sizeof (subdir));

  omask = umask (mh_umask (dest));
  FOREVER
  {
    snprintf (path, _POSIX_PATH_MAX, "%s/tmp/%s.%lld.%u_%d.%s%s",
	      dest->path, subdir, (long long)time (NULL), (unsigned int)getpid (),
	      Counter++, NONULL (Hostname), suffix);

    dprint (2, (debugfile, "maildir_open_new_message (): Trying %s.\n",
		path));

    if ((fd = open (path, O_WRONLY | O_EXCL | O_CREAT, 0666)) == -1)
    {
      if (errno != EEXIST)
      {
	umask (omask);
	mutt_perror (path);
	return -1;
      }
    }
    else
    {
      dprint (2, (debugfile, "maildir_open_new_message (): Success.\n"));
      msg->path = safe_strdup (path);
      break;
    }
  }
  umask (omask);

  if ((msg->fp = fdopen (fd, "w")) == NULL)
  {
    FREE (&msg->path);
    close (fd);
    unlink (path);
    return (-1);
  }

  return 0;
}



/*
 * Commit a message to a maildir folder.
 * 
 * msg->path contains the file name of a file in tmp/. We take the
 * flags from this file's name. 
 *
 * ctx is the mail folder we commit to.
 * 
 * hdr is a header structure to which we write the message's new
 * file name.  This is used in the mh and maildir folder synch
 * routines.  When this routine is invoked from mx_commit_message,
 * hdr is NULL. 
 *
 * msg->path looks like this:
 * 
 *    tmp/{cur,new}.mutt-HOSTNAME-PID-COUNTER:flags
 * 
 * See also maildir_open_new_message().
 * 
 */

int maildir_commit_message (CONTEXT * ctx, MESSAGE * msg, HEADER * hdr)
{
  char subdir[4];
  char suffix[16];
  char path[_POSIX_PATH_MAX];
  char full[_POSIX_PATH_MAX];
  char *s;

  if (safe_fsync_close (&msg->fp))
  {
    mutt_perror (_("Could not flush message to disk"));
    return -1;
  }

  /* extract the subdir */
  s = strrchr (msg->path, '/') + 1;
  strfcpy (subdir, s, 4);

  /* extract the flags */
  if ((s = strchr (s, ':')))
    strfcpy (suffix, s, sizeof (suffix));
  else
    suffix[0] = '\0';

  /* construct a new file name. */
  FOREVER
  {
    snprintf (path, _POSIX_PATH_MAX, "%s/%lld.%u_%d.%s%s", subdir,
	      (long long)time (NULL), (unsigned int)getpid (), Counter++,
	      NONULL (Hostname), suffix);
    snprintf (full, _POSIX_PATH_MAX, "%s/%s", ctx->path, path);

    dprint (2, (debugfile, "maildir_commit_message (): renaming %s to %s.\n",
		msg->path, full));

    if (safe_rename (msg->path, full) == 0)
    {
      if (hdr)
	mutt_str_replace (&hdr->path, path);
      FREE (&msg->path);

      /*
       * Adjust the mtime on the file to match the time at which this
       * message was received.  Currently this is only set when copying
       * messages between mailboxes, so we test to ensure that it is
       * actually set.
       */
      if (msg->received)
      {
	struct utimbuf ut;

	ut.actime = msg->received;
	ut.modtime = msg->received;
	if (utime (full, &ut))
	{
	  mutt_perror (_("maildir_commit_message(): unable to set time on file"));
	  return -1;
	}
      }

      return 0;
    }
    else if (errno != EEXIST)
    {
      mutt_perror (ctx->path);
      return -1;
    }
  }
}

/* 
 * commit a message to an MH folder.
 * 
 */


static int _mh_commit_message (CONTEXT * ctx, MESSAGE * msg, HEADER * hdr,
			       short updseq)
{
  DIR *dirp;
  struct dirent *de;
  char *cp, *dep;
  unsigned int n, hi = 0;
  char path[_POSIX_PATH_MAX];
  char tmp[16];

  if (safe_fsync_close (&msg->fp))
  {
    mutt_perror (_("Could not flush message to disk"));
    return -1;
  }

  if ((dirp = opendir (ctx->path)) == NULL)
  {
    mutt_perror (ctx->path);
    return (-1);
  }

  /* figure out what the next message number is */
  while ((de = readdir (dirp)) != NULL)
  {
    dep = de->d_name;
    if (*dep == ',')
      dep++;
    cp = dep;
    while (*cp)
    {
      if (!isdigit ((unsigned char) *cp))
	break;
      cp++;
    }
    if (!*cp)
    {
      n = atoi (dep);
      if (n > hi)
	hi = n;
    }
  }
  closedir (dirp);

  /* 
   * Now try to rename the file to the proper name.
   * 
   * Note: We may have to try multiple times, until we find a free
   * slot.
   */

  FOREVER
  {
    hi++;
    snprintf (tmp, sizeof (tmp), "%d", hi);
    snprintf (path, sizeof (path), "%s/%s", ctx->path, tmp);
    if (safe_rename (msg->path, path) == 0)
    {
      if (hdr)
	mutt_str_replace (&hdr->path, tmp);
      FREE (&msg->path);
      break;
    }
    else if (errno != EEXIST)
    {
      mutt_perror (ctx->path);
      return -1;
    }
  }
  if (updseq)
    mh_sequences_add_one (ctx, hi, !msg->flags.read, msg->flags.flagged,
			  msg->flags.replied);
  return 0;
}

int mh_commit_message (CONTEXT * ctx, MESSAGE * msg, HEADER * hdr)
{
  return _mh_commit_message (ctx, msg, hdr, 1);
}


/* Sync a message in an MH folder.
 * 
 * This code is also used for attachment deletion in maildir
 * folders.
 */

static int mh_rewrite_message (CONTEXT * ctx, int msgno)
{
  HEADER *h = ctx->hdrs[msgno];
  MESSAGE *dest;

  int rc;
  short restore = 1;
  char oldpath[_POSIX_PATH_MAX];
  char newpath[_POSIX_PATH_MAX];
  char partpath[_POSIX_PATH_MAX];

  long old_body_offset = h->content->offset;
  long old_body_length = h->content->length;
  long old_hdr_lines = h->lines;

  if ((dest = mx_open_new_message (ctx, h, 0)) == NULL)
    return -1;

  if ((rc = mutt_copy_message (dest->fp, ctx, h,
			       M_CM_UPDATE, CH_UPDATE | CH_UPDATE_LEN)) == 0)
  {
    snprintf (oldpath, _POSIX_PATH_MAX, "%s/%s", ctx->path, h->path);
    strfcpy (partpath, h->path, _POSIX_PATH_MAX);

    if (ctx->magic == M_MAILDIR)
      rc = maildir_commit_message (ctx, dest, h);
    else
      rc = _mh_commit_message (ctx, dest, h, 0);

    mx_close_message (&dest);

    if (rc == 0)
    {
      unlink (oldpath);
      restore = 0;
    }

    /* 
     * Try to move the new message to the old place.
     * (MH only.)
     *
     * This is important when we are just updating flags.
     *
     * Note that there is a race condition against programs which
     * use the first free slot instead of the maximum message
     * number.  Mutt does _not_ behave like this.
     * 
     * Anyway, if this fails, the message is in the folder, so
     * all what happens is that a concurrently runnung mutt will
     * lose flag modifications.
     */

    if (ctx->magic == M_MH && rc == 0)
    {
      snprintf (newpath, _POSIX_PATH_MAX, "%s/%s", ctx->path, h->path);
      if ((rc = safe_rename (newpath, oldpath)) == 0)
	mutt_str_replace (&h->path, partpath);
    }
  }
  else
    mx_close_message (&dest);

  if (rc == -1 && restore)
  {
    h->content->offset = old_body_offset;
    h->content->length = old_body_length;
    h->lines = old_hdr_lines;
  }

  mutt_free_body (&h->content->parts);
  return rc;
}

static int mh_sync_message (CONTEXT * ctx, int msgno)
{
  HEADER *h = ctx->hdrs[msgno];

  if (h->attach_del || 
      (h->env && (h->env->refs_changed || h->env->irt_changed)))
    if (mh_rewrite_message (ctx, msgno) != 0)
      return -1;

  return 0;
}

static int maildir_sync_message (CONTEXT * ctx, int msgno)
{
  HEADER *h = ctx->hdrs[msgno];

  if (h->attach_del || 
      (h->env && (h->env->refs_changed || h->env->irt_changed)))
  {
    /* when doing attachment deletion/rethreading, fall back to the MH case. */
    if (mh_rewrite_message (ctx, msgno) != 0)
      return (-1);
  }
  else
  {
    /* we just have to rename the file. */

    char newpath[_POSIX_PATH_MAX];
    char partpath[_POSIX_PATH_MAX];
    char fullpath[_POSIX_PATH_MAX];
    char oldpath[_POSIX_PATH_MAX];
    char suffix[16];
    char *p;

    if ((p = strrchr (h->path, '/')) == NULL)
    {
      dprint (1,
	      (debugfile,
	       "maildir_sync_message: %s: unable to find subdir!\n",
	       h->path));
      return (-1);
    }
    p++;
    strfcpy (newpath, p, sizeof (newpath));

    /* kill the previous flags */
    if ((p = strchr (newpath, ':')) != NULL)
      *p = 0;

    maildir_flags (suffix, sizeof (suffix), h);

    snprintf (partpath, sizeof (partpath), "%s/%s%s",
	      (h->read || h->old) ? "cur" : "new", newpath, suffix);
    snprintf (fullpath, sizeof (fullpath), "%s/%s", ctx->path, partpath);
    snprintf (oldpath, sizeof (oldpath), "%s/%s", ctx->path, h->path);

    if (mutt_strcmp (fullpath, oldpath) == 0)
    {
      /* message hasn't really changed */
      return 0;
    }

    /* record that the message is possibly marked as trashed on disk */
    h->trash = h->deleted;

    if (rename (oldpath, fullpath) != 0)
    {
      mutt_perror ("rename");
      return (-1);
    }
    mutt_str_replace (&h->path, partpath);
  }
  return (0);
}

int mh_sync_mailbox (CONTEXT * ctx, int *index_hint)
{
  char path[_POSIX_PATH_MAX], tmp[_POSIX_PATH_MAX];
  int i, j;
#if USE_HCACHE
  header_cache_t *hc = NULL;
#endif /* USE_HCACHE */
  char msgbuf[STRING];
  progress_t progress;

  if (ctx->magic == M_MH)
    i = mh_check_mailbox (ctx, index_hint);
  else 
    i = maildir_check_mailbox (ctx, index_hint);
      
  if (i != 0)
    return i;

#if USE_HCACHE
  if (ctx->magic == M_MAILDIR || ctx->magic == M_MH)
    hc = mutt_hcache_open(HeaderCache, ctx->path, NULL);
#endif /* USE_HCACHE */

  if (!ctx->quiet)
  {
    snprintf (msgbuf, sizeof (msgbuf), _("Writing %s..."), ctx->path);
    mutt_progress_init (&progress, msgbuf, M_PROGRESS_MSG, WriteInc, ctx->msgcount);
  }

  for (i = 0; i < ctx->msgcount; i++)
  {
    if (!ctx->quiet)
      mutt_progress_update (&progress, i, -1);

    if (ctx->hdrs[i]->deleted
	&& (ctx->magic != M_MAILDIR || !option (OPTMAILDIRTRASH)))
    {
      snprintf (path, sizeof (path), "%s/%s", ctx->path, ctx->hdrs[i]->path);
      if (ctx->magic == M_MAILDIR
	  || (option (OPTMHPURGE) && ctx->magic == M_MH))
      {
#if USE_HCACHE
        if (ctx->magic == M_MAILDIR)
          mutt_hcache_delete (hc, ctx->hdrs[i]->path + 3, &maildir_hcache_keylen);
	else if (ctx->magic == M_MH)
	  mutt_hcache_delete (hc, ctx->hdrs[i]->path, strlen);
#endif /* USE_HCACHE */
	unlink (path);
      }
      else if (ctx->magic == M_MH)
      {
	/* MH just moves files out of the way when you delete them */
	if (*ctx->hdrs[i]->path != ',')
	{
	  snprintf (tmp, sizeof (tmp), "%s/,%s", ctx->path,
		    ctx->hdrs[i]->path);
	  unlink (tmp);
	  rename (path, tmp);
	}

      }
    }
    else if (ctx->hdrs[i]->changed || ctx->hdrs[i]->attach_del ||
	     (ctx->magic == M_MAILDIR
	      && (option (OPTMAILDIRTRASH) || ctx->hdrs[i]->trash)
	      && (ctx->hdrs[i]->deleted != ctx->hdrs[i]->trash)))
    {
      if (ctx->magic == M_MAILDIR)
      {
	if (maildir_sync_message (ctx, i) == -1)
	  goto err;
      }
      else
      {
	if (mh_sync_message (ctx, i) == -1)
	  goto err;
      }
    }

#if USE_HCACHE
    if (ctx->hdrs[i]->changed)
    {
      if (ctx->magic == M_MAILDIR)
	mutt_hcache_store (hc, ctx->hdrs[i]->path + 3, ctx->hdrs[i],
			   0, &maildir_hcache_keylen);
      else if (ctx->magic == M_MH)
	mutt_hcache_store (hc, ctx->hdrs[i]->path, ctx->hdrs[i], 0, strlen);
    }
#endif

  }

#if USE_HCACHE
  if (ctx->magic == M_MAILDIR || ctx->magic == M_MH)
    mutt_hcache_close (hc);
#endif /* USE_HCACHE */

  if (ctx->magic == M_MH)
    mh_update_sequences (ctx);

  /* XXX race condition? */

  maildir_update_mtime (ctx);

  /* adjust indices */

  if (ctx->deleted)
  {
    for (i = 0, j = 0; i < ctx->msgcount; i++)
    {
      if (!ctx->hdrs[i]->deleted
	  || (ctx->magic == M_MAILDIR && option (OPTMAILDIRTRASH)))
	ctx->hdrs[i]->index = j++;
    }
  }

  return 0;

err:
#if USE_HCACHE
  if (ctx->magic == M_MAILDIR || ctx->magic == M_MH)
    mutt_hcache_close (hc);
#endif /* USE_HCACHE */
  return -1;
}

static char *maildir_canon_filename (char *dest, const char *src, size_t l)
{
  char *t, *u;

  if ((t = strrchr (src, '/')))
    src = t + 1;

  strfcpy (dest, src, l);
  if ((u = strrchr (dest, ':')))
    *u = '\0';

  return dest;
}

static void maildir_update_tables (CONTEXT *ctx, int *index_hint)
{
  short old_sort;
  int old_count;
  int i, j;
  
  if (Sort != SORT_ORDER)
  {
    old_sort = Sort;
    Sort = SORT_ORDER;
    mutt_sort_headers (ctx, 1);
    Sort = old_sort;
  }
  
  old_count = ctx->msgcount;
  for (i = 0, j = 0; i < old_count; i++)
  {
    if (ctx->hdrs[i]->active && index_hint && *index_hint == i)
      *index_hint = j;
    
    if (ctx->hdrs[i]->active)
      ctx->hdrs[i]->index = j++;
  }

  mx_update_tables (ctx, 0);
  mutt_clear_threads (ctx);
}

static void maildir_update_flags (CONTEXT *ctx, HEADER *o, HEADER *n)
{
  /* save the global state here so we can reset it at the
   * end of list block if required.
   */
  int context_changed = ctx->changed;
  
  /* user didn't modify this message.  alter the flags to match the
   * current state on disk.  This may not actually do
   * anything. mutt_set_flag() will just ignore the call if the status
   * bits are already properly set, but it is still faster not to pass
   * through it */
  if (o->flagged != n->flagged)
    mutt_set_flag (ctx, o, M_FLAG, n->flagged);
  if (o->replied != n->replied)
    mutt_set_flag (ctx, o, M_REPLIED, n->replied);
  if (o->read != n->read)
    mutt_set_flag (ctx, o, M_READ, n->read);
  if (o->old != n->old)
    mutt_set_flag (ctx, o, M_OLD, n->old);

  /* mutt_set_flag() will set this, but we don't need to
   * sync the changes we made because we just updated the
   * context to match the current on-disk state of the
   * message.
   */
  o->changed = 0;
  
  /* if the mailbox was not modified before we made these
   * changes, unset the changed flag since nothing needs to
   * be synchronized.
   */
  if (!context_changed)
    ctx->changed = 0;
}


/* This function handles arrival of new mail and reopening of
 * maildir folders.  The basic idea here is we check to see if either
 * the new or cur subdirectories have changed, and if so, we scan them
 * for the list of files.  We check for newly added messages, and
 * then merge the flags messages we already knew about.  We don't treat
 * either subdirectory differently, as mail could be copied directly into
 * the cur directory from another agent.
 */
int maildir_check_mailbox (CONTEXT * ctx, int *index_hint)
{
  struct stat st_new;		/* status of the "new" subdirectory */
  struct stat st_cur;		/* status of the "cur" subdirectory */
  char buf[_POSIX_PATH_MAX];
  int changed = 0;		/* bitmask representing which subdirectories
				   have changed.  0x1 = new, 0x2 = cur */
  int occult = 0;		/* messages were removed from the mailbox */
  int have_new = 0;		/* messages were added to the mailbox */
  struct maildir *md;		/* list of messages in the mailbox */
  struct maildir **last, *p;
  int i;
  HASH *fnames;			/* hash table for quickly looking up the base filename
				   for a maildir message */
  struct mh_data *data = mh_data (ctx);

  /* XXX seems like this check belongs in mx_check_mailbox()
   * rather than here.
   */
  if (!option (OPTCHECKNEW))
    return 0;

  snprintf (buf, sizeof (buf), "%s/new", ctx->path);
  if (stat (buf, &st_new) == -1)
    return -1;

  snprintf (buf, sizeof (buf), "%s/cur", ctx->path);
  if (stat (buf, &st_cur) == -1)
    return -1;

  /* determine which subdirectories need to be scanned */
  if (st_new.st_mtime > ctx->mtime)
    changed = 1;
  if (st_cur.st_mtime > data->mtime_cur)
    changed |= 2;

  if (!changed)
    return 0;			/* nothing to do */

  /* update the modification times on the mailbox */
  data->mtime_cur = st_cur.st_mtime;
  ctx->mtime = st_new.st_mtime;

  /* do a fast scan of just the filenames in
   * the subdirectories that have changed.
   */
  md = NULL;
  last = &md;
  if (changed & 1)
    maildir_parse_dir (ctx, &last, "new", NULL, NULL);
  if (changed & 2)
    maildir_parse_dir (ctx, &last, "cur", NULL, NULL);

  /* we create a hash table keyed off the canonical (sans flags) filename
   * of each message we scanned.  This is used in the loop over the
   * existing messages below to do some correlation.
   */
  fnames = hash_create (1031, 0);

  for (p = md; p; p = p->next)
  {
    maildir_canon_filename (buf, p->h->path, sizeof (buf));
    p->canon_fname = safe_strdup (buf);
    hash_insert (fnames, p->canon_fname, p, 0);
  }

  /* check for modifications and adjust flags */
  for (i = 0; i < ctx->msgcount; i++)
  {
    ctx->hdrs[i]->active = 0;
    maildir_canon_filename (buf, ctx->hdrs[i]->path, sizeof (buf));
    p = hash_find (fnames, buf);
    if (p && p->h)
    {
      /* message already exists, merge flags */
      ctx->hdrs[i]->active = 1;

      /* check to see if the message has moved to a different
       * subdirectory.  If so, update the associated filename.
       */
      if (mutt_strcmp (ctx->hdrs[i]->path, p->h->path))
	mutt_str_replace (&ctx->hdrs[i]->path, p->h->path);

      /* if the user hasn't modified the flags on this message, update
       * the flags we just detected.
       */
      if (!ctx->hdrs[i]->changed)
	maildir_update_flags (ctx, ctx->hdrs[i], p->h);

      if (ctx->hdrs[i]->deleted == ctx->hdrs[i]->trash)
	ctx->hdrs[i]->deleted = p->h->deleted;
      ctx->hdrs[i]->trash = p->h->trash;

      /* this is a duplicate of an existing header, so remove it */
      mutt_free_header (&p->h);
    }
    /* This message was not in the list of messages we just scanned.
     * Check to see if we have enough information to know if the
     * message has disappeared out from underneath us.
     */
    else if (((changed & 1) && (!strncmp (ctx->hdrs[i]->path, "new/", 4))) ||
	     ((changed & 2) && (!strncmp (ctx->hdrs[i]->path, "cur/", 4))))
    {
      /* This message disappeared, so we need to simulate a "reopen"
       * event.  We know it disappeared because we just scanned the
       * subdirectory it used to reside in.
       */
      occult = 1;
    }
    else
    {
      /* This message resides in a subdirectory which was not
       * modified, so we assume that it is still present and
       * unchanged.
       */
      ctx->hdrs[i]->active = 1;
    }
  }

  /* destroy the file name hash */
  hash_destroy (&fnames, NULL);

  /* If we didn't just get new mail, update the tables. */
  if (occult)
    maildir_update_tables (ctx, index_hint);
  
  /* do any delayed parsing we need to do. */
  maildir_delayed_parsing (ctx, &md, NULL);

  /* Incorporate new messages */
  have_new = maildir_move_to_context (ctx, &md);

  return occult ? M_REOPENED : (have_new ? M_NEW_MAIL : 0);
}

/* 
 * This function handles arrival of new mail and reopening of
 * mh/maildir folders. Things are getting rather complex because we
 * don't have a well-defined "mailbox order", so the tricks from
 * mbox.c and mx.c won't work here.
 *
 * Don't change this code unless you _really_ understand what
 * happens.
 *
 */

int mh_check_mailbox (CONTEXT * ctx, int *index_hint)
{
  char buf[_POSIX_PATH_MAX];
  struct stat st, st_cur;
  short modified = 0, have_new = 0, occult = 0;
  struct maildir *md, *p;
  struct maildir **last = NULL;
  struct mh_sequences mhs;
  HASH *fnames;
  int i;
  struct mh_data *data = mh_data (ctx);

  if (!option (OPTCHECKNEW))
    return 0;

  strfcpy (buf, ctx->path, sizeof (buf));
  if (stat (buf, &st) == -1)
    return -1;
  
  /* create .mh_sequences when there isn't one. */
  snprintf (buf, sizeof (buf), "%s/.mh_sequences", ctx->path);
  if ((i = stat (buf, &st_cur)) == -1 && errno == ENOENT)
  {
    char *tmp;
    FILE *fp = NULL;
    
    if (mh_mkstemp (ctx, &fp, &tmp) == 0)
    {
      safe_fclose (&fp);
      if (safe_rename (tmp, buf) == -1)
	unlink (tmp);
      FREE (&tmp);
    }
  }

  if (i == -1 && stat (buf, &st_cur) == -1)
    modified = 1;

  if (st.st_mtime > ctx->mtime || st_cur.st_mtime > data->mtime_cur)
    modified = 1;

  if (!modified)
    return 0;

  data->mtime_cur = st_cur.st_mtime;
  ctx->mtime = st.st_mtime;

  memset (&mhs, 0, sizeof (mhs));

  md   = NULL;
  last = &md;

  maildir_parse_dir (ctx, &last, NULL, NULL, NULL);
  maildir_delayed_parsing (ctx, &md, NULL);

  if (mh_read_sequences (&mhs, ctx->path) < 0)
    return -1;
  mh_update_maildir (md, &mhs);
  mhs_free_sequences (&mhs);

  /* check for modifications and adjust flags */
  fnames = hash_create (1031, 0);

  for (p = md; p; p = p->next)
    hash_insert (fnames, p->h->path, p, 0);

  for (i = 0; i < ctx->msgcount; i++)
  {
    ctx->hdrs[i]->active = 0;

    if ((p = hash_find (fnames, ctx->hdrs[i]->path)) && p->h &&
	(mbox_strict_cmp_headers (ctx->hdrs[i], p->h)))
    {
      ctx->hdrs[i]->active = 1;
      /* found the right message */
      if (!ctx->hdrs[i]->changed)
	maildir_update_flags (ctx, ctx->hdrs[i], p->h);

      mutt_free_header (&p->h);
    }
    else /* message has disappeared */
      occult = 1;
  }

  /* destroy the file name hash */

  hash_destroy (&fnames, NULL);

  /* If we didn't just get new mail, update the tables. */
  if (occult)
    maildir_update_tables (ctx, index_hint);

  /* Incorporate new messages */
  have_new = maildir_move_to_context (ctx, &md);

  return occult ? M_REOPENED : (have_new ? M_NEW_MAIL : 0);
}




/*
 * These functions try to find a message in a maildir folder when it
 * has moved under our feet.  Note that this code is rather expensive, but
 * then again, it's called rarely.
 */

static FILE *_maildir_open_find_message (const char *folder, const char *unique,
				  const char *subfolder)
{
  char dir[_POSIX_PATH_MAX];
  char tunique[_POSIX_PATH_MAX];
  char fname[_POSIX_PATH_MAX];

  DIR *dp;
  struct dirent *de;

  FILE *fp = NULL;
  int oe = ENOENT;

  snprintf (dir, sizeof (dir), "%s/%s", folder, subfolder);

  if ((dp = opendir (dir)) == NULL)
  {
    errno = ENOENT;
    return NULL;
  }

  while ((de = readdir (dp)))
  {
    maildir_canon_filename (tunique, de->d_name, sizeof (tunique));

    if (!mutt_strcmp (tunique, unique))
    {
      snprintf (fname, sizeof (fname), "%s/%s/%s", folder, subfolder,
		de->d_name);
      fp = fopen (fname, "r");	/* __FOPEN_CHECKED__ */
      oe = errno;
      break;
    }
  }

  closedir (dp);

  errno = oe;
  return fp;
}

FILE *maildir_open_find_message (const char *folder, const char *msg)
{
  char unique[_POSIX_PATH_MAX];
  FILE *fp;

  static unsigned int new_hits = 0, cur_hits = 0;	/* simple dynamic optimization */

  maildir_canon_filename (unique, msg, sizeof (unique));

  if (
      (fp =
       _maildir_open_find_message (folder, unique,
				   new_hits > cur_hits ? "new" : "cur"))
      || errno != ENOENT)
  {
    if (new_hits < UINT_MAX && cur_hits < UINT_MAX)
    {
      new_hits += (new_hits > cur_hits ? 1 : 0);
      cur_hits += (new_hits > cur_hits ? 0 : 1);
    }

    return fp;
  }
  if (
      (fp =
       _maildir_open_find_message (folder, unique,
				   new_hits > cur_hits ? "cur" : "new"))
      || errno != ENOENT)
  {
    if (new_hits < UINT_MAX && cur_hits < UINT_MAX)
    {
      new_hits += (new_hits > cur_hits ? 0 : 1);
      cur_hits += (new_hits > cur_hits ? 1 : 0);
    }

    return fp;
  }

  return NULL;
}


/*
 * Returns:
 * 1 if there are no messages in the mailbox
 * 0 if there are messages in the mailbox
 * -1 on error
 */
int maildir_check_empty (const char *path)
{
  DIR *dp;
  struct dirent *de;
  int r = 1; /* assume empty until we find a message */
  char realpath[_POSIX_PATH_MAX];
  int iter = 0;

  /* Strategy here is to look for any file not beginning with a period */

  do {
    /* we do "cur" on the first iteration since its more likely that we'll
     * find old messages without having to scan both subdirs
     */
    snprintf (realpath, sizeof (realpath), "%s/%s", path,
	      iter == 0 ? "cur" : "new");
    if ((dp = opendir (realpath)) == NULL)
      return -1;
    while ((de = readdir (dp)))
    {
      if (*de->d_name != '.')
      {
	r = 0;
	break;
      }
    }
    closedir (dp);
    iter++;
  } while (r && iter < 2);

  return r;
}

/*
 * Returns:
 * 1 if there are no messages in the mailbox
 * 0 if there are messages in the mailbox
 * -1 on error
 */
int mh_check_empty (const char *path)
{
  DIR *dp;
  struct dirent *de;
  int r = 1; /* assume empty until we find a message */
  
  if ((dp = opendir (path)) == NULL)
    return -1;
  while ((de = readdir (dp)))
  {
    if (mh_valid_message (de->d_name))
    {
      r = 0;
      break;
    }
  }
  closedir (dp);
  
  return r;
}

int mx_is_maildir (const char *path)
{
  char tmp[_POSIX_PATH_MAX];
  struct stat st;

  snprintf (tmp, sizeof (tmp), "%s/cur", path);
  if (stat (tmp, &st) == 0 && S_ISDIR (st.st_mode))
    return 1;
  return 0;
}

int mx_is_mh (const char *path)
{
  char tmp[_POSIX_PATH_MAX];

  snprintf (tmp, sizeof (tmp), "%s/.mh_sequences", path);
  if (access (tmp, F_OK) == 0)
    return 1;

  snprintf (tmp, sizeof (tmp), "%s/.xmhcache", path);
  if (access (tmp, F_OK) == 0)
    return 1;

  snprintf (tmp, sizeof (tmp), "%s/.mew_cache", path);
  if (access (tmp, F_OK) == 0)
    return 1;

  snprintf (tmp, sizeof (tmp), "%s/.mew-cache", path);
  if (access (tmp, F_OK) == 0)
    return 1;

  snprintf (tmp, sizeof (tmp), "%s/.sylpheed_cache", path);
  if (access (tmp, F_OK) == 0)
    return 1;

  /*
   * ok, this isn't an mh folder, but mh mode can be used to read
   * Usenet news from the spool. ;-)
   */

  snprintf (tmp, sizeof (tmp), "%s/.overview", path);
  if (access (tmp, F_OK) == 0)
    return 1;

  return 0;
}
