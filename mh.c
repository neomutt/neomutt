/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (C) 1999-2000 Thomas Roessler <roessler@guug.de>
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

/*
 * This file contains routines specific to MH and ``maildir'' style
 * mailboxes.
 */

#include "mutt.h"
#include "mailbox.h"
#include "mx.h"
#include "copy.h"
#include "buffy.h"
#include "sort.h"

#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>

struct maildir
{
  HEADER *h;
  char *canon_fname;
  struct maildir *next;
};

struct mh_sequences
{
  int max;
  short *flags;
};

/* mh_sequences support */

#define MH_SEQ_UNSEEN  (1 << 0)
#define MH_SEQ_REPLIED (1 << 1)
#define MH_SEQ_FLAGGED (1 << 2)

static void mhs_alloc (struct mh_sequences *mhs, int i)
{
  int j;
  int newmax;
  
  if (i > mhs->max)
  {
    newmax = i + 128;
    safe_realloc ((void **) &mhs->flags, sizeof (mhs->flags[0]) * (newmax + 1));
    for (j = mhs->max + 1; j <= newmax; j++)
      mhs->flags[j] = 0;
    
    mhs->max = newmax;
  }
}

static void mhs_free_sequences (struct mh_sequences *mhs)
{
  safe_free ((void **) &mhs->flags);
}

