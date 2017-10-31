/**
 * @file
 * Maildir/MH local mailbox type
 *
 * @authors
 * Copyright (C) 1996-2002,2007,2009 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2005 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2010,2013 Michael R. Elkins <me@mutt.org>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * This file contains routines specific to MH and ``maildir'' style
 * mailboxes.
 */

#include "config.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include "lib/lib.h"
#include "mutt.h"
#include "body.h"
#include "buffy.h"
#include "context.h"
#include "copy.h"
#include "envelope.h"
#include "globals.h"
#include "header.h"
#include "mailbox.h"
#include "mutt_curses.h"
#include "mx.h"
#include "options.h"
#include "protos.h"
#include "sort.h"
#include "thread.h"
#ifdef USE_NOTMUCH
#include "mutt_notmuch.h"
#endif
#ifdef USE_HCACHE
#include "hcache/hcache.h"
#endif

#define INS_SORT_THRESHOLD 6

/**
 * struct Maildir - A Maildir mailbox
 */
struct Maildir
{
  struct Header *h;
  char *canon_fname;
  unsigned header_parsed : 1;
  ino_t inode;
  struct Maildir *next;
};

/**
 * struct MhSequences - Set of MH sequence numbers
 */
struct MhSequences
{
  int max;
  short *flags;
};

/**
 * struct MhData - MH-specific mailbox data
 */
struct MhData
{
  time_t mtime_cur;
  mode_t mh_umask;
};

/* mh_sequences support */

#define MH_SEQ_UNSEEN (1 << 0)
#define MH_SEQ_REPLIED (1 << 1)
#define MH_SEQ_FLAGGED (1 << 2)

static inline struct MhData *mh_data(struct Context *ctx)
{
  return (struct MhData *) ctx->data;
}

static void mhs_alloc(struct MhSequences *mhs, int i)
{
  int j;
  int newmax;

  if (i > mhs->max || !mhs->flags)
  {
    newmax = i + 128;
    j = mhs->flags ? mhs->max + 1 : 0;
    safe_realloc(&mhs->flags, sizeof(mhs->flags[0]) * (newmax + 1));
    while (j <= newmax)
      mhs->flags[j++] = 0;

    mhs->max = newmax;
  }
}

static void mhs_free_sequences(struct MhSequences *mhs)
{
  FREE(&mhs->flags);
}

static short mhs_check(struct MhSequences *mhs, int i)
{
  if (!mhs->flags || i > mhs->max)
    return 0;
  else
    return mhs->flags[i];
}

static short mhs_set(struct MhSequences *mhs, int i, short f)
{
  mhs_alloc(mhs, i);
  mhs->flags[i] |= f;
  return mhs->flags[i];
}

static int mh_read_token(char *t, int *first, int *last)
{
  char *p = NULL;
  if ((p = strchr(t, '-')))
  {
    *p++ = '\0';
    if (mutt_atoi(t, first) < 0 || mutt_atoi(p, last) < 0)
      return -1;
  }
  else
  {
    if (mutt_atoi(t, first) < 0)
      return -1;
    *last = *first;
  }
  return 0;
}

static int mh_read_sequences(struct MhSequences *mhs, const char *path)
{
  FILE *fp = NULL;
  int line = 1;
  char *buff = NULL;
  char *t = NULL;
  size_t sz = 0;

  short f;
  int first, last, rc = 0;

  char pathname[_POSIX_PATH_MAX];
  snprintf(pathname, sizeof(pathname), "%s/.mh_sequences", path);

  fp = fopen(pathname, "r");
  if (!fp)
    return 0; /* yes, ask callers to silently ignore the error */

  while ((buff = mutt_read_line(buff, &sz, fp, &line, 0)))
  {
    t = strtok(buff, " \t:");
    if (!t)
      continue;

    if (mutt_strcmp(t, MhSeqUnseen) == 0)
      f = MH_SEQ_UNSEEN;
    else if (mutt_strcmp(t, MhSeqFlagged) == 0)
      f = MH_SEQ_FLAGGED;
    else if (mutt_strcmp(t, MhSeqReplied) == 0)
      f = MH_SEQ_REPLIED;
    else /* unknown sequence */
      continue;

    while ((t = strtok(NULL, " \t:")))
    {
      if (mh_read_token(t, &first, &last) < 0)
      {
        mhs_free_sequences(mhs);
        rc = -1;
        goto out;
      }
      for (; first <= last; first++)
        mhs_set(mhs, first, f);
    }
  }

  rc = 0;

out:
  FREE(&buff);
  safe_fclose(&fp);
  return rc;
}

static inline mode_t mh_umask(struct Context *ctx)
{
  struct stat st;
  struct MhData *data = mh_data(ctx);

  if (data && data->mh_umask)
    return data->mh_umask;

  if (stat(ctx->path, &st))
  {
    mutt_debug(1, "stat failed on %s\n", ctx->path);
    return 077;
  }

  return 0777 & ~st.st_mode;
}

/**
 * mh_sequences_changed - Has the mailbox changed
 * @param b Mailbox
 * @retval 1 mh_sequences last modification time is more recent than the last visit to this mailbox
 * @retval 0 modification time is older
 * @retval -1 Error
 */
static int mh_sequences_changed(struct Buffy *b)
{
  char path[_POSIX_PATH_MAX];
  struct stat sb;

  if ((snprintf(path, sizeof(path), "%s/.mh_sequences", b->path) < sizeof(path)) &&
      (stat(path, &sb) == 0))
    return (sb.st_mtime > b->last_visited);
  return -1;
}

/**
 * mh_already_notified - Has the message changed
 * @param b     Mailbox
 * @param msgno Message number
 * @retval 1 Modification time on the message file is older than the last visit to this mailbox
 * @retval 0 Modification time on the message file is newer
 * @retval -1 Error
 */
static int mh_already_notified(struct Buffy *b, int msgno)
{
  char path[_POSIX_PATH_MAX];
  struct stat sb;

  if ((snprintf(path, sizeof(path), "%s/%d", b->path, msgno) < sizeof(path)) &&
      (stat(path, &sb) == 0))
    return (sb.st_mtime <= b->last_visited);
  return -1;
}

/**
 * mh_valid_message - Is this a valid MH message filename
 * @param s Pathname to examine
 * @retval true name is valid
 * @retval false name is invalid
 *
 * Ignore the garbage files.  A valid MH message consists of only
 * digits.  Deleted message get moved to a filename with a comma before
 * it.
 */
static bool mh_valid_message(const char *s)
{
  for (; *s; s++)
  {
    if (!isdigit((unsigned char) *s))
      return false;
  }
  return true;
}

/**
 * mh_buffy - Check for new mail for a mh mailbox
 * @param mailbox     Mailbox to check
 * @param check_stats Also count total, new, and flagged messages
 * @retval true if the mailbox has new mail
 */
bool mh_buffy(struct Buffy *mailbox, bool check_stats)
{
  struct MhSequences mhs;
  bool check_new = true;
  bool rc = false;
  DIR *dirp = NULL;
  struct dirent *de = NULL;

  /* when $mail_check_recent is set and the .mh_sequences file hasn't changed
   * since the last mailbox visit, there is no "new mail" */
  if (option(OPT_MAIL_CHECK_RECENT) && mh_sequences_changed(mailbox) <= 0)
  {
    rc = false;
    check_new = false;
  }

  if (!(check_new || check_stats))
    return rc;

  memset(&mhs, 0, sizeof(mhs));
  if (mh_read_sequences(&mhs, mailbox->path) < 0)
    return false;

  if (check_stats)
  {
    mailbox->msg_count = 0;
    mailbox->msg_unread = 0;
    mailbox->msg_flagged = 0;
  }

  for (int i = mhs.max; i > 0; i--)
  {
    if (check_stats && (mhs_check(&mhs, i) & MH_SEQ_FLAGGED))
      mailbox->msg_flagged++;
    if (mhs_check(&mhs, i) & MH_SEQ_UNSEEN)
    {
      if (check_stats)
        mailbox->msg_unread++;
      if (check_new)
      {
        /* if the first unseen message we encounter was in the mailbox during the
           last visit, don't notify about it */
        if (!option(OPT_MAIL_CHECK_RECENT) || mh_already_notified(mailbox, i) == 0)
        {
          mailbox->new = true;
          rc = true;
        }
        /* Because we are traversing from high to low, we can stop
         * checking for new mail after the first unseen message.
         * Whether it resulted in "new mail" or not. */
        check_new = false;
        if (!check_stats)
          break;
      }
    }
  }
  mhs_free_sequences(&mhs);

  if (check_stats)
  {
    dirp = opendir(mailbox->path);
    if (dirp)
    {
      while ((de = readdir(dirp)) != NULL)
      {
        if (*de->d_name == '.')
          continue;
        if (mh_valid_message(de->d_name))
          mailbox->msg_count++;
      }
      closedir(dirp);
    }
  }

  return rc;
}

