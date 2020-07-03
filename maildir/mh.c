/**
 * @file
 * MH local mailbox type
 *
 * @authors
 * Copyright (C) 1996-2002,2007,2009 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2005 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
 * @page maildir_mh MH local mailbox type
 *
 * MH local mailbox type
 */

#include "config.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "private.h"
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "lib.h"
#include "errno.h"
#include "monitor.h"
#include "mutt_globals.h"
#include "mx.h"

/**
 * mhs_alloc - Allocate more memory for sequences
 * @param mhs Existing sequences
 * @param i   Number required
 *
 * @note Memory is allocated in blocks of 128.
 */
static void mhs_alloc(struct MhSequences *mhs, int i)
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
 * mhs_sequences_free - Free some sequences
 * @param mhs Sequences to free
 */
void mhs_sequences_free(struct MhSequences *mhs)
{
  FREE(&mhs->flags);
}

/**
 * mhs_check - Get the flags for a given sequence
 * @param mhs Sequences
 * @param i   Index number required
 * @retval num Flags, see #MhSeqFlags
 */
MhSeqFlags mhs_check(struct MhSequences *mhs, int i)
{
  if (!mhs->flags || (i > mhs->max))
    return 0;
  return mhs->flags[i];
}

/**
 * mhs_set - Set a flag for a given sequence
 * @param mhs Sequences
 * @param i   Index number
 * @param f   Flags, see #MhSeqFlags
 * @retval num Resulting flags, see #MhSeqFlags
 */
MhSeqFlags mhs_set(struct MhSequences *mhs, int i, MhSeqFlags f)
{
  mhs_alloc(mhs, i);
  mhs->flags[i] |= f;
  return mhs->flags[i];
}

/**
 * mhs_write_one_sequence - Write a flag sequence to a file
 * @param fp  File to write to
 * @param mhs Sequence list
 * @param f   Flag, see #MhSeqFlags
 * @param tag string tag, e.g. "unseen"
 */
static void mhs_write_one_sequence(FILE *fp, struct MhSequences *mhs,
                                   MhSeqFlags f, const char *tag)
{
  fprintf(fp, "%s:", tag);

  int first = -1;
  int last = -1;

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
 * @param m Mailbox
 *
 * XXX we don't currently remove deleted messages from sequences we don't know.
 * Should we?
 */
void mh_update_sequences(struct Mailbox *m)
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

  snprintf(seq_unseen, sizeof(seq_unseen), "%s:", NONULL(C_MhSeqUnseen));
  snprintf(seq_replied, sizeof(seq_replied), "%s:", NONULL(C_MhSeqReplied));
  snprintf(seq_flagged, sizeof(seq_flagged), "%s:", NONULL(C_MhSeqFlagged));

  FILE *fp_new = NULL;
  if (mh_mkstemp(m, &fp_new, &tmpfname) != 0)
  {
    /* error message? */
    return;
  }

  snprintf(sequences, sizeof(sequences), "%s/.mh_sequences", mailbox_path(m));

  /* first, copy unknown sequences */
  FILE *fp_old = fopen(sequences, "r");
  if (fp_old)
  {
    while ((buf = mutt_file_read_line(buf, &s, fp_old, NULL, 0)))
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

    if (mutt_str_atoi(p, &seq_num) < 0)
      continue;

    if (!e->read)
    {
      mhs_set(&mhs, seq_num, MH_SEQ_UNSEEN);
      unseen++;
    }
    if (e->flagged)
    {
      mhs_set(&mhs, seq_num, MH_SEQ_FLAGGED);
      flagged++;
    }
    if (e->replied)
    {
      mhs_set(&mhs, seq_num, MH_SEQ_REPLIED);
      replied++;
    }
  }

  /* write out the new sequences */
  if (unseen)
    mhs_write_one_sequence(fp_new, &mhs, MH_SEQ_UNSEEN, NONULL(C_MhSeqUnseen));
  if (flagged)
    mhs_write_one_sequence(fp_new, &mhs, MH_SEQ_FLAGGED, NONULL(C_MhSeqFlagged));
  if (replied)
    mhs_write_one_sequence(fp_new, &mhs, MH_SEQ_REPLIED, NONULL(C_MhSeqReplied));

  mhs_sequences_free(&mhs);

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
 * mh_read_token - Parse a number, or number range
 * @param t     String to parse
 * @param first First number
 * @param last  Last number (if a range, first number if not)
 * @retval  0 Success
 * @retval -1 Error
 */
static int mh_read_token(char *t, int *first, int *last)
{
  char *p = strchr(t, '-');
  if (p)
  {
    *p++ = '\0';
    if ((mutt_str_atoi(t, first) < 0) || (mutt_str_atoi(p, last) < 0))
      return -1;
  }
  else
  {
    if (mutt_str_atoi(t, first) < 0)
      return -1;
    *last = *first;
  }
  return 0;
}

/**
 * mh_read_sequences - Read a set of MH sequences
 * @param mhs  Existing sequences
 * @param path File to read from
 * @retval  0 Success
 * @retval -1 Error
 */
int mh_read_sequences(struct MhSequences *mhs, const char *path)
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

  while ((buf = mutt_file_read_line(buf, &sz, fp, NULL, 0)))
  {
    char *t = strtok(buf, " \t:");
    if (!t)
      continue;

    if (mutt_str_equal(t, C_MhSeqUnseen))
      flags = MH_SEQ_UNSEEN;
    else if (mutt_str_equal(t, C_MhSeqFlagged))
      flags = MH_SEQ_FLAGGED;
    else if (mutt_str_equal(t, C_MhSeqReplied))
      flags = MH_SEQ_REPLIED;
    else /* unknown sequence */
      continue;

    while ((t = strtok(NULL, " \t:")))
    {
      if (mh_read_token(t, &first, &last) < 0)
      {
        mhs_sequences_free(mhs);
        rc = -1;
        goto out;
      }
      for (; first <= last; first++)
        mhs_set(mhs, first, flags);
    }
  }

  rc = 0;

out:
  FREE(&buf);
  mutt_file_fclose(&fp);
  return rc;
}