static short mhs_check (struct mh_sequences *mhs, int i)
{
  if (i > mhs->max)
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

static short mhs_unset (struct mh_sequences *mhs, int i, short f)
{
  mhs_alloc (mhs, i);
  mhs->flags[i] &= ~f;
  return mhs->flags[i];
}

static void mh_read_token (char *t, int *first, int *last)
{
  char *p;
  if ((p = strchr (t, '-')))
  {
    *p++ = '\0';
    *first = atoi (t);
    *last  = atoi (p);
  }
  else
    *first = *last = atoi (t);
}

static void mh_read_sequences (struct mh_sequences *mhs, char *path)
{
  FILE *fp;
  int line = 1;
  char *buff = NULL;
  char *t;
  size_t sz = 0;
  
  short f;
  int first, last;

  char pathname[_POSIX_PATH_MAX];
  snprintf (pathname, sizeof (pathname),  "%s/.mh_sequences", path);

  if (!(fp = fopen (pathname, "r")))
    return;

  while ((buff = mutt_read_line (buff, &sz, fp, &line)))
  {
    if (!(t = strtok (buff, " \t:")))
      continue;
    
    if (!strcmp (t, "unseen"))
      f = MH_SEQ_UNSEEN;
    else if (!strcmp (t, "flagged"))
      f = MH_SEQ_FLAGGED;
    else if (!strcmp (t, "replied"))
      f = MH_SEQ_REPLIED;
    else	/* unknown sequence */
      continue;
    
    while ((t = strtok (NULL, " \t:")))
    {
      mh_read_token (t, &first, &last);
      for (; first <= last; first++)
	mhs_set (mhs, first, f);
    }
  }

  safe_free ((void **) &buff);
  safe_fclose (&fp);
}

static int mh_mkstemp (CONTEXT *dest, FILE **fp, char **tgt)
{
  int fd;
  char path[_POSIX_PATH_MAX];

  FOREVER 
  {
    snprintf (path, _POSIX_PATH_MAX, "%s/.mutt-%s-%d-%d",
	      dest->path, NONULL (Hostname), (int) getpid (), Counter++);
    if ((fd = open (path, O_WRONLY | O_EXCL | O_CREAT, 0600)) == -1)
    {
      if (errno != EEXIST)
      {
	mutt_perror (path);
	return -1;
      }
    }
    else
    {
      *tgt = safe_strdup (path);
      break;
    }
  }

  if ((*fp = fdopen (fd, "w")) == NULL)
  {
    FREE (tgt);
    close (fd);
    unlink (path);
    return (-1);
  }
  
  return 0;
}

static void mhs_write_one_sequence (FILE *fp, struct mh_sequences *mhs,
				    short f, const char *tag)
{
  int i;
  int first, last;
  fprintf (fp, "%s:", tag);
  
  first = -1;
  last  = -1;
  
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

void mh_update_sequences (CONTEXT *ctx)
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
  
  struct mh_sequences mhs;
  memset (&mhs, 0, sizeof (mhs));
  
  if (mh_mkstemp (ctx, &nfp, &tmpfname) != 0)
  {
    /* error message? */
    return;
  }

  snprintf (sequences, sizeof (sequences), "%s/.mh_sequences", ctx->path);

  
  /* first, copy unknown sequences */
  if ((ofp = fopen (sequences, "r")))
  {
    while ((buff = mutt_read_line (buff, &s, ofp, &l)))
    {
      if (!strncmp (buff, "unseen:", 7))
	continue;
      if (!strncmp (buff, "flagged:", 8))
	continue;
      if (!strncmp (buff, "replied:", 8))
	continue;
	
      fprintf (nfp, "%s\n", buff);
    }
  }
  safe_fclose (&ofp);
  
  /* now, update our unseen, flagged, and replied sequences */
  for (l = 0; l < ctx->msgcount; l++)
  {
    if ((p = strrchr (ctx->hdrs[l]->path, '/')))
      p++;
    else
      p = ctx->hdrs[l]->path;

    i = atoi (p);
    
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
  if (unseen)  mhs_write_one_sequence (nfp, &mhs, MH_SEQ_UNSEEN, "unseen");
  if (flagged) mhs_write_one_sequence (nfp, &mhs, MH_SEQ_FLAGGED, "flagged");
  if (replied) mhs_write_one_sequence (nfp, &mhs, MH_SEQ_REPLIED, "replied");

  mhs_free_sequences (&mhs);

  
  /* try to commit the changes - no guarantee here */
  safe_fclose (&nfp);
  
  unlink (sequences);
  if (safe_rename (tmpfname, sequences) != 0)
  {
    /* report an error? */
    unlink (tmpfname);
  }
  
  safe_free ((void **) &tmpfname);
}

static void mh_sequences_add_one (CONTEXT *ctx, int n, short unseen, short flagged, short replied)
{
  short unseen_done = 0;
  short flagged_done = 0;
  short replied_done = 0;
  
  FILE *ofp = NULL, *nfp = NULL;
 
  char *tmpfname;
  char sequences[_POSIX_PATH_MAX];
  
  char *buff = NULL;
  int line;
  size_t sz;
  
  if (mh_mkstemp (ctx, &nfp, &tmpfname) == -1)
    return;
  
  snprintf (sequences, sizeof (sequences), "%s/.mh_sequences", ctx->path);
  if ((ofp = fopen (sequences, "r")))
  {
    while ((buff = mutt_read_line (buff, &sz, ofp, &line)))
    {
      if (unseen && !strncmp (buff, "unseen:", 7))
      {
	fprintf (nfp, "%s %d\n", buff, n);
	unseen_done = 1;
      }
      else if (flagged && !strncmp (buff, "flagged:", 8))
      {
	fprintf (nfp, "%s %d\n", buff, n);
	flagged_done = 1;
      }
      else if (replied && !strncmp (buff, "replied:", 8))
      {
	fprintf (nfp, "%s %d\n", buff, n);
	replied_done = 1;
      }
      else
	fprintf (nfp, "%s\n", buff);
    }
  }
  safe_fclose (&ofp);
  safe_free ((void **) &buff);
  
  if (!unseen_done  && unseen)   fprintf (nfp, "unseen: %d\n",  n);
  if (!flagged_done && flagged)  fprintf (nfp, "flagged: %d\n", n);
  if (!replied_done && replied)  fprintf (nfp, "replied: %d\n", n);
  
  safe_fclose (&nfp);
  
  unlink (sequences);
  if (safe_rename (tmpfname, sequences) != 0)
    unlink (tmpfname);
  
  safe_free ((void **) &tmpfname);
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

    i = atoi (p);
    f = mhs_check (mhs, i);
    
    md->h->read    = (f & MH_SEQ_UNSEEN)  ? 0 : 1;
    md->h->flagged = (f & MH_SEQ_FLAGGED) ? 1 : 0;
    md->h->replied = (f & MH_SEQ_REPLIED) ? 1 : 0;
  }
}

/* maildir support */

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
  
  if ((p = strrchr (path, ':')) != NULL && mutt_strncmp (p + 1, "2,", 2) == 0)
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

	case 'T': /* trashed */

	h->deleted = 1;
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
  {
    snprintf (buf, sizeof (buf), "%s/.mh_sequences", ctx->path);
    if (stat (buf, &st) == 0)
      ctx->mtime_cur = st.st_mtime;
    
    strfcpy(buf, ctx->path, sizeof(buf));
  }
  
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
    h->env = mutt_read_rfc822_header (f, h, 0, 0);

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
	mutt_message (_("Reading %s... %d"), ctx->path, *count);
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
    is_old = (mutt_strcmp("cur", subdir) == 0) && option(OPTMARKOLD);
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

    dprint(2, (debugfile, "%s:%d: parsing %s\n", __FILE__, __LINE__, de->d_name));
    maildir_parse_entry(ctx, last, subdir, de->d_name, count, is_old);
  }

  closedir(dirp);
  return 0;
}