static int mh_mkstemp(struct Context *dest, FILE **fp, char **tgt)
{
  int fd;
  char path[_POSIX_PATH_MAX];
  mode_t omask;

  omask = umask(mh_umask(dest));
  while (true)
  {
    snprintf(path, _POSIX_PATH_MAX, "%s/.mutt-%s-%d-%" PRIu64, dest->path,
             NONULL(ShortHostname), (int) getpid(), mutt_rand64());
    fd = open(path, O_WRONLY | O_EXCL | O_CREAT, 0666);
    if (fd == -1)
    {
      if (errno != EEXIST)
      {
        mutt_perror(path);
        umask(omask);
        return -1;
      }
    }
    else
    {
      *tgt = safe_strdup(path);
      break;
    }
  }
  umask(omask);

  *fp = fdopen(fd, "w");
  if (!*fp)
  {
    FREE(tgt);
    close(fd);
    unlink(path);
    return -1;
  }

  return 0;
}

static void mhs_write_one_sequence(FILE *fp, struct MhSequences *mhs, short f, const char *tag)
{
  int first, last;
  fprintf(fp, "%s:", tag);

  first = -1;
  last = -1;

  for (int i = 0; i <= mhs->max; i++)
  {
    if ((mhs_check(mhs, i) & f))
    {
      if (first < 0)
        first = i;
      else
        last = i;
    }
    else if (first >= 0)
    {
      if (last < 0)
        fprintf(fp, " %d", first);
      else
        fprintf(fp, " %d-%d", first, last);

      first = -1;
      last = -1;
    }
  }

  if (first >= 0)
  {
    if (last < 0)
      fprintf(fp, " %d", first);
    else
      fprintf(fp, " %d-%d", first, last);
  }

  fputc('\n', fp);
}

/**
 * mh_update_sequences - Update sequence numbers
 *
 * XXX we don't currently remove deleted messages from sequences we don't know.
 * Should we?
 */
static void mh_update_sequences(struct Context *ctx)
{
  FILE *ofp = NULL, *nfp = NULL;

  char sequences[_POSIX_PATH_MAX];
  char *tmpfname = NULL;
  char *buff = NULL;
  char *p = NULL;
  size_t s;
  int l = 0;
  int i;

  int unseen = 0;
  int flagged = 0;
  int replied = 0;

  char seq_unseen[STRING];
  char seq_replied[STRING];
  char seq_flagged[STRING];

  struct MhSequences mhs;
  memset(&mhs, 0, sizeof(mhs));

  snprintf(seq_unseen, sizeof(seq_unseen), "%s:", NONULL(MhSeqUnseen));
  snprintf(seq_replied, sizeof(seq_replied), "%s:", NONULL(MhSeqReplied));
  snprintf(seq_flagged, sizeof(seq_flagged), "%s:", NONULL(MhSeqFlagged));

  if (mh_mkstemp(ctx, &nfp, &tmpfname) != 0)
  {
    /* error message? */
    return;
  }

  snprintf(sequences, sizeof(sequences), "%s/.mh_sequences", ctx->path);

  /* first, copy unknown sequences */
  if ((ofp = fopen(sequences, "r")))
  {
    while ((buff = mutt_read_line(buff, &s, ofp, &l, 0)))
    {
      if (mutt_strncmp(buff, seq_unseen, mutt_strlen(seq_unseen)) == 0)
        continue;
      if (mutt_strncmp(buff, seq_flagged, mutt_strlen(seq_flagged)) == 0)
        continue;
      if (mutt_strncmp(buff, seq_replied, mutt_strlen(seq_replied)) == 0)
        continue;

      fprintf(nfp, "%s\n", buff);
    }
  }
  safe_fclose(&ofp);

  /* now, update our unseen, flagged, and replied sequences */
  for (l = 0; l < ctx->msgcount; l++)
  {
    if (ctx->hdrs[l]->deleted)
      continue;

    if ((p = strrchr(ctx->hdrs[l]->path, '/')))
      p++;
    else
      p = ctx->hdrs[l]->path;

    if (mutt_atoi(p, &i) < 0)
      continue;

    if (!ctx->hdrs[l]->read)
    {
      mhs_set(&mhs, i, MH_SEQ_UNSEEN);
      unseen++;
    }
    if (ctx->hdrs[l]->flagged)
    {
      mhs_set(&mhs, i, MH_SEQ_FLAGGED);
      flagged++;
    }
    if (ctx->hdrs[l]->replied)
    {
      mhs_set(&mhs, i, MH_SEQ_REPLIED);
      replied++;
    }
  }

  /* write out the new sequences */
  if (unseen)
    mhs_write_one_sequence(nfp, &mhs, MH_SEQ_UNSEEN, NONULL(MhSeqUnseen));
  if (flagged)
    mhs_write_one_sequence(nfp, &mhs, MH_SEQ_FLAGGED, NONULL(MhSeqFlagged));
  if (replied)
    mhs_write_one_sequence(nfp, &mhs, MH_SEQ_REPLIED, NONULL(MhSeqReplied));

  mhs_free_sequences(&mhs);

  /* try to commit the changes - no guarantee here */
  safe_fclose(&nfp);

  unlink(sequences);
  if (safe_rename(tmpfname, sequences) != 0)
  {
    /* report an error? */
    unlink(tmpfname);
  }

  FREE(&tmpfname);
}

static void mh_sequences_add_one(struct Context *ctx, int n, short unseen,
                                 short flagged, short replied)
{
  short unseen_done = 0;
  short flagged_done = 0;
  short replied_done = 0;

  FILE *ofp = NULL, *nfp = NULL;

  char *tmpfname = NULL;
  char sequences[_POSIX_PATH_MAX];

  char seq_unseen[STRING];
  char seq_replied[STRING];
  char seq_flagged[STRING];

  char *buff = NULL;
  int line = 0;
  size_t sz;

  if (mh_mkstemp(ctx, &nfp, &tmpfname) == -1)
    return;

  snprintf(seq_unseen, sizeof(seq_unseen), "%s:", NONULL(MhSeqUnseen));
  snprintf(seq_replied, sizeof(seq_replied), "%s:", NONULL(MhSeqReplied));
  snprintf(seq_flagged, sizeof(seq_flagged), "%s:", NONULL(MhSeqFlagged));

  snprintf(sequences, sizeof(sequences), "%s/.mh_sequences", ctx->path);
  if ((ofp = fopen(sequences, "r")))
  {
    while ((buff = mutt_read_line(buff, &sz, ofp, &line, 0)))
    {
      if (unseen && (strncmp(buff, seq_unseen, mutt_strlen(seq_unseen)) == 0))
      {
        fprintf(nfp, "%s %d\n", buff, n);
        unseen_done = 1;
      }
      else if (flagged && (strncmp(buff, seq_flagged, mutt_strlen(seq_flagged)) == 0))
      {
        fprintf(nfp, "%s %d\n", buff, n);
        flagged_done = 1;
      }
      else if (replied && (strncmp(buff, seq_replied, mutt_strlen(seq_replied)) == 0))
      {
        fprintf(nfp, "%s %d\n", buff, n);
        replied_done = 1;
      }
      else
        fprintf(nfp, "%s\n", buff);
    }
  }
  safe_fclose(&ofp);
  FREE(&buff);

  if (!unseen_done && unseen)
    fprintf(nfp, "%s: %d\n", NONULL(MhSeqUnseen), n);
  if (!flagged_done && flagged)
    fprintf(nfp, "%s: %d\n", NONULL(MhSeqFlagged), n);
  if (!replied_done && replied)
    fprintf(nfp, "%s: %d\n", NONULL(MhSeqReplied), n);

  safe_fclose(&nfp);

  unlink(sequences);
  if (safe_rename(tmpfname, sequences) != 0)
    unlink(tmpfname);

  FREE(&tmpfname);
}

static void mh_update_maildir(struct Maildir *md, struct MhSequences *mhs)
{
  int i;
  short f;
  char *p = NULL;

  for (; md; md = md->next)
  {
    if ((p = strrchr(md->h->path, '/')))
      p++;
    else
      p = md->h->path;

    if (mutt_atoi(p, &i) < 0)
      continue;
    f = mhs_check(mhs, i);

    md->h->read = (f & MH_SEQ_UNSEEN) ? false : true;
    md->h->flagged = (f & MH_SEQ_FLAGGED) ? true : false;
    md->h->replied = (f & MH_SEQ_REPLIED) ? true : false;
  }
}

/**
 * maildir_free_entry - Free a maildir object
 */
static void maildir_free_entry(struct Maildir **md)
{
  if (!md || !*md)
    return;

  FREE(&(*md)->canon_fname);
  if ((*md)->h)
    mutt_free_header(&(*md)->h);

  FREE(md);
}

