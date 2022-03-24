/**
 * @file
 * MH Mailbox Sequences
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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

/**
 * @page maildir_sequence MH Mailbox Sequences
 *
 * MH Mailbox Sequences
 */

#include "config.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "sequence.h"

/**
 * mh_seq_alloc - Allocate more memory for sequences
 * @param mhs Existing sequences
 * @param i   Number required
 *
 * @note Memory is allocated in blocks of 128.
 */
static void mh_seq_alloc(struct MhSequences *mhs, int i)
{
  if ((i <= mhs->max) && mhs->flags)
    return;

  const int newmax = i + 128;
  int j = mhs->flags ? mhs->max + 1 : 0;
  mutt_mem_realloc(&mhs->flags, sizeof(mhs->flags[0]) * (newmax + 1));
  while (j <= newmax)
    mhs->flags[j++] = 0;

  mhs->max = newmax;
}

/**
 * mh_seq_free - Free some sequences
 * @param mhs Sequences to free
 */
void mh_seq_free(struct MhSequences *mhs)
{
  FREE(&mhs->flags);
}

/**
 * mh_seq_check - Get the flags for a given sequence
 * @param mhs Sequences
 * @param i   Index number required
 * @retval num Flags, see #MhSeqFlags
 */
MhSeqFlags mh_seq_check(struct MhSequences *mhs, int i)
{
  if (!mhs->flags || (i > mhs->max))
    return 0;
  return mhs->flags[i];
}

/**
 * mh_seq_set - Set a flag for a given sequence
 * @param mhs Sequences
 * @param i   Index number
 * @param f   Flags, see #MhSeqFlags
 * @retval num Resulting flags, see #MhSeqFlags
 */
MhSeqFlags mh_seq_set(struct MhSequences *mhs, int i, MhSeqFlags f)
{
  mh_seq_alloc(mhs, i);
  mhs->flags[i] |= f;
  return mhs->flags[i];
}

/**
 * mh_seq_add_one - Update the flags for one sequence
 * @param m       Mailbox
 * @param n       Sequence number to update
 * @param unseen  Update the unseen sequence
 * @param flagged Update the flagged sequence
 * @param replied Update the replied sequence
 */
void mh_seq_add_one(struct Mailbox *m, int n, bool unseen, bool flagged, bool replied)
{
  bool unseen_done = false;
  bool flagged_done = false;
  bool replied_done = false;

  char *tmpfname = NULL;
  char sequences[PATH_MAX];

  char seq_unseen[256];
  char seq_replied[256];
  char seq_flagged[256];

  char *buf = NULL;
  size_t sz;

  FILE *fp_new = NULL;
  if (!mh_mkstemp(m, &fp_new, &tmpfname))
    return;

  const char *const c_mh_seq_unseen = cs_subset_string(NeoMutt->sub, "mh_seq_unseen");
  const char *const c_mh_seq_replied = cs_subset_string(NeoMutt->sub, "mh_seq_replied");
  const char *const c_mh_seq_flagged = cs_subset_string(NeoMutt->sub, "mh_seq_flagged");
  snprintf(seq_unseen, sizeof(seq_unseen), "%s:", NONULL(c_mh_seq_unseen));
  snprintf(seq_replied, sizeof(seq_replied), "%s:", NONULL(c_mh_seq_replied));
  snprintf(seq_flagged, sizeof(seq_flagged), "%s:", NONULL(c_mh_seq_flagged));

  snprintf(sequences, sizeof(sequences), "%s/.mh_sequences", mailbox_path(m));
  FILE *fp_old = fopen(sequences, "r");
  if (fp_old)
  {
    while ((buf = mutt_file_read_line(buf, &sz, fp_old, NULL, MUTT_RL_NO_FLAGS)))
    {
      if (unseen && mutt_strn_equal(buf, seq_unseen, mutt_str_len(seq_unseen)))
      {
        fprintf(fp_new, "%s %d\n", buf, n);
        unseen_done = true;
      }
      else if (flagged && mutt_strn_equal(buf, seq_flagged, mutt_str_len(seq_flagged)))
      {
        fprintf(fp_new, "%s %d\n", buf, n);
        flagged_done = true;
      }
      else if (replied && mutt_strn_equal(buf, seq_replied, mutt_str_len(seq_replied)))
      {
        fprintf(fp_new, "%s %d\n", buf, n);
        replied_done = true;
      }
      else
        fprintf(fp_new, "%s\n", buf);
    }
  }
  mutt_file_fclose(&fp_old);
  FREE(&buf);

  if (!unseen_done && unseen)
    fprintf(fp_new, "%s: %d\n", NONULL(c_mh_seq_unseen), n);
  if (!flagged_done && flagged)
    fprintf(fp_new, "%s: %d\n", NONULL(c_mh_seq_flagged), n);
  if (!replied_done && replied)
    fprintf(fp_new, "%s: %d\n", NONULL(c_mh_seq_replied), n);

  mutt_file_fclose(&fp_new);

  unlink(sequences);
  if (mutt_file_safe_rename(tmpfname, sequences) != 0)
    unlink(tmpfname);

  FREE(&tmpfname);
}