/**
 * mh_sequences_changed - Has the mailbox changed
 * @param m Mailbox
 * @retval 1 mh_sequences last modification time is more recent than the last visit to this mailbox
 * @retval 0 modification time is older
 * @retval -1 Error
 */
static int mh_sequences_changed(struct Mailbox *m)
{
  char path[PATH_MAX];
  struct stat sb;

  if ((snprintf(path, sizeof(path), "%s/.mh_sequences", mailbox_path(m)) < sizeof(path)) &&
      (stat(path, &sb) == 0))
  {
    return (mutt_file_stat_timespec_compare(&sb, MUTT_STAT_MTIME, &m->last_visited) > 0);
  }
  return -1;
}

/**
 * mh_already_notified - Has the message changed
 * @param m     Mailbox
 * @param msgno Message number
 * @retval 1 Modification time on the message file is older than the last visit to this mailbox
 * @retval 0 Modification time on the message file is newer
 * @retval -1 Error
 */
static int mh_already_notified(struct Mailbox *m, int msgno)
{
  char path[PATH_MAX];
  struct stat sb;

  if ((snprintf(path, sizeof(path), "%s/%d", mailbox_path(m), msgno) < sizeof(path)) &&
      (stat(path, &sb) == 0))
  {
    return (mutt_file_stat_timespec_compare(&sb, MUTT_STAT_MTIME, &m->last_visited) <= 0);
  }
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
bool mh_valid_message(const char *s)
{
  for (; *s; s++)
  {
    if (!isdigit((unsigned char) *s))
      return false;
  }
  return true;
}

/**
 * mh_mbox_check_stats - Check the Mailbox statistics - Implements MxOps::mbox_check_stats()
 */
static int mh_mbox_check_stats(struct Mailbox *m, int flags)
{
  struct MhSequences mhs = { 0 };
  bool check_new = true;
  int rc = -1;
  DIR *dirp = NULL;
  struct dirent *de = NULL;

  /* when $mail_check_recent is set and the .mh_sequences file hasn't changed
   * since the last m visit, there is no "new mail" */
  if (C_MailCheckRecent && (mh_sequences_changed(m) <= 0))
  {
    rc = 0;
    check_new = false;
  }

  if (!check_new)
    return 0;

  if (mh_read_sequences(&mhs, mailbox_path(m)) < 0)
    return -1;

  m->msg_count = 0;
  m->msg_unread = 0;
  m->msg_flagged = 0;

  for (int i = mhs.max; i > 0; i--)
  {
    if ((mhs_check(&mhs, i) & MH_SEQ_FLAGGED))
      m->msg_flagged++;
    if (mhs_check(&mhs, i) & MH_SEQ_UNSEEN)
    {
      m->msg_unread++;
      if (check_new)
      {
        /* if the first unseen message we encounter was in the m during the
         * last visit, don't notify about it */
        if (!C_MailCheckRecent || (mh_already_notified(m, i) == 0))
        {
          m->has_new = true;
          rc = 1;
        }
        /* Because we are traversing from high to low, we can stop
         * checking for new mail after the first unseen message.
         * Whether it resulted in "new mail" or not. */
        check_new = false;
      }
    }
  }

  mhs_sequences_free(&mhs);

  dirp = opendir(mailbox_path(m));
  if (dirp)
  {
    while ((de = readdir(dirp)))
    {
      if (*de->d_name == '.')
        continue;
      if (mh_valid_message(de->d_name))
        m->msg_count++;
    }
    closedir(dirp);
  }

  return rc;
}

/**
 * mh_update_maildir - Update our record of flags
 * @param md  Maildir to update
 * @param mhs Sequences
 */
void mh_update_maildir(struct Maildir *md, struct MhSequences *mhs)
{
  int i;

  for (; md; md = md->next)
  {
    char *p = strrchr(md->email->path, '/');
    if (p)
      p++;
    else
      p = md->email->path;

    if (mutt_str_atoi(p, &i) < 0)
      continue;
    MhSeqFlags flags = mhs_check(mhs, i);

    md->email->read = (flags & MH_SEQ_UNSEEN) ? false : true;
    md->email->flagged = (flags & MH_SEQ_FLAGGED) ? true : false;
    md->email->replied = (flags & MH_SEQ_REPLIED) ? true : false;
  }
}

/**
 * mh_sync_message - Sync an email to an MH folder
 * @param m     Mailbox
 * @param msgno Index number
 * @retval  0 Success
 * @retval -1 Error
 */
int mh_sync_message(struct Mailbox *m, int msgno)
{
  if (!m || !m->emails)
    return -1;

  struct Email *e = m->emails[msgno];
  if (!e)
    return -1;

  /* TODO: why the e->env check? */
  if (e->attach_del || (e->env && e->env->changed))
  {
    if (mh_rewrite_message(m, msgno) != 0)
      return -1;
    /* TODO: why the env check? */
    if (e->env)
      e->env->changed = 0;
  }

  return 0;
}

/**
 * mh_mbox_open - Open a Mailbox - Implements MxOps::mbox_open()
 */
static int mh_mbox_open(struct Mailbox *m)
{
  return mh_read_dir(m, NULL);
}

/**
 * mh_mbox_open_append - Open a Mailbox for appending - Implements MxOps::mbox_open_append()
 */
static int mh_mbox_open_append(struct Mailbox *m, OpenMailboxFlags flags)
{
  if (!m)
    return -1;

  if (!(flags & (MUTT_APPENDNEW | MUTT_NEWFOLDER)))
    return 0;

  if (mutt_file_mkdir(mailbox_path(m), S_IRWXU))
  {
    mutt_perror(mailbox_path(m));
    return -1;
  }

  char tmp[PATH_MAX];
  snprintf(tmp, sizeof(tmp), "%s/.mh_sequences", mailbox_path(m));
  const int i = creat(tmp, S_IRWXU);
  if (i == -1)
  {
    mutt_perror(tmp);
    rmdir(mailbox_path(m));
    return -1;
  }
  close(i);

  return 0;
}

/**
 * mh_mbox_check - Check for new mail - Implements MxOps::mbox_check()
 *
 * This function handles arrival of new mail and reopening of mh/maildir
 * folders. Things are getting rather complex because we don't have a
 * well-defined "mailbox order", so the tricks from mbox.c and mx.c won't work
 * here.
 *
 * Don't change this code unless you _really_ understand what happens.
 */
int mh_mbox_check(struct Mailbox *m)
{
  if (!m)
    return -1;

  char buf[PATH_MAX];
  struct stat st, st_cur;
  bool modified = false, occult = false, flags_changed = false;
  int num_new = 0;
  struct Maildir *md = NULL, *p = NULL;
  struct Maildir **last = NULL;
  struct MhSequences mhs = { 0 };
  int count = 0;
  struct HashTable *fnames = NULL;
  struct MaildirMboxData *mdata = maildir_mdata_get(m);

  if (!C_CheckNew)
    return 0;

  mutt_str_copy(buf, mailbox_path(m), sizeof(buf));
  if (stat(buf, &st) == -1)
    return -1;

  /* create .mh_sequences when there isn't one. */
  snprintf(buf, sizeof(buf), "%s/.mh_sequences", mailbox_path(m));
  int rc = stat(buf, &st_cur);
  if ((rc == -1) && (errno == ENOENT))
  {
    char *tmp = NULL;
    FILE *fp = NULL;

    if (mh_mkstemp(m, &fp, &tmp) == 0)
    {
      mutt_file_fclose(&fp);
      if (mutt_file_safe_rename(tmp, buf) == -1)
        unlink(tmp);
      FREE(&tmp);
    }
  }

  if ((rc == -1) && (stat(buf, &st_cur) == -1))
    modified = true;

  if ((mutt_file_stat_timespec_compare(&st, MUTT_STAT_MTIME, &m->mtime) > 0) ||
      (mutt_file_stat_timespec_compare(&st_cur, MUTT_STAT_MTIME, &mdata->mtime_cur) > 0))
  {
    modified = true;
  }

  if (!modified)
    return 0;

    /* Update the modification times on the mailbox.
     *
     * The monitor code notices changes in the open mailbox too quickly.
     * In practice, this sometimes leads to all the new messages not being
     * noticed during the SAME group of mtime stat updates.  To work around
     * the problem, don't update the stat times for a monitor caused check. */
#ifdef USE_INOTIFY
  if (MonitorContextChanged)
    MonitorContextChanged = false;
  else
#endif
  {
    mutt_file_get_stat_timespec(&mdata->mtime_cur, &st_cur, MUTT_STAT_MTIME);
    mutt_file_get_stat_timespec(&m->mtime, &st, MUTT_STAT_MTIME);
  }

  md = NULL;
  last = &md;

  maildir_parse_dir(m, &last, NULL, &count, NULL);
  maildir_delayed_parsing(m, &md, NULL);

  if (mh_read_sequences(&mhs, mailbox_path(m)) < 0)
    return -1;
  mh_update_maildir(md, &mhs);
  mhs_sequences_free(&mhs);

  /* check for modifications and adjust flags */
  fnames = mutt_hash_new(count, MUTT_HASH_NO_FLAGS);

  for (p = md; p; p = p->next)
  {
    /* the hash key must survive past the header, which is freed below. */
    p->canon_fname = mutt_str_dup(p->email->path);
    mutt_hash_insert(fnames, p->canon_fname, p);
  }

  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;

    e->active = false;

    p = mutt_hash_find(fnames, e->path);
    if (p && p->email && email_cmp_strict(e, p->email))
    {
      e->active = true;
      /* found the right message */
      if (!e->changed)
        if (maildir_update_flags(m, e, p->email))
          flags_changed = true;

      email_free(&p->email);
    }
    else /* message has disappeared */
      occult = true;
  }

  /* destroy the file name hash */

  mutt_hash_free(&fnames);

  /* If we didn't just get new mail, update the tables. */
  if (occult)
    mailbox_changed(m, NT_MAILBOX_RESORT);

  /* Incorporate new messages */
  num_new = maildir_move_to_mailbox(m, &md);
  if (num_new > 0)
  {
    mailbox_changed(m, NT_MAILBOX_INVALID);
    m->changed = true;
  }

  if (occult)
    return MUTT_REOPENED;
  if (num_new > 0)
    return MUTT_NEW_MAIL;
  if (flags_changed)
    return MUTT_FLAGS;
  return 0;
}