static void maildir_free_maildir(struct Maildir **md)
{
  struct Maildir *p = NULL, *q = NULL;

  if (!md || !*md)
    return;

  for (p = *md; p; p = q)
  {
    q = p->next;
    maildir_free_entry(&p);
  }
}

void maildir_parse_flags(struct Header *h, const char *path)
{
  char *p = NULL, *q = NULL;

  h->flagged = false;
  h->read = false;
  h->replied = false;

  if ((p = strrchr(path, ':')) != NULL && (mutt_strncmp(p + 1, "2,", 2) == 0))
  {
    p += 3;

    mutt_str_replace(&h->maildir_flags, p);
    q = h->maildir_flags;

    while (*p)
    {
      switch (*p)
      {
        case 'F':

          h->flagged = true;
          break;

        case 'S': /* seen */

          h->read = true;
          break;

        case 'R': /* replied */

          h->replied = true;
          break;

        case 'T': /* trashed */
          if (!h->flagged || !option(OPT_FLAG_SAFE))
          {
            h->trash = true;
            h->deleted = true;
          }
          break;

        default:
          *q++ = *p;
          break;
      }
      p++;
    }
  }

  if (q == h->maildir_flags)
    FREE(&h->maildir_flags);
  else if (q)
    *q = '\0';
}

static void maildir_update_mtime(struct Context *ctx)
{
  char buf[_POSIX_PATH_MAX];
  struct stat st;
  struct MhData *data = mh_data(ctx);

  if (ctx->magic == MUTT_MAILDIR)
  {
    snprintf(buf, sizeof(buf), "%s/%s", ctx->path, "cur");
    if (stat(buf, &st) == 0)
      data->mtime_cur = st.st_mtime;
    snprintf(buf, sizeof(buf), "%s/%s", ctx->path, "new");
  }
  else
  {
    snprintf(buf, sizeof(buf), "%s/.mh_sequences", ctx->path);
    if (stat(buf, &st) == 0)
      data->mtime_cur = st.st_mtime;

    strfcpy(buf, ctx->path, sizeof(buf));
  }

  if (stat(buf, &st) == 0)
    ctx->mtime = st.st_mtime;
}

/**
 * maildir_parse_stream - Parse a Maildir message
 *
 * Actually parse a maildir message.  This may also be used to fill
 * out a fake header structure generated by lazy maildir parsing.
 */
struct Header *maildir_parse_stream(int magic, FILE *f, const char *fname,
                                    bool is_old, struct Header *_h)
{
  struct Header *h = _h;
  struct stat st;

  if (!h)
    h = mutt_new_header();
  h->env = mutt_read_rfc822_header(f, h, 0, 0);

  fstat(fileno(f), &st);

  if (!h->received)
    h->received = h->date_sent;

  /* always update the length since we have fresh information available. */
  h->content->length = st.st_size - h->content->offset;

  h->index = -1;

  if (magic == MUTT_MAILDIR)
  {
    /*
     * maildir stores its flags in the filename, so ignore the
     * flags in the header of the message
     */

    h->old = is_old;
    maildir_parse_flags(h, fname);
  }
  return h;
}

/**
 * maildir_parse_message - Actually parse a maildir message
 *
 * This may also be used to fill out a fake header structure generated by lazy
 * maildir parsing.
 */
struct Header *maildir_parse_message(int magic, const char *fname, bool is_old,
                                     struct Header *h)
{
  FILE *f = NULL;

  f = fopen(fname, "r");
  if (f)
  {
    h = maildir_parse_stream(magic, f, fname, is_old, h);
    safe_fclose(&f);
    return h;
  }
  return NULL;
}

static int maildir_parse_dir(struct Context *ctx, struct Maildir ***last,
                             const char *subdir, int *count, struct Progress *progress)
{
  DIR *dirp = NULL;
  struct dirent *de = NULL;
  char buf[_POSIX_PATH_MAX];
  int is_old = 0;
  struct Maildir *entry = NULL;
  struct Header *h = NULL;

  if (subdir)
  {
    snprintf(buf, sizeof(buf), "%s/%s", ctx->path, subdir);
    is_old = option(OPT_MARK_OLD) ? (mutt_strcmp("cur", subdir) == 0) : false;
  }
  else
    strfcpy(buf, ctx->path, sizeof(buf));

  dirp = opendir(buf);
  if (!dirp)
    return -1;

  while (((de = readdir(dirp)) != NULL) && (SigInt != 1))
  {
    if ((ctx->magic == MUTT_MH && !mh_valid_message(de->d_name)) ||
        (ctx->magic == MUTT_MAILDIR && *de->d_name == '.'))
      continue;

    /* FOO - really ignore the return value? */
    mutt_debug(2, "%s:%d: queueing %s\n", __FILE__, __LINE__, de->d_name);

    h = mutt_new_header();
    h->old = is_old;
    if (ctx->magic == MUTT_MAILDIR)
      maildir_parse_flags(h, de->d_name);

    if (count)
    {
      (*count)++;
      if (!ctx->quiet && progress)
        mutt_progress_update(progress, *count, -1);
    }

    if (subdir)
    {
      char tmp[LONG_STRING];
      snprintf(tmp, sizeof(tmp), "%s/%s", subdir, de->d_name);
      h->path = safe_strdup(tmp);
    }
    else
      h->path = safe_strdup(de->d_name);

    entry = safe_calloc(1, sizeof(struct Maildir));
    entry->h = h;
    entry->inode = de->d_ino;
    **last = entry;
    *last = &entry->next;
  }

  closedir(dirp);

  if (SigInt == 1)
  {
    SigInt = 0;
    return -2; /* action aborted */
  }

  return 0;
}

static bool maildir_add_to_context(struct Context *ctx, struct Maildir *md)
{
  int oldmsgcount = ctx->msgcount;

  while (md)
  {
    mutt_debug(2, "%s:%d maildir_add_to_context(): Considering %s\n", __FILE__,
               __LINE__, NONULL(md->canon_fname));

    if (md->h)
    {
      mutt_debug(2, "%s:%d Adding header structure. Flags: %s%s%s%s%s\n",
                 __FILE__, __LINE__, md->h->flagged ? "f" : "",
                 md->h->deleted ? "D" : "", md->h->replied ? "r" : "",
                 md->h->old ? "O" : "", md->h->read ? "R" : "");
      if (ctx->msgcount == ctx->hdrmax)
        mx_alloc_memory(ctx);

      ctx->hdrs[ctx->msgcount] = md->h;
      ctx->hdrs[ctx->msgcount]->index = ctx->msgcount;
      ctx->size += md->h->content->length + md->h->content->offset -
                   md->h->content->hdr_offset;

      md->h = NULL;
      ctx->msgcount++;
    }
    md = md->next;
  }

  if (ctx->msgcount > oldmsgcount)
  {
    mx_update_context(ctx, ctx->msgcount - oldmsgcount);
    return true;
  }
  return false;
}

static int maildir_move_to_context(struct Context *ctx, struct Maildir **md)
{
  int r;
  r = maildir_add_to_context(ctx, *md);
  maildir_free_maildir(md);
  return r;
}

#ifdef USE_HCACHE
static size_t maildir_hcache_keylen(const char *fn)
{
  const char *p = strrchr(fn, ':');
  return p ? (size_t)(p - fn) : mutt_strlen(fn);
}
#endif

static int md_cmp_inode(struct Maildir *a, struct Maildir *b)
{
  return a->inode - b->inode;
}

static int md_cmp_path(struct Maildir *a, struct Maildir *b)
{
  return strcmp(a->h->path, b->h->path);
}

/**
 * maildir_merge_lists - Merge two maildir lists according to the inode numbers
 */