static void maildir_add_to_context(CONTEXT *ctx, struct maildir *md)
{
  while(md)
  {
    
    dprint(2, (debugfile, "%s:%d maildir_add_to_context(): Considering %s\n",
	       __FILE__, __LINE__, md->canon_fname));
    
    if(md->h)
    {

      dprint(2, (debugfile, "%s:%d Adding header structure. Flags: %s%s%s%s%s\n", __FILE__, __LINE__,
		 md->h->flagged ? "f" : "",
		 md->h->deleted ? "D" : "",
		 md->h->replied ? "r" : "",
		 md->h->old     ? "O" : "",
		 md->h->read    ? "R" : ""));
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
  struct mh_sequences mhs;
  struct maildir **last;
  int count;
  
  md = NULL;
  last = &md;
  count = 0;

  memset (&mhs, 0, sizeof (mhs));
  
  maildir_update_mtime(ctx);

  if(maildir_parse_dir(ctx, &last, subdir, &count) == -1)
    return -1;

  mh_read_sequences (&mhs, ctx->path);
  mh_update_maildir (md, &mhs);
  mhs_free_sequences (&mhs);
  
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

/*
 * Open a new (temporary) message in an MH folder.
 */

int mh_open_new_message (MESSAGE *msg, CONTEXT *dest, HEADER *hdr)
{
  return mh_mkstemp (dest, &msg->fp, &msg->path);
}

static void maildir_flags (char *dest, size_t destlen, HEADER *hdr)
{
  *dest = '\0';
  
  if (hdr && (hdr->flagged || hdr->replied || hdr->read || hdr->deleted))
  {
    snprintf (dest, destlen, 
	      ":2,%s%s%s%s",
	     hdr->flagged ? "F" : "",
	     hdr->replied ? "R" : "",
	     hdr->read ? "S" : "",
	     hdr->deleted ? "T" : "");
  }
}
    
  
/*
 * Open a new (temporary) message in a maildir folder.
 * 
 * Note that this uses _almost_ the maildir file name format, but
 * with a {cur,new} prefix.
 *
 */

int maildir_open_new_message (MESSAGE *msg, CONTEXT *dest, HEADER *hdr)
{
  int fd;
  char path[_POSIX_PATH_MAX];
  char suffix[16];
  char subdir[16];

  maildir_flags (suffix, sizeof (suffix), hdr);
		 
  if (hdr && (hdr->read || hdr->old))
    strfcpy (subdir, "cur", sizeof (subdir));
  else
    strfcpy (subdir, "new", sizeof (subdir));

  FOREVER
  {
    snprintf (path, _POSIX_PATH_MAX, "%s/tmp/%s.%ld.%d_%d.%s%s",
	     dest->path, subdir, time (NULL), getpid (), Counter++,
	     NONULL (Hostname), suffix);

    dprint (2, (debugfile, "maildir_open_new_message (): Trying %s.\n",
		path));

    if ((fd = open (path, O_WRONLY | O_EXCL | O_CREAT, 0600)) == -1)
    {
      if (errno != EEXIST)
      {
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

int maildir_commit_message (CONTEXT *ctx, MESSAGE *msg, HEADER *hdr)
{
  char subdir[4];
  char suffix[16];
  char path[_POSIX_PATH_MAX];
  char full[_POSIX_PATH_MAX];
  char *s;

  if (safe_fclose (&msg->fp) != 0)
    return -1;
  
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
    snprintf (path, _POSIX_PATH_MAX, "%s/%ld.%d_%d.%s%s", subdir,
	      time (NULL), getpid(), Counter++, NONULL (Hostname), suffix);
    snprintf (full, _POSIX_PATH_MAX, "%s/%s", ctx->path, path);

    dprint (2, (debugfile, "maildir_commit_message (): renaming %s to %s.\n",
		msg->path, full));

    if (safe_rename (msg->path, full) == 0)
    {
      if (hdr) 
	mutt_str_replace (&hdr->path, path);
      FREE (&msg->path);
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


static int _mh_commit_message (CONTEXT *ctx, MESSAGE *msg, HEADER *hdr, short updseq)
{
  DIR *dirp;
  struct dirent *de;
  char *cp, *dep;
  unsigned int n, hi = 0;
  char path[_POSIX_PATH_MAX];
  char tmp[16];

  if (safe_fclose (&msg->fp) != 0)
    return -1;
  
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
    mh_sequences_add_one (ctx, hi, !msg->flags.read, msg->flags.flagged, msg->flags.replied);
  return 0;
}

int mh_commit_message (CONTEXT *ctx, MESSAGE *msg, HEADER *hdr)
{
  return _mh_commit_message (ctx, msg, hdr, 1);
}


/* Sync a message in an MH folder.
 * 
 * This code is also used for attachment deletion in maildir
 * folders.
 */

static int mh_rewrite_message (CONTEXT *ctx, int msgno)
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
  long old_hdr_lines   = h->lines;

  if ((dest = mx_open_new_message (ctx, h, 0)) == NULL)
    return -1;

  if ((rc = mutt_copy_message (dest->fp, ctx, h, 
	       M_CM_UPDATE, CH_UPDATE | CH_UPDATE_LEN)) == 0)
  {
    snprintf (oldpath, _POSIX_PATH_MAX, "%s/%s", ctx->path, h->path);
    strfcpy  (partpath, h->path, _POSIX_PATH_MAX);

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
    h->lines           = old_hdr_lines;
  }

  mutt_free_body (&h->content->parts);
  return rc;
}

static int mh_sync_message (CONTEXT *ctx, int msgno)
{
  HEADER *h = ctx->hdrs[msgno];

  if (h->attach_del)
    if (mh_rewrite_message (ctx, msgno) != 0)
      return -1;
  
  return 0;
}

static int maildir_sync_message (CONTEXT *ctx, int msgno)
{
  HEADER *h = ctx->hdrs[msgno];

  if (h->attach_del)
  {
    /* when doing attachment deletion, fall back to the MH case. */
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
      dprint (1, (debugfile, "maildir_sync_message: %s: unable to find subdir!\n",
		  h->path));
      return (-1);
    }
    p++;
    strfcpy (newpath, p, sizeof (newpath));
    
    /* kill the previous flags */
    if ((p = strchr (newpath, ':')) != NULL) *p = 0;
    
    maildir_flags (suffix, sizeof (suffix), h);
    
    snprintf (partpath, sizeof (partpath), "%s/%s%s",
	      (h->read || h->old) ? "cur" : "new",
	      newpath, suffix);
    snprintf (fullpath, sizeof (fullpath), "%s/%s", ctx->path, partpath);
    snprintf (oldpath, sizeof (oldpath), "%s/%s", ctx->path, h->path);
    
    if (mutt_strcmp (fullpath, oldpath) == 0)
    {
      /* message hasn't really changed */
      return 0;
    }
    
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

  if ((i = mh_check_mailbox(ctx, index_hint)) != 0)
    return i;

  for (i = 0; i < ctx->msgcount; i++)
  {
    if (ctx->hdrs[i]->deleted && (ctx->magic != M_MAILDIR || !option(OPTMAILDIRTRASH)))
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
      {
	if (maildir_sync_message (ctx, i) == -1)
	  return -1;
      }
      else
      {
	if (mh_sync_message (ctx, i) == -1)
	  return -1;
      }
    }
  }

  mh_update_sequences (ctx);
  
  /* XXX race condition? */

  maildir_update_mtime(ctx);

  /* adjust indices */

  if (ctx->deleted)
  {
    for (i = 0, j = 0; i < ctx->msgcount; i++)
    {
      if (!ctx->hdrs[i]->deleted)
	ctx->hdrs[i]->index = j++;
    }
  }

  return 0;
}

static char *maildir_canon_filename(char *dest, char *src, size_t l)
{
  char *t, *u;
  
  if((t = strrchr(src, '/')))
    src = t + 1;

  strfcpy(dest, src, l);
  if((u = strrchr(dest, ':')))
    *u = '\0';

  return dest;
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

int mh_check_mailbox(CONTEXT *ctx, int *index_hint)
{
  char buf[_POSIX_PATH_MAX], b1[LONG_STRING], b2[LONG_STRING];
  struct stat st, st_cur;
  short modified = 0, have_new = 0, occult = 0;
  struct maildir *md, *p;
  struct maildir **last;
  HASH *fnames;
  int i, j, deleted;
  
  if(!option (OPTCHECKNEW))
    return 0;
  
  if(ctx->magic == M_MH)
  {
    strfcpy(buf, ctx->path, sizeof(buf));
    if(stat(buf, &st) == -1)
      return -1;
    
    snprintf (buf, sizeof (buf), "%s/.mh_sequences", ctx->path);
    if (stat (buf, &st_cur) == -1)
      modified = 1;
    
  }
  else if(ctx->magic == M_MAILDIR)
  {
    snprintf(buf, sizeof(buf), "%s/new", ctx->path);
    if(stat(buf, &st) == -1)
      return -1;
    
    snprintf(buf, sizeof(buf), "%s/cur", ctx->path);
    if(stat(buf, &st_cur) == -1)			/* XXX - name is bad. */
      modified = 1;

  }
  
  if(!modified && ctx->magic == M_MAILDIR && st_cur.st_mtime > ctx->mtime_cur)
    modified = 1;
  
  if(!modified && ctx->magic == M_MH && (st.st_mtime > ctx->mtime || st_cur.st_mtime > ctx->mtime_cur))
    modified = 1;
  
  if(modified || (ctx->magic == M_MAILDIR && st.st_mtime > ctx->mtime))
    have_new = 1;
  
  if(!modified && !have_new)
    return 0;

  ctx->mtime_cur = st_cur.st_mtime;
  ctx->mtime = st.st_mtime;

#if 0
  if(Sort != SORT_ORDER)
  {
    short old_sort;
    
    old_sort = Sort;
    Sort = SORT_ORDER;
    mutt_sort_headers(ctx, 1);
    Sort = old_sort;
  }
#endif

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
  {
    struct mh_sequences mhs;
    memset (&mhs, 0, sizeof (mhs));
    maildir_parse_dir (ctx, &last, NULL, NULL);
    mh_read_sequences (&mhs, ctx->path);
    mh_update_maildir (md, &mhs);
    mhs_free_sequences (&mhs);
  }

  /* check for modifications and adjust flags */

  fnames = hash_create (1031);
  
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

  deleted = 0;
  
  for(i = 0; i < ctx->msgcount; i++)
  {
    ctx->hdrs[i]->active = 0;

    if(ctx->magic == M_MAILDIR)
      maildir_canon_filename(b1, ctx->hdrs[i]->path, sizeof(b1));
    else
      strfcpy(b1, ctx->hdrs[i]->path, sizeof(b1));

    dprint(2, (debugfile, "%s:%d: mh_check_mailbox(): Looking for %s.\n", __FILE__, __LINE__, b1));
    
    if((p = hash_find(fnames, b1)) && p->h &&
       mbox_strict_cmp_headers(ctx->hdrs[i], p->h))
    {
      /* found the right message */

      dprint(2, (debugfile, "%s:%d: Found.  Flags before: %s%s%s%s%s\n", __FILE__, __LINE__,
		 ctx->hdrs[i]->flagged ? "f" : "",
		 ctx->hdrs[i]->deleted ? "D" : "",
		 ctx->hdrs[i]->replied ? "r" : "",
		 ctx->hdrs[i]->old     ? "O" : "",
		 ctx->hdrs[i]->read    ? "R" : ""));

      if(mutt_strcmp(ctx->hdrs[i]->path, p->h->path))
	mutt_str_replace (&ctx->hdrs[i]->path, p->h->path);

      if(modified)
      {
	if(!ctx->hdrs[i]->changed)
	{
	  mutt_set_flag (ctx, ctx->hdrs[i], M_FLAG, p->h->flagged);
	  mutt_set_flag (ctx, ctx->hdrs[i], M_REPLIED, p->h->replied);
	  mutt_set_flag (ctx, ctx->hdrs[i], M_READ, p->h->read);
	}

	mutt_set_flag(ctx, ctx->hdrs[i], M_OLD, p->h->old);
      }

      ctx->hdrs[i]->active = 1;

      dprint(2, (debugfile, "%s:%d:         Flags after: %s%s%s%s%s\n", __FILE__, __LINE__,
		 ctx->hdrs[i]->flagged ? "f" : "",
		 ctx->hdrs[i]->deleted ? "D" : "",
		 ctx->hdrs[i]->replied ? "r" : "",
		 ctx->hdrs[i]->old     ? "O" : "",
		 ctx->hdrs[i]->read    ? "R" : ""));

      mutt_free_header(&p->h);
    }
    else if (ctx->magic == M_MAILDIR && !modified && !strncmp("cur/", ctx->hdrs[i]->path, 4))
    {
      /* If the cur/ part wasn't externally modified for a maildir
       * type folder, assume the message is still active. Actually,
       * we simply don't know.
       */

      ctx->hdrs[i]->active = 1;
    }
    else if (modified || (ctx->magic == M_MAILDIR && !strncmp("new/", ctx->hdrs[i]->path, 4)))
    {
      
      /* Mailbox was modified, or a new message vanished. */

      /* Note: This code will _not_ apply for a new message which
       * is just moved to cur/, as this would modify cur's time
       * stamp and lead to modified == 1.  Thus, we'd have parsed
       * the complete folder above, and the message would have
       * been found in the look-up table.
       */
      
      dprint(2, (debugfile, "%s:%d: Not found.  Flags were: %s%s%s%s%s\n", __FILE__, __LINE__,
		 ctx->hdrs[i]->flagged ? "f" : "",
		 ctx->hdrs[i]->deleted ? "D" : "",
		 ctx->hdrs[i]->replied ? "r" : "",
		 ctx->hdrs[i]->old     ? "O" : "",
		 ctx->hdrs[i]->read    ? "R" : ""));
      
      occult = 1;

    }
  }

  /* destroy the file name hash */

  hash_destroy(&fnames, NULL);

  /* If we didn't just get new mail, update the tables. */
  
  if(modified || occult)
  {
    short old_sort;
    int old_count;

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
    mx_update_tables(ctx, 0);
  }

  /* Incorporate new messages */

  maildir_move_to_context(ctx, &md);

  return (modified || occult) ? M_REOPENED : have_new ? M_NEW_MAIL : 0;
}