/**
 * mh_msg_open - Open an email message in a Mailbox - Implements MxOps::msg_open()
 */
static int mh_msg_open(struct Mailbox *m, struct Message *msg, int msgno)
{
  if (!m)
    return -1;
  return maildir_mh_open_message(m, msg, msgno, false);
}

/**
 * mh_msg_open_new - Open a new message in a Mailbox - Implements MxOps::msg_open_new()
 *
 * Open a new (temporary) message in an MH folder.
 */
static int mh_msg_open_new(struct Mailbox *m, struct Message *msg, struct Email *e)
{
  if (!m || !msg)
    return -1;
  return mh_mkstemp(m, &msg->fp, &msg->path);
}

/**
 * mh_msg_commit - Save changes to an email - Implements MxOps::msg_commit()
 */
static int mh_msg_commit(struct Mailbox *m, struct Message *msg)
{
  if (!m)
    return -1;

  return mh_commit_msg(m, msg, NULL, true);
}

/**
 * mh_path_probe - Is this an mh Mailbox? - Implements MxOps::path_probe()
 */
static enum MailboxType mh_path_probe(const char *path, const struct stat *st)
{
  if (!path)
    return MUTT_UNKNOWN;

  if (!st || !S_ISDIR(st->st_mode))
    return MUTT_UNKNOWN;