static struct Maildir *maildir_merge_lists(struct Maildir *left, struct Maildir *right,
                                           int (*cmp)(struct Maildir *, struct Maildir *))
{
  struct Maildir *head = NULL;
  struct Maildir *tail = NULL;

  if (left && right)
  {
    if (cmp(left, right) < 0)
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
    if (cmp(left, right) < 0)
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

static struct Maildir *maildir_ins_sort(struct Maildir *list,
                                        int (*cmp)(struct Maildir *, struct Maildir *))
{
  struct Maildir *tmp = NULL, *last = NULL, *ret = NULL, *back = NULL;

  ret = list;
  list = list->next;
  ret->next = NULL;

  while (list)
  {
    last = NULL;
    back = list->next;
    for (tmp = ret; tmp && cmp(tmp, list) <= 0; tmp = tmp->next)
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

/**
 * maildir_sort - Sort maildir list according to inode
 */
static struct Maildir *maildir_sort(struct Maildir *list, size_t len,
                                    int (*cmp)(struct Maildir *, struct Maildir *))
{
  struct Maildir *left = list;
  struct Maildir *right = list;
  size_t c = 0;

  if (!list || !list->next)
  {
    return list;
  }

  if (len != (size_t)(-1) && len <= INS_SORT_THRESHOLD)
    return maildir_ins_sort(list, cmp);

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

  left = maildir_sort(left, c, cmp);
  right = maildir_sort(right, c, cmp);
  return maildir_merge_lists(left, right, cmp);
}

/**
 * mh_sort_natural - Sorts mailbox into its natural order
 *
 * Currently only defined for MH where files are numbered.
 */
static void mh_sort_natural(struct Context *ctx, struct Maildir **md)
{
  if (!ctx || !md || !*md || ctx->magic != MUTT_MH || Sort != SORT_ORDER)
    return;
  mutt_debug(4, "maildir: sorting %s into natural order\n", ctx->path);
  *md = maildir_sort(*md, (size_t) -1, md_cmp_path);
}

static struct Maildir *skip_duplicates(struct Maildir *p, struct Maildir **last)
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
  while (!p->h || p->header_parsed)
  {
    *last = p;
    p = p->next;
  }
  return p;
}

/**
 * maildir_delayed_parsing - This function does the second parsing pass
 */
static void maildir_delayed_parsing(struct Context *ctx, struct Maildir **md,
                                    struct Progress *progress)
{
  struct Maildir *p, *last = NULL;
  char fn[_POSIX_PATH_MAX];
  int count;
  int sort = 0;
#ifdef USE_HCACHE
  header_cache_t *hc = NULL;
  void *data = NULL;
  const char *key = NULL;
  size_t keylen;
  struct timeval *when = NULL;
  struct stat lastchanged;
  int ret;
#endif

#ifdef USE_HCACHE
  hc = mutt_hcache_open(HeaderCache, ctx->path, NULL);
#endif

  for (p = *md, count = 0; p; p = p->next, count++)
  {
    if (!(p && p->h && !p->header_parsed))
    {
      last = p;
      continue;
    }

    if (!ctx->quiet && progress)
      mutt_progress_update(progress, count, -1);

    if (!sort)
    {
      mutt_debug(4, "maildir: need to sort %s by inode\n", ctx->path);
      p = maildir_sort(p, (size_t) -1, md_cmp_inode);
      if (!last)
        *md = p;
      else
        last->next = p;
      sort = 1;
      p = skip_duplicates(p, &last);
      snprintf(fn, sizeof(fn), "%s/%s", ctx->path, p->h->path);
    }

    snprintf(fn, sizeof(fn), "%s/%s", ctx->path, p->h->path);

#ifdef USE_HCACHE
    if (option(OPT_MAILDIR_HEADER_CACHE_VERIFY))
    {
      ret = stat(fn, &lastchanged);
    }
    else
    {
      lastchanged.st_mtime = 0;
      ret = 0;
    }

    if (ctx->magic == MUTT_MH)
    {
      key = p->h->path;
      keylen = strlen(key);
    }
    else
    {
      key = p->h->path + 3;
      keylen = maildir_hcache_keylen(key);
    }
    data = mutt_hcache_fetch(hc, key, keylen);
    when = (struct timeval *) data;

    if (data != NULL && !ret && lastchanged.st_mtime <= when->tv_sec)
    {
      struct Header *h = mutt_hcache_restore((unsigned char *) data);
      h->old = p->h->old;
      h->path = safe_strdup(p->h->path);
      mutt_free_header(&p->h);
      p->h = h;
      if (ctx->magic == MUTT_MAILDIR)
        maildir_parse_flags(p->h, fn);
    }
    else
    {
#endif /* USE_HCACHE */

      if (maildir_parse_message(ctx->magic, fn, p->h->old, p->h))
      {
        p->header_parsed = 1;
#ifdef USE_HCACHE
        if (ctx->magic == MUTT_MH)
        {
          key = p->h->path;
          keylen = strlen(key);
        }
        else
        {
          key = p->h->path + 3;
          keylen = maildir_hcache_keylen(key);
        }
        mutt_hcache_store(hc, key, keylen, p->h, 0);
#endif
      }
      else
        mutt_free_header(&p->h);
#ifdef USE_HCACHE
    }
    mutt_hcache_free(hc, &data);
#endif
    last = p;
  }
#ifdef USE_HCACHE
  mutt_hcache_close(hc);
#endif

  mh_sort_natural(ctx, md);
}

static int mh_close_mailbox(struct Context *ctx)
{
  FREE(&ctx->data);

  return 0;
}

/**
 * mh_read_dir - Read a MH/maildir style mailbox
 * @param ctx    Current mailbox
 * @param subdir NULL for MH mailboxes,
 *               otherwise the subdir of the maildir mailbox to read from
 */
static int mh_read_dir(struct Context *ctx, const char *subdir)
{
  struct Maildir *md = NULL;
  struct MhSequences mhs;
  struct Maildir **last = NULL;
  struct MhData *data = NULL;
  int count;
  char msgbuf[STRING];
  struct Progress progress;

  memset(&mhs, 0, sizeof(mhs));
  if (!ctx->quiet)
  {
    snprintf(msgbuf, sizeof(msgbuf), _("Scanning %s..."), ctx->path);
    mutt_progress_init(&progress, msgbuf, MUTT_PROGRESS_MSG, ReadInc, 0);
  }

  if (!ctx->data)
  {
    ctx->data = safe_calloc(1, sizeof(struct MhData));
  }
  data = mh_data(ctx);

  maildir_update_mtime(ctx);

  md = NULL;
  last = &md;
  count = 0;
  if (maildir_parse_dir(ctx, &last, subdir, &count, &progress) == -1)
    return -1;

  if (!ctx->quiet)
  {
    snprintf(msgbuf, sizeof(msgbuf), _("Reading %s..."), ctx->path);
    mutt_progress_init(&progress, msgbuf, MUTT_PROGRESS_MSG, ReadInc, count);
  }
  maildir_delayed_parsing(ctx, &md, &progress);

  if (ctx->magic == MUTT_MH)
  {
    if (mh_read_sequences(&mhs, ctx->path) < 0)
    {
      maildir_free_maildir(&md);
      return -1;
    }
    mh_update_maildir(md, &mhs);
    mhs_free_sequences(&mhs);
  }

  maildir_move_to_context(ctx, &md);

  if (!data->mh_umask)
    data->mh_umask = mh_umask(ctx);

  return 0;
}

/**
 * maildir_read_dir - read a maildir style mailbox
 */
static int maildir_read_dir(struct Context *ctx)
{
  /* maildir looks sort of like MH, except that there are two subdirectories
   * of the main folder path from which to read messages
   */
  if (mh_read_dir(ctx, "new") == -1 || mh_read_dir(ctx, "cur") == -1)
    return -1;

  return 0;
}

static int maildir_open_mailbox(struct Context *ctx)
{
  return maildir_read_dir(ctx);
}

static int maildir_open_mailbox_append(struct Context *ctx, int flags)
{
  char tmp[_POSIX_PATH_MAX];

  if (flags & MUTT_APPENDNEW)
  {
    if (mkdir(ctx->path, S_IRWXU))
    {
      mutt_perror(ctx->path);
      return -1;
    }

    snprintf(tmp, sizeof(tmp), "%s/cur", ctx->path);
    if (mkdir(tmp, S_IRWXU))
    {
      mutt_perror(tmp);
      rmdir(ctx->path);
      return -1;
    }

    snprintf(tmp, sizeof(tmp), "%s/new", ctx->path);
    if (mkdir(tmp, S_IRWXU))
    {
      mutt_perror(tmp);
      snprintf(tmp, sizeof(tmp), "%s/cur", ctx->path);
      rmdir(tmp);
      rmdir(ctx->path);
      return -1;
    }

    snprintf(tmp, sizeof(tmp), "%s/tmp", ctx->path);
    if (mkdir(tmp, S_IRWXU))
    {
      mutt_perror(tmp);
      snprintf(tmp, sizeof(tmp), "%s/cur", ctx->path);
      rmdir(tmp);
      snprintf(tmp, sizeof(tmp), "%s/new", ctx->path);
      rmdir(tmp);
      rmdir(ctx->path);
      return -1;
    }
  }

  return 0;
}

static int mh_open_mailbox(struct Context *ctx)
{
  return mh_read_dir(ctx, NULL);
}

static int mh_open_mailbox_append(struct Context *ctx, int flags)
{
  char tmp[_POSIX_PATH_MAX];
  int i;

  if (flags & MUTT_APPENDNEW)
  {
    if (mkdir(ctx->path, S_IRWXU))
    {
      mutt_perror(ctx->path);
      return -1;
    }

    snprintf(tmp, sizeof(tmp), "%s/.mh_sequences", ctx->path);
    i = creat(tmp, S_IRWXU);
    if (i == -1)
    {
      mutt_perror(tmp);
      rmdir(ctx->path);
      return -1;
    }
    close(i);
  }

  return 0;
}

/*
 * Open a new (temporary) message in an MH folder.
 */

static int mh_open_new_message(struct Message *msg, struct Context *dest, struct Header *hdr)
{
  return mh_mkstemp(dest, &msg->fp, &msg->path);
}

static int ch_compar(const void *a, const void *b)
{
  return (int) (*((const char *) a) - *((const char *) b));
}

void maildir_flags(char *dest, size_t destlen, struct Header *hdr)
{
  *dest = '\0';

  /*
   * The maildir specification requires that all files in the cur
   * subdirectory have the :unique string appended, regardless of whether
   * or not there are any flags.  If .old is set, we know that this message
   * will end up in the cur directory, so we include it in the following
   * test even though there is no associated flag.
   */

  if (hdr && (hdr->flagged || hdr->replied || hdr->read || hdr->deleted ||
              hdr->old || hdr->maildir_flags))
  {
    char tmp[LONG_STRING];
    snprintf(tmp, sizeof(tmp), "%s%s%s%s%s", hdr->flagged ? "F" : "",
             hdr->replied ? "R" : "", hdr->read ? "S" : "",
             hdr->deleted ? "T" : "", NONULL(hdr->maildir_flags));
    if (hdr->maildir_flags)
      qsort(tmp, strlen(tmp), 1, ch_compar);
    snprintf(dest, destlen, ":2,%s", tmp);
  }
}

static int maildir_mh_open_message(struct Context *ctx, struct Message *msg,
                                   int msgno, int is_maildir)
{
  struct Header *cur = ctx->hdrs[msgno];
  char path[_POSIX_PATH_MAX];

  snprintf(path, sizeof(path), "%s/%s", ctx->path, cur->path);

  msg->fp = fopen(path, "r");
  if (msg->fp == NULL && errno == ENOENT && is_maildir)
    msg->fp = maildir_open_find_message(ctx->path, cur->path, NULL);

  if (!msg->fp)
  {
    mutt_perror(path);
    mutt_debug(1, "maildir_mh_open_message: fopen: %s: %s (errno %d).\n", path,
               strerror(errno), errno);
    return -1;
  }

  return 0;
}

static int maildir_open_message(struct Context *ctx, struct Message *msg, int msgno)
{
  return maildir_mh_open_message(ctx, msg, msgno, 1);
}

static int mh_open_message(struct Context *ctx, struct Message *msg, int msgno)
{
  return maildir_mh_open_message(ctx, msg, msgno, 0);
}

static int mh_close_message(struct Context *ctx, struct Message *msg)
{
  return safe_fclose(&msg->fp);
}

/**
 * maildir_open_new_message - Open a new message in a maildir folder
 *
 * Open a new (temporary) message in a maildir folder.
 *
 * Note that this uses _almost_ the maildir file name format,
 * but with a {cur,new} prefix.
 */
static int maildir_open_new_message(struct Message *msg, struct Context *dest,
                                    struct Header *hdr)
{
  int fd;
  char path[_POSIX_PATH_MAX];
  char suffix[16];
  char subdir[16];
  mode_t omask;

  if (hdr)
  {
    bool deleted = hdr->deleted;
    hdr->deleted = false;

    maildir_flags(suffix, sizeof(suffix), hdr);

    hdr->deleted = deleted;
  }
  else
    *suffix = '\0';

  if (hdr && (hdr->read || hdr->old))
    strfcpy(subdir, "cur", sizeof(subdir));
  else
    strfcpy(subdir, "new", sizeof(subdir));

  omask = umask(mh_umask(dest));
  while (true)
  {
    snprintf(path, _POSIX_PATH_MAX, "%s/tmp/%s.%lld.R%" PRIu64 ".%s%s", dest->path, subdir,
             (long long) time(NULL), mutt_rand64(), NONULL(ShortHostname), suffix);

    mutt_debug(2, "maildir_open_new_message (): Trying %s.\n", path);

    fd = open(path, O_WRONLY | O_EXCL | O_CREAT, 0666);
    if (fd == -1)
    {
      if (errno != EEXIST)
      {
        umask(omask);
        mutt_perror(path);
        return -1;
      }
    }
    else
    {
      mutt_debug(2, "maildir_open_new_message (): Success.\n");
      msg->path = safe_strdup(path);
      break;
    }
  }
  umask(omask);

  msg->fp = fdopen(fd, "w");
  if (!msg->fp)
  {
    FREE(&msg->path);
    close(fd);
    unlink(path);
    return -1;
  }

  return 0;
}

/**
 * _maildir_commit_message - Commit a message to a maildir folder
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
 *    tmp/{cur,new}.neomutt-HOSTNAME-PID-COUNTER:flags
 *
 * See also maildir_open_new_message().
 */
static int _maildir_commit_message(struct Context *ctx, struct Message *msg, struct Header *hdr)
{
  char subdir[4];
  char suffix[16];
  char path[_POSIX_PATH_MAX];
  char full[_POSIX_PATH_MAX];
  char *s = NULL;

  if (safe_fsync_close(&msg->fp))
  {
    mutt_perror(_("Could not flush message to disk"));
    return -1;
  }

  /* extract the subdir */
  s = strrchr(msg->path, '/') + 1;
  strfcpy(subdir, s, 4);

  /* extract the flags */
  if ((s = strchr(s, ':')))
    strfcpy(suffix, s, sizeof(suffix));
  else
    suffix[0] = '\0';

  /* construct a new file name. */
  while (true)
  {
    snprintf(path, _POSIX_PATH_MAX, "%s/%lld.R%" PRIu64 ".%s%s", subdir,
             (long long) time(NULL), mutt_rand64(), NONULL(ShortHostname), suffix);
    snprintf(full, _POSIX_PATH_MAX, "%s/%s", ctx->path, path);

    mutt_debug(2, "_maildir_commit_message (): renaming %s to %s.\n", msg->path, full);

    if (safe_rename(msg->path, full) == 0)
    {
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
        if (utime(full, &ut))
        {
          mutt_perror(
              _("_maildir_commit_message(): unable to set time on file"));
          goto post_rename_err;
        }
      }

#ifdef USE_NOTMUCH
      if (ctx->magic == MUTT_NOTMUCH)
        nm_update_filename(ctx, hdr->path, full, hdr);
#endif
      if (hdr)
        mutt_str_replace(&hdr->path, path);
      mutt_str_replace(&msg->commited_path, full);
      FREE(&msg->path);

      return 0;

    post_rename_err:
      return -1;
    }
    else if (errno != EEXIST)
    {
      mutt_perror(ctx->path);
      return -1;
    }
  }
}

static int maildir_commit_message(struct Context *ctx, struct Message *msg)
{
  return _maildir_commit_message(ctx, msg, NULL);
}

/*
 * commit a message to an MH folder.
 */

static int _mh_commit_message(struct Context *ctx, struct Message *msg,
                              struct Header *hdr, short updseq)
{
  DIR *dirp = NULL;
  struct dirent *de = NULL;
  char *cp = NULL, *dep = NULL;
  unsigned int n, hi = 0;
  char path[_POSIX_PATH_MAX];
  char tmp[16];

  if (safe_fsync_close(&msg->fp))
  {
    mutt_perror(_("Could not flush message to disk"));
    return -1;
  }

  dirp = opendir(ctx->path);
  if (!dirp)
  {
    mutt_perror(ctx->path);
    return -1;
  }

  /* figure out what the next message number is */
  while ((de = readdir(dirp)) != NULL)
  {
    dep = de->d_name;
    if (*dep == ',')
      dep++;
    cp = dep;
    while (*cp)
    {
      if (!isdigit((unsigned char) *cp))
        break;
      cp++;
    }
    if (!*cp)
    {
      n = atoi(dep);
      if (n > hi)
        hi = n;
    }
  }
  closedir(dirp);

  /*
   * Now try to rename the file to the proper name.
   *
   * Note: We may have to try multiple times, until we find a free
   * slot.
   */

  while (true)
  {
    hi++;
    snprintf(tmp, sizeof(tmp), "%d", hi);
    snprintf(path, sizeof(path), "%s/%s", ctx->path, tmp);
    if (safe_rename(msg->path, path) == 0)
    {
      if (hdr)
        mutt_str_replace(&hdr->path, tmp);
      mutt_str_replace(&msg->commited_path, path);
      FREE(&msg->path);
      break;
    }
    else if (errno != EEXIST)
    {
      mutt_perror(ctx->path);
      return -1;
    }
  }
  if (updseq)
    mh_sequences_add_one(ctx, hi, !msg->flags.read, msg->flags.flagged,
                         msg->flags.replied);
  return 0;
}

static int mh_commit_message(struct Context *ctx, struct Message *msg)
{
  return _mh_commit_message(ctx, msg, NULL, 1);
}

/**
 * mh_rewrite_message - Sync a message in an MH folder
 *
 * This code is also used for attachment deletion in maildir folders.
 */
static int mh_rewrite_message(struct Context *ctx, int msgno)
{
  struct Header *h = ctx->hdrs[msgno];
  struct Message *dest = NULL;

  int rc;
  bool restore = true;
  char oldpath[_POSIX_PATH_MAX];
  char newpath[_POSIX_PATH_MAX];
  char partpath[_POSIX_PATH_MAX];

  long old_body_offset = h->content->offset;
  long old_body_length = h->content->length;
  long old_hdr_lines = h->lines;

  dest = mx_open_new_message(ctx, h, 0);
  if (!dest)
    return -1;

  rc = mutt_copy_message(dest->fp, ctx, h, MUTT_CM_UPDATE, CH_UPDATE | CH_UPDATE_LEN);
  if (rc == 0)
  {
    snprintf(oldpath, _POSIX_PATH_MAX, "%s/%s", ctx->path, h->path);
    strfcpy(partpath, h->path, _POSIX_PATH_MAX);

    if (ctx->magic == MUTT_MAILDIR)
      rc = _maildir_commit_message(ctx, dest, h);
    else
      rc = _mh_commit_message(ctx, dest, h, 0);

    mx_close_message(ctx, &dest);

    if (rc == 0)
    {
      unlink(oldpath);
      restore = false;
    }

    /*
     * Try to move the new message to the old place.
     * (MH only.)
     *
     * This is important when we are just updating flags.
     *
     * Note that there is a race condition against programs which
     * use the first free slot instead of the maximum message
     * number.  NeoMutt does _not_ behave like this.
     *
     * Anyway, if this fails, the message is in the folder, so
     * all what happens is that a concurrently running neomutt will
     * lose flag modifications.
     */

    if (ctx->magic == MUTT_MH && rc == 0)
    {
      snprintf(newpath, _POSIX_PATH_MAX, "%s/%s", ctx->path, h->path);
      rc = safe_rename(newpath, oldpath);
      if (rc == 0)
        mutt_str_replace(&h->path, partpath);
    }
  }
  else
    mx_close_message(ctx, &dest);

  if (rc == -1 && restore)
  {
    h->content->offset = old_body_offset;
    h->content->length = old_body_length;
    h->lines = old_hdr_lines;
  }

  mutt_free_body(&h->content->parts);
  return rc;
}

static int mh_sync_message(struct Context *ctx, int msgno)
{
  struct Header *h = ctx->hdrs[msgno];

  if (h->attach_del || h->xlabel_changed ||
      (h->env && (h->env->refs_changed || h->env->irt_changed)))
    if (mh_rewrite_message(ctx, msgno) != 0)
      return -1;

  return 0;
}

static int maildir_sync_message(struct Context *ctx, int msgno)
{
  struct Header *h = ctx->hdrs[msgno];

  if (h->attach_del || h->xlabel_changed ||
      (h->env && (h->env->refs_changed || h->env->irt_changed)))
  {
    /* when doing attachment deletion/rethreading, fall back to the MH case. */
    if (mh_rewrite_message(ctx, msgno) != 0)
      return -1;
  }
  else
  {
    /* we just have to rename the file. */

    char newpath[_POSIX_PATH_MAX];
    char partpath[_POSIX_PATH_MAX];
    char fullpath[_POSIX_PATH_MAX];
    char oldpath[_POSIX_PATH_MAX];
    char suffix[16];
    char *p = NULL;

    p = strrchr(h->path, '/');
    if (!p)
    {
      mutt_debug(1, "maildir_sync_message: %s: unable to find subdir!\n", h->path);
      return -1;
    }
    p++;
    strfcpy(newpath, p, sizeof(newpath));

    /* kill the previous flags */
    p = strchr(newpath, ':');
    if (p)
      *p = '\0';

    maildir_flags(suffix, sizeof(suffix), h);

    snprintf(partpath, sizeof(partpath), "%s/%s%s",
             (h->read || h->old) ? "cur" : "new", newpath, suffix);
    snprintf(fullpath, sizeof(fullpath), "%s/%s", ctx->path, partpath);
    snprintf(oldpath, sizeof(oldpath), "%s/%s", ctx->path, h->path);

    if (mutt_strcmp(fullpath, oldpath) == 0)
    {
      /* message hasn't really changed */
      return 0;
    }

    /* record that the message is possibly marked as trashed on disk */
    h->trash = h->deleted;

    if (rename(oldpath, fullpath) != 0)
    {
      mutt_perror("rename");
      return -1;
    }
    mutt_str_replace(&h->path, partpath);
  }
  return 0;
}

#ifdef USE_HCACHE
int mh_sync_mailbox_message(struct Context *ctx, int msgno, header_cache_t *hc)
#else
int mh_sync_mailbox_message(struct Context *ctx, int msgno)
#endif
{
#ifdef USE_HCACHE
  const char *key = NULL;
  size_t keylen;
#endif
  char path[_POSIX_PATH_MAX], tmp[_POSIX_PATH_MAX];
  struct Header *h = ctx->hdrs[msgno];

  if (h->deleted && (ctx->magic != MUTT_MAILDIR || !option(OPT_MAILDIR_TRASH)))
  {
    snprintf(path, sizeof(path), "%s/%s", ctx->path, h->path);
    if (ctx->magic == MUTT_MAILDIR || (option(OPT_MH_PURGE) && ctx->magic == MUTT_MH))
    {
#ifdef USE_HCACHE
      if (hc)
      {
        if (ctx->magic == MUTT_MH)
        {
          key = h->path;
          keylen = strlen(key);
        }
        else
        {
          key = h->path + 3;
          keylen = maildir_hcache_keylen(key);
        }
        mutt_hcache_delete(hc, key, keylen);
      }
#endif /* USE_HCACHE */
      unlink(path);
    }
    else if (ctx->magic == MUTT_MH)
    {
      /* MH just moves files out of the way when you delete them */
      if (*h->path != ',')
      {
        snprintf(tmp, sizeof(tmp), "%s/,%s", ctx->path, h->path);
        unlink(tmp);
        rename(path, tmp);
      }
    }
  }
  else if (h->changed || h->attach_del || h->xlabel_changed ||
           (ctx->magic == MUTT_MAILDIR &&
            (option(OPT_MAILDIR_TRASH) || h->trash) && (h->deleted != h->trash)))
  {
    if (ctx->magic == MUTT_MAILDIR)
    {
      if (maildir_sync_message(ctx, msgno) == -1)
        return -1;
    }
    else
    {
      if (mh_sync_message(ctx, msgno) == -1)
        return -1;
    }
  }

#ifdef USE_HCACHE
  if (hc && h->changed)
  {
    if (ctx->magic == MUTT_MH)
    {
      key = h->path;
      keylen = strlen(key);
    }
    else
    {
      key = h->path + 3;
      keylen = maildir_hcache_keylen(key);
    }
    mutt_hcache_store(hc, key, keylen, h, 0);
  }
#endif

  return 0;
}

static char *maildir_canon_filename(char *dest, const char *src, size_t l)
{
  char *t = NULL, *u = NULL;

  if ((t = strrchr(src, '/')))
    src = t + 1;

  strfcpy(dest, src, l);
  if ((u = strrchr(dest, ':')))
    *u = '\0';

  return dest;
}

static void maildir_update_tables(struct Context *ctx, int *index_hint)
{
  short old_sort;
  int old_count;
  int i, j;

  if (Sort != SORT_ORDER)
  {
    old_sort = Sort;
    Sort = SORT_ORDER;
    mutt_sort_headers(ctx, 1);
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

  mx_update_tables(ctx, false);
  mutt_clear_threads(ctx);
}

/**
 * maildir_check_mailbox - Check for new mail
 *
 * This function handles arrival of new mail and reopening of maildir folders.
 * The basic idea here is we check to see if either the new or cur
 * subdirectories have changed, and if so, we scan them for the list of files.
 * We check for newly added messages, and then merge the flags messages we
 * already knew about.  We don't treat either subdirectory differently, as mail
 * could be copied directly into the cur directory from another agent.
 */
static int maildir_check_mailbox(struct Context *ctx, int *index_hint)
{
  struct stat st_new; /* status of the "new" subdirectory */
  struct stat st_cur; /* status of the "cur" subdirectory */
  char buf[_POSIX_PATH_MAX];
  int changed = 0;            /* bitmask representing which subdirectories
                                 have changed.  0x1 = new, 0x2 = cur */
  bool occult = false;        /* messages were removed from the mailbox */
  int have_new = 0;           /* messages were added to the mailbox */
  bool flags_changed = false; /* message flags were changed in the mailbox */
  struct Maildir *md = NULL;  /* list of messages in the mailbox */
  struct Maildir **last = NULL, *p = NULL;
  int count = 0;
  struct Hash *fnames = NULL; /* hash table for quickly looking up the base filename
                                   for a maildir message */
  struct MhData *data = mh_data(ctx);

  /* XXX seems like this check belongs in mx_check_mailbox()
   * rather than here.
   */
  if (!option(OPT_CHECK_NEW))
    return 0;

  snprintf(buf, sizeof(buf), "%s/new", ctx->path);
  if (stat(buf, &st_new) == -1)
    return -1;

  snprintf(buf, sizeof(buf), "%s/cur", ctx->path);
  if (stat(buf, &st_cur) == -1)
    return -1;

  /* determine which subdirectories need to be scanned */
  if (st_new.st_mtime > ctx->mtime)
    changed = 1;
  if (st_cur.st_mtime > data->mtime_cur)
    changed |= 2;

  if (!changed)
    return 0; /* nothing to do */

  /* update the modification times on the mailbox */
  data->mtime_cur = st_cur.st_mtime;
  ctx->mtime = st_new.st_mtime;

  /* do a fast scan of just the filenames in
   * the subdirectories that have changed.
   */
  md = NULL;
  last = &md;
  if (changed & 1)
    maildir_parse_dir(ctx, &last, "new", &count, NULL);
  if (changed & 2)
    maildir_parse_dir(ctx, &last, "cur", &count, NULL);

  /* we create a hash table keyed off the canonical (sans flags) filename
   * of each message we scanned.  This is used in the loop over the
   * existing messages below to do some correlation.
   */
  fnames = hash_create(count, 0);

  for (p = md; p; p = p->next)
  {
    maildir_canon_filename(buf, p->h->path, sizeof(buf));
    p->canon_fname = safe_strdup(buf);
    hash_insert(fnames, p->canon_fname, p);
  }

  /* check for modifications and adjust flags */
  for (int i = 0; i < ctx->msgcount; i++)
  {
    ctx->hdrs[i]->active = false;
    maildir_canon_filename(buf, ctx->hdrs[i]->path, sizeof(buf));
    p = hash_find(fnames, buf);
    if (p && p->h)
    {
      /* message already exists, merge flags */
      ctx->hdrs[i]->active = true;

      /* check to see if the message has moved to a different
       * subdirectory.  If so, update the associated filename.
       */
      if (mutt_strcmp(ctx->hdrs[i]->path, p->h->path) != 0)
        mutt_str_replace(&ctx->hdrs[i]->path, p->h->path);

      /* if the user hasn't modified the flags on this message, update
       * the flags we just detected.
       */
      if (!ctx->hdrs[i]->changed)
        if (maildir_update_flags(ctx, ctx->hdrs[i], p->h))
          flags_changed = true;

      if (ctx->hdrs[i]->deleted == ctx->hdrs[i]->trash)
        if (ctx->hdrs[i]->deleted != p->h->deleted)
        {
          ctx->hdrs[i]->deleted = p->h->deleted;
          flags_changed = true;
        }
      ctx->hdrs[i]->trash = p->h->trash;

      /* this is a duplicate of an existing header, so remove it */
      mutt_free_header(&p->h);
    }
    /* This message was not in the list of messages we just scanned.
     * Check to see if we have enough information to know if the
     * message has disappeared out from underneath us.
     */
    else if (((changed & 1) && (strncmp(ctx->hdrs[i]->path, "new/", 4) == 0)) ||
             ((changed & 2) && (strncmp(ctx->hdrs[i]->path, "cur/", 4) == 0)))
    {
      /* This message disappeared, so we need to simulate a "reopen"
       * event.  We know it disappeared because we just scanned the
       * subdirectory it used to reside in.
       */
      occult = true;
    }
    else
    {
      /* This message resides in a subdirectory which was not
       * modified, so we assume that it is still present and
       * unchanged.
       */
      ctx->hdrs[i]->active = true;
    }
  }

  /* destroy the file name hash */
  hash_destroy(&fnames, NULL);

  /* If we didn't just get new mail, update the tables. */
  if (occult)
    maildir_update_tables(ctx, index_hint);

  /* do any delayed parsing we need to do. */
  maildir_delayed_parsing(ctx, &md, NULL);

  /* Incorporate new messages */
  have_new = maildir_move_to_context(ctx, &md);

  if (occult)
    return MUTT_REOPENED;
  if (have_new)
    return MUTT_NEW_MAIL;
  if (flags_changed)
    return MUTT_FLAGS;
  return 0;
}

/**
 * mh_check_mailbox - Check for new mail
 *
 * This function handles arrival of new mail and reopening of mh/maildir
 * folders. Things are getting rather complex because we don't have a
 * well-defined "mailbox order", so the tricks from mbox.c and mx.c won't work
 * here.
 *
 * Don't change this code unless you _really_ understand what happens.
 */
static int mh_check_mailbox(struct Context *ctx, int *index_hint)
{
  char buf[_POSIX_PATH_MAX];
  struct stat st, st_cur;
  bool modified = false, have_new = false, occult = false, flags_changed = false;
  struct Maildir *md = NULL, *p = NULL;
  struct Maildir **last = NULL;
  struct MhSequences mhs;
  int count = 0;
  struct Hash *fnames = NULL;
  int i;
  struct MhData *data = mh_data(ctx);

  if (!option(OPT_CHECK_NEW))
    return 0;

  strfcpy(buf, ctx->path, sizeof(buf));
  if (stat(buf, &st) == -1)
    return -1;

  /* create .mh_sequences when there isn't one. */
  snprintf(buf, sizeof(buf), "%s/.mh_sequences", ctx->path);
  if ((i = stat(buf, &st_cur)) == -1 && errno == ENOENT)
  {
    char *tmp = NULL;
    FILE *fp = NULL;

    if (mh_mkstemp(ctx, &fp, &tmp) == 0)
    {
      safe_fclose(&fp);
      if (safe_rename(tmp, buf) == -1)
        unlink(tmp);
      FREE(&tmp);
    }
  }

  if (i == -1 && stat(buf, &st_cur) == -1)
    modified = true;

  if (st.st_mtime > ctx->mtime || st_cur.st_mtime > data->mtime_cur)
    modified = true;

  if (!modified)
    return 0;

  data->mtime_cur = st_cur.st_mtime;
  ctx->mtime = st.st_mtime;

  memset(&mhs, 0, sizeof(mhs));

  md = NULL;
  last = &md;

  maildir_parse_dir(ctx, &last, NULL, &count, NULL);
  maildir_delayed_parsing(ctx, &md, NULL);

  if (mh_read_sequences(&mhs, ctx->path) < 0)
    return -1;
  mh_update_maildir(md, &mhs);
  mhs_free_sequences(&mhs);

  /* check for modifications and adjust flags */
  fnames = hash_create(count, 0);

  for (p = md; p; p = p->next)
  {
    /* the hash key must survive past the header, which is freed below. */
    p->canon_fname = safe_strdup(p->h->path);
    hash_insert(fnames, p->canon_fname, p);
  }

  for (i = 0; i < ctx->msgcount; i++)
  {
    ctx->hdrs[i]->active = false;

    if ((p = hash_find(fnames, ctx->hdrs[i]->path)) && p->h &&
        (mbox_strict_cmp_headers(ctx->hdrs[i], p->h)))
    {
      ctx->hdrs[i]->active = true;
      /* found the right message */
      if (!ctx->hdrs[i]->changed)
        if (maildir_update_flags(ctx, ctx->hdrs[i], p->h))
          flags_changed = true;

      mutt_free_header(&p->h);
    }
    else /* message has disappeared */
      occult = true;
  }

  /* destroy the file name hash */

  hash_destroy(&fnames, NULL);

  /* If we didn't just get new mail, update the tables. */
  if (occult)
    maildir_update_tables(ctx, index_hint);

  /* Incorporate new messages */
  have_new = maildir_move_to_context(ctx, &md);

  if (occult)
    return MUTT_REOPENED;
  if (have_new)
    return MUTT_NEW_MAIL;
  if (flags_changed)
    return MUTT_FLAGS;
  return 0;
}

static int mh_sync_mailbox(struct Context *ctx, int *index_hint)
{
  int i, j;
#ifdef USE_HCACHE
  header_cache_t *hc = NULL;
#endif /* USE_HCACHE */
  char msgbuf[STRING];
  struct Progress progress;

  if (ctx->magic == MUTT_MH)
    i = mh_check_mailbox(ctx, index_hint);
  else
    i = maildir_check_mailbox(ctx, index_hint);

  if (i != 0)
    return i;

#ifdef USE_HCACHE
  if (ctx->magic == MUTT_MAILDIR || ctx->magic == MUTT_MH)
    hc = mutt_hcache_open(HeaderCache, ctx->path, NULL);
#endif /* USE_HCACHE */

  if (!ctx->quiet)
  {
    snprintf(msgbuf, sizeof(msgbuf), _("Writing %s..."), ctx->path);
    mutt_progress_init(&progress, msgbuf, MUTT_PROGRESS_MSG, WriteInc, ctx->msgcount);
  }

  for (i = 0; i < ctx->msgcount; i++)
  {
    if (!ctx->quiet)
      mutt_progress_update(&progress, i, -1);

#ifdef USE_HCACHE
    if (mh_sync_mailbox_message(ctx, i, hc) == -1)
      goto err;
#else
    if (mh_sync_mailbox_message(ctx, i) == -1)
      goto err;
#endif
  }

#ifdef USE_HCACHE
  if (ctx->magic == MUTT_MAILDIR || ctx->magic == MUTT_MH)
    mutt_hcache_close(hc);
#endif /* USE_HCACHE */

  if (ctx->magic == MUTT_MH)
    mh_update_sequences(ctx);

  /* XXX race condition? */

  maildir_update_mtime(ctx);

  /* adjust indices */

  if (ctx->deleted)
  {
    for (i = 0, j = 0; i < ctx->msgcount; i++)
    {
      if (!ctx->hdrs[i]->deleted || (ctx->magic == MUTT_MAILDIR && option(OPT_MAILDIR_TRASH)))
        ctx->hdrs[i]->index = j++;
    }
  }

  return 0;

err:
#ifdef USE_HCACHE
  if (ctx->magic == MUTT_MAILDIR || ctx->magic == MUTT_MH)
    mutt_hcache_close(hc);
#endif /* USE_HCACHE */
  return -1;
}

bool maildir_update_flags(struct Context *ctx, struct Header *o, struct Header *n)
{
  /* save the global state here so we can reset it at the
   * end of list block if required.
   */
  bool context_changed = ctx->changed;
  bool header_changed;

  /* user didn't modify this message.  alter the flags to match the
   * current state on disk.  This may not actually do
   * anything. mutt_set_flag() will just ignore the call if the status
   * bits are already properly set, but it is still faster not to pass
   * through it */
  if (o->flagged != n->flagged)
    mutt_set_flag(ctx, o, MUTT_FLAG, n->flagged);
  if (o->replied != n->replied)
    mutt_set_flag(ctx, o, MUTT_REPLIED, n->replied);
  if (o->read != n->read)
    mutt_set_flag(ctx, o, MUTT_READ, n->read);
  if (o->old != n->old)
    mutt_set_flag(ctx, o, MUTT_OLD, n->old);

  /* mutt_set_flag() will set this, but we don't need to
   * sync the changes we made because we just updated the
   * context to match the current on-disk state of the
   * message.
   */
  header_changed = o->changed;
  o->changed = false;

  /* if the mailbox was not modified before we made these
   * changes, unset the changed flag since nothing needs to
   * be synchronized.
   */
  if (!context_changed)
    ctx->changed = false;

  return header_changed;
}

/**
 * _maildir_open_find_message - Find a message in a maildir folder
 *
 * These functions try to find a message in a maildir folder when it
 * has moved under our feet.  Note that this code is rather expensive, but
 * then again, it's called rarely.
 */
static FILE *_maildir_open_find_message(const char *folder, const char *unique,
                                        const char *subfolder, char **newname)
{
  char dir[_POSIX_PATH_MAX];
  char tunique[_POSIX_PATH_MAX];
  char fname[LONG_STRING];

  DIR *dp = NULL;
  struct dirent *de = NULL;

  FILE *fp = NULL;
  int oe = ENOENT;

  snprintf(dir, sizeof(dir), "%s/%s", folder, subfolder);

  dp = opendir(dir);
  if (!dp)
  {
    errno = ENOENT;
    return NULL;
  }

  while ((de = readdir(dp)))
  {
    maildir_canon_filename(tunique, de->d_name, sizeof(tunique));

    if (mutt_strcmp(tunique, unique) == 0)
    {
      snprintf(fname, sizeof(fname), "%s/%s/%s", folder, subfolder, de->d_name);
      fp = fopen(fname, "r");
      oe = errno;
      break;
    }
  }

  closedir(dp);

  if (newname && fp)
    *newname = safe_strdup(fname);

  errno = oe;
  return fp;
}

FILE *maildir_open_find_message(const char *folder, const char *msg, char **newname)
{
  char unique[_POSIX_PATH_MAX];
  FILE *fp = NULL;

  static unsigned int new_hits = 0, cur_hits = 0; /* simple dynamic optimization */

  maildir_canon_filename(unique, msg, sizeof(unique));

  if ((fp = _maildir_open_find_message(
           folder, unique, new_hits > cur_hits ? "new" : "cur", newname)) ||
      errno != ENOENT)
  {
    if (new_hits < UINT_MAX && cur_hits < UINT_MAX)
    {
      new_hits += (new_hits > cur_hits ? 1 : 0);
      cur_hits += (new_hits > cur_hits ? 0 : 1);
    }

    return fp;
  }
  if ((fp = _maildir_open_find_message(
           folder, unique, new_hits > cur_hits ? "cur" : "new", newname)) ||
      errno != ENOENT)
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

/**
 * maildir_check_empty - Is the mailbox empty
 * @param path Mailbox to check
 * @retval 1 Mailbox is empty
 * @retval 0 Mailbox contains mail
 * @retval -1 Error
 */
int maildir_check_empty(const char *path)
{
  DIR *dp = NULL;
  struct dirent *de = NULL;
  int r = 1; /* assume empty until we find a message */
  char realpath[_POSIX_PATH_MAX];
  int iter = 0;

  /* Strategy here is to look for any file not beginning with a period */

  do
  {
    /* we do "cur" on the first iteration since it's more likely that we'll
     * find old messages without having to scan both subdirs
     */
    snprintf(realpath, sizeof(realpath), "%s/%s", path,
             iter == 0 ? "cur" : "new");
    dp = opendir(realpath);
    if (!dp)
      return -1;
    while ((de = readdir(dp)))
    {
      if (*de->d_name != '.')
      {
        r = 0;
        break;
      }
    }
    closedir(dp);
    iter++;
  } while (r && iter < 2);

  return r;
}

/**
 * mh_check_empty - Is mailbox empty
 * @param path Mailbox to check
 * @retval 1 Mailbox is empty
 * @retval 0 Mailbox contains mail
 * @retval -1 Error
 */
int mh_check_empty(const char *path)
{
  DIR *dp = NULL;
  struct dirent *de = NULL;
  int r = 1; /* assume empty until we find a message */

  dp = opendir(path);
  if (!dp)
    return -1;
  while ((de = readdir(dp)))
  {
    if (mh_valid_message(de->d_name))
    {
      r = 0;
      break;
    }
  }
  closedir(dp);

  return r;
}

bool mx_is_maildir(const char *path)
{
  char tmp[_POSIX_PATH_MAX];
  struct stat st;

  snprintf(tmp, sizeof(tmp), "%s/cur", path);
  if (stat(tmp, &st) == 0 && S_ISDIR(st.st_mode))
    return true;
  return false;
}

bool mx_is_mh(const char *path)
{
  char tmp[_POSIX_PATH_MAX];

  snprintf(tmp, sizeof(tmp), "%s/.mh_sequences", path);
  if (access(tmp, F_OK) == 0)
    return true;

  snprintf(tmp, sizeof(tmp), "%s/.xmhcache", path);
  if (access(tmp, F_OK) == 0)
    return true;

  snprintf(tmp, sizeof(tmp), "%s/.mew_cache", path);
  if (access(tmp, F_OK) == 0)
    return true;

  snprintf(tmp, sizeof(tmp), "%s/.mew-cache", path);
  if (access(tmp, F_OK) == 0)
    return true;

  snprintf(tmp, sizeof(tmp), "%s/.sylpheed_cache", path);
  if (access(tmp, F_OK) == 0)
    return true;

  /*
   * ok, this isn't an mh folder, but mh mode can be used to read
   * Usenet news from the spool. ;-)
   */

  snprintf(tmp, sizeof(tmp), "%s/.overview", path);
  if (access(tmp, F_OK) == 0)
    return true;

  return false;
}

struct MxOps mx_maildir_ops = {
  .open = maildir_open_mailbox,
  .open_append = maildir_open_mailbox_append,
  .close = mh_close_mailbox,
  .open_msg = maildir_open_message,
  .close_msg = mh_close_message,
  .commit_msg = maildir_commit_message,
  .open_new_msg = maildir_open_new_message,
  .check = maildir_check_mailbox,
  .sync = mh_sync_mailbox,
  .edit_msg_tags = NULL,
  .commit_msg_tags = NULL,
};

struct MxOps mx_mh_ops = {
  .open = mh_open_mailbox,
  .open_append = mh_open_mailbox_append,
  .close = mh_close_mailbox,
  .open_msg = mh_open_message,
  .close_msg = mh_close_message,
  .commit_msg = mh_commit_message,
  .open_new_msg = mh_open_new_message,
  .check = mh_check_mailbox,
  .sync = mh_sync_mailbox,
  .edit_msg_tags = NULL,
  .commit_msg_tags = NULL,
};