/**
 * mh_seq_write_one - Write a flag sequence to a file
 * @param fp  File to write to
 * @param mhs Sequence list
 * @param f   Flag, see #MhSeqFlags
 * @param tag string tag, e.g. "unseen"
 */
static void mh_seq_write_one(FILE *fp, struct MhSequences *mhs, MhSeqFlags f, const char *tag)
{
  fprintf(fp, "%s:", tag);

  int first = -1;
  int last = -1;

  for (int i = 0; i <= mhs->max; i++)
  {
    if ((mh_seq_check(mhs, i) & f))
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
 * mh_seq_update - Update sequence numbers
 * @param m Mailbox
 *
 * XXX we don't currently remove deleted messages from sequences we don't know.
 * Should we?
 */
void mh_seq_update(struct Mailbox *m)
{
  char sequences[PATH_MAX];
  char *tmpfname = NULL;
  char *buf = NULL;
  char *p = NULL;
  size_t s;
  int seq_num = 0;

  int unseen = 0;
  int flagged = 0;
  int replied = 0;

  char seq_unseen[256];
  char seq_replied[256];
  char seq_flagged[256];

  struct MhSequences mhs = { 0 };

  const char *const c_mh_seq_unseen = cs_subset_string(NeoMutt->sub, "mh_seq_unseen");
  const char *const c_mh_seq_replied = cs_subset_string(NeoMutt->sub, "mh_seq_replied");
  const char *const c_mh_seq_flagged = cs_subset_string(NeoMutt->sub, "mh_seq_flagged");
  snprintf(seq_unseen, sizeof(seq_unseen), "%s:", NONULL(c_mh_seq_unseen));
  snprintf(seq_replied, sizeof(seq_replied), "%s:", NONULL(c_mh_seq_replied));
  snprintf(seq_flagged, sizeof(seq_flagged), "%s:", NONULL(c_mh_seq_flagged));

  FILE *fp_new = NULL;
  if (!mh_mkstemp(m, &fp_new, &tmpfname))
  {
    /* error message? */
    return;
  }

  snprintf(sequences, sizeof(sequences), "%s/.mh_sequences", mailbox_path(m));

  /* first, copy unknown sequences */
  FILE *fp_old = fopen(sequences, "r");
  if (fp_old)
  {
    while ((buf = mutt_file_read_line(buf, &s, fp_old, NULL, MUTT_RL_NO_FLAGS)))
    {
      if (mutt_str_startswith(buf, seq_unseen) || mutt_str_startswith(buf, seq_flagged) ||
          mutt_str_startswith(buf, seq_replied))
      {
        continue;
      }

      fprintf(fp_new, "%s\n", buf);
    }
  }
  mutt_file_fclose(&fp_old);

  /* now, update our unseen, flagged, and replied sequences */
  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;

    if (e->deleted)
      continue;

    p = strrchr(e->path, '/');
    if (p)
      p++;
    else
      p = e->path;

    if (!mutt_str_atoi_full(p, &seq_num))
      continue;

    if (!e->read)
    {
      mh_seq_set(&mhs, seq_num, MH_SEQ_UNSEEN);
      unseen++;
    }
    if (e->flagged)
    {
      mh_seq_set(&mhs, seq_num, MH_SEQ_FLAGGED);
      flagged++;
    }
    if (e->replied)
    {
      mh_seq_set(&mhs, seq_num, MH_SEQ_REPLIED);
      replied++;
    }
  }

  /* write out the new sequences */
  if (unseen)
    mh_seq_write_one(fp_new, &mhs, MH_SEQ_UNSEEN, NONULL(c_mh_seq_unseen));
  if (flagged)
    mh_seq_write_one(fp_new, &mhs, MH_SEQ_FLAGGED, NONULL(c_mh_seq_flagged));
  if (replied)
    mh_seq_write_one(fp_new, &mhs, MH_SEQ_REPLIED, NONULL(c_mh_seq_replied));

  mh_seq_free(&mhs);

  /* try to commit the changes - no guarantee here */
  mutt_file_fclose(&fp_new);

  unlink(sequences);
  if (mutt_file_safe_rename(tmpfname, sequences) != 0)
  {
    /* report an error? */
    unlink(tmpfname);
  }

  FREE(&tmpfname);
}

/**
 * mh_seq_read_token - Parse a number, or number range
 * @param t     String to parse
 * @param first First number
 * @param last  Last number (if a range, first number if not)
 * @retval  0 Success
 * @retval -1 Error
 */
static int mh_seq_read_token(char *t, int *first, int *last)
{
  char *p = strchr(t, '-');
  if (p)
  {
    *p++ = '\0';
    if (!mutt_str_atoi_full(t, first) || !mutt_str_atoi_full(p, last))
      return -1;
  }
  else
  {
    if (!mutt_str_atoi_full(t, first))
      return -1;
    *last = *first;
  }
  return 0;
}

/**
 * mh_seq_read - Read a set of MH sequences
 * @param mhs  Existing sequences
 * @param path File to read from
 * @retval  0 Success
 * @retval -1 Error
 */
int mh_seq_read(struct MhSequences *mhs, const char *path)
{
  char *buf = NULL;
  size_t sz = 0;

  MhSeqFlags flags;
  int first, last, rc = 0;

  char pathname[PATH_MAX];
  snprintf(pathname, sizeof(pathname), "%s/.mh_sequences", path);

  FILE *fp = fopen(pathname, "r");
  if (!fp)
    return 0; /* yes, ask callers to silently ignore the error */

  while ((buf = mutt_file_read_line(buf, &sz, fp, NULL, MUTT_RL_NO_FLAGS)))
  {
    char *t = strtok(buf, " \t:");
    if (!t)
      continue;

    const char *const c_mh_seq_unseen = cs_subset_string(NeoMutt->sub, "mh_seq_unseen");
    const char *const c_mh_seq_flagged = cs_subset_string(NeoMutt->sub, "mh_seq_flagged");
    const char *const c_mh_seq_replied = cs_subset_string(NeoMutt->sub, "mh_seq_replied");
    if (mutt_str_equal(t, c_mh_seq_unseen))
      flags = MH_SEQ_UNSEEN;
    else if (mutt_str_equal(t, c_mh_seq_flagged))
      flags = MH_SEQ_FLAGGED;
    else if (mutt_str_equal(t, c_mh_seq_replied))
      flags = MH_SEQ_REPLIED;
    else /* unknown sequence */
      continue;

    while ((t = strtok(NULL, " \t:")))
    {
      if (mh_seq_read_token(t, &first, &last) < 0)
      {
        mh_seq_free(mhs);
        rc = -1;
        goto out;
      }
      for (; first <= last; first++)
        mh_seq_set(mhs, first, flags);
    }
  }

  rc = 0;

out:
  FREE(&buf);
  mutt_file_fclose(&fp);
  return rc;
}

/**
 * mh_seq_changed - Has the mailbox changed
 * @param m Mailbox
 * @retval 1 mh_sequences last modification time is more recent than the last visit to this mailbox
 * @retval 0 modification time is older
 * @retval -1 Error
 */
int mh_seq_changed(struct Mailbox *m)
{
  char path[PATH_MAX];
  struct stat st = { 0 };

  if ((snprintf(path, sizeof(path), "%s/.mh_sequences", mailbox_path(m)) < sizeof(path)) &&
      (stat(path, &st) == 0))
  {
    return (mutt_file_stat_timespec_compare(&st, MUTT_STAT_MTIME, &m->last_visited) > 0);
  }
  return -1;
}