  char tmp[PATH_MAX];

  snprintf(tmp, sizeof(tmp), "%s/.mh_sequences", path);
  if (access(tmp, F_OK) == 0)
    return MUTT_MH;

  snprintf(tmp, sizeof(tmp), "%s/.xmhcache", path);
  if (access(tmp, F_OK) == 0)
    return MUTT_MH;

  snprintf(tmp, sizeof(tmp), "%s/.mew_cache", path);
  if (access(tmp, F_OK) == 0)
    return MUTT_MH;

  snprintf(tmp, sizeof(tmp), "%s/.mew-cache", path);
  if (access(tmp, F_OK) == 0)
    return MUTT_MH;

  snprintf(tmp, sizeof(tmp), "%s/.sylpheed_cache", path);
  if (access(tmp, F_OK) == 0)
    return MUTT_MH;

  /* ok, this isn't an mh folder, but mh mode can be used to read
   * Usenet news from the spool.  */

  snprintf(tmp, sizeof(tmp), "%s/.overview", path);
  if (access(tmp, F_OK) == 0)
    return MUTT_MH;

  return MUTT_UNKNOWN;
}

// clang-format off
/**
 * MxMhOps - MH Mailbox - Implements ::MxOps
 */
struct MxOps MxMhOps = {
  .type            = MUTT_MH,
  .name             = "mh",
  .is_local         = true,
  .ac_find          = maildir_ac_find,
  .ac_add           = maildir_ac_add,
  .mbox_open        = mh_mbox_open,
  .mbox_open_append = mh_mbox_open_append,
  .mbox_check       = mh_mbox_check,
  .mbox_check_stats = mh_mbox_check_stats,
  .mbox_sync        = mh_mbox_sync,
  .mbox_close       = mh_mbox_close,
  .msg_open         = mh_msg_open,
  .msg_open_new     = mh_msg_open_new,
  .msg_commit       = mh_msg_commit,
  .msg_close        = mh_msg_close,
  .msg_padding_size = NULL,
  .msg_save_hcache  = mh_msg_save_hcache,
  .tags_edit        = NULL,
  .tags_commit      = NULL,
  .path_probe       = mh_path_probe,
  .path_canon       = maildir_path_canon,
  .path_pretty      = maildir_path_pretty,
  .path_parent      = maildir_path_parent,
};
// clang-format on
