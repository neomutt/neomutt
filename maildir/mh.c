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
#include <inttypes.h>
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
#include "maildir/lib.h"
#include "copy.h"
#include "edata.h"
#include "errno.h"
#include "mdata.h"
#include "mdemail.h"
#include "monitor.h"
#include "mutt_globals.h"
#include "mx.h"
#include "progress.h"
#ifdef USE_HCACHE
#include "hcache/lib.h"
#endif

/**
 * mh_mkstemp - Create a temporary file
 * @param[in]  m   Mailbox to create the file in
 * @param[out] fp  File handle
 * @param[out] tgt File name
 * @retval  0 Success
 * @retval -1 Failure
 */
int mh_mkstemp(struct Mailbox *m, FILE **fp, char **tgt)
{
  int fd;
  char path[PATH_MAX];

  mode_t omask = umask(mh_umask(m));
  while (true)
  {
    snprintf(path, sizeof(path), "%s/.neomutt-%s-%d-%" PRIu64, mailbox_path(m),
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
      *tgt = mutt_str_dup(path);
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
 * mh_check_empty - Is mailbox empty
 * @param path Mailbox to check
 * @retval 1 Mailbox is empty
 * @retval 0 Mailbox contains mail
 * @retval -1 Error
 */
int mh_check_empty(const char *path)
{
  struct dirent *de = NULL;
  int rc = 1; /* assume empty until we find a message */

  DIR *dp = opendir(path);
  if (!dp)
    return -1;
  while ((de = readdir(dp)))
  {
    if (mh_valid_message(de->d_name))
    {
      rc = 0;
      break;
    }
  }
  closedir(dp);

  return rc;
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
  if (C_MailCheckRecent && (mh_seq_changed(m) <= 0))
  {
    rc = 0;
    check_new = false;
  }

  if (!check_new)
    return 0;

  if (mh_seq_read(&mhs, mailbox_path(m)) < 0)
    return -1;

  m->msg_count = 0;
  m->msg_unread = 0;
  m->msg_flagged = 0;

  for (int i = mhs.max; i > 0; i--)
  {
    if ((mh_seq_check(&mhs, i) & MH_SEQ_FLAGGED))
      m->msg_flagged++;
    if (mh_seq_check(&mhs, i) & MH_SEQ_UNSEEN)
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

  mh_seq_free(&mhs);

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
void mh_update_maildir(struct MdEmail *md, struct MhSequences *mhs)
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
    MhSeqFlags flags = mh_seq_check(mhs, i);

    md->email->read = !(flags & MH_SEQ_UNSEEN);
    md->email->flagged = (flags & MH_SEQ_FLAGGED);
    md->email->replied = (flags & MH_SEQ_REPLIED);
  }
}

/**
 * mh_commit_msg - Commit a message to an MH folder
 * @param m   Mailbox
 * @param msg Message to commit
 * @param e   Email
 * @param updseq  If true, update the sequence number
 * @retval  0 Success
 * @retval -1 Failure
 */
int mh_commit_msg(struct Mailbox *m, struct Message *msg, struct Email *e, bool updseq)
{
  struct dirent *de = NULL;
  char *cp = NULL, *dep = NULL;
  unsigned int n, hi = 0;
  char path[PATH_MAX];
  char tmp[16];

  if (mutt_file_fsync_close(&msg->fp))
  {
    mutt_perror(_("Could not flush message to disk"));
    return -1;
  }

  DIR *dirp = opendir(mailbox_path(m));
  if (!dirp)
  {
    mutt_perror(mailbox_path(m));
    return -1;
  }

  /* figure out what the next message number is */
  while ((de = readdir(dirp)))
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
    if (*cp == '\0')
    {
      if (mutt_str_atoui(dep, &n) < 0)
        mutt_debug(LL_DEBUG2, "Invalid MH message number '%s'\n", dep);
      if (n > hi)
        hi = n;
    }
  }
  closedir(dirp);

  /* Now try to rename the file to the proper name.
   * Note: We may have to try multiple times, until we find a free slot.  */

  while (true)
  {
    hi++;
    snprintf(tmp, sizeof(tmp), "%u", hi);
    snprintf(path, sizeof(path), "%s/%s", mailbox_path(m), tmp);
    if (mutt_file_safe_rename(msg->path, path) == 0)
    {
      if (e)
        mutt_str_replace(&e->path, tmp);
      mutt_str_replace(&msg->committed_path, path);
      FREE(&msg->path);
      break;
    }
    else if (errno != EEXIST)
    {
      mutt_perror(mailbox_path(m));
      return -1;
    }
  }
  if (updseq)
  {
    mh_seq_add_one(m, hi, !msg->flags.read, msg->flags.flagged, msg->flags.replied);
  }
  return 0;
}

/**
 * mh_rewrite_message - Sync a message in an MH folder
 * @param m     Mailbox
 * @param msgno Index number
 * @retval  0 Success
 * @retval -1 Error
 *
 * This code is also used for attachment deletion in maildir folders.
 */
int mh_rewrite_message(struct Mailbox *m, int msgno)
{
  if (!m || !m->emails || (msgno >= m->msg_count))
    return -1;

  struct Email *e = m->emails[msgno];
  if (!e)
    return -1;

  bool restore = true;

  long old_body_offset = e->body->offset;
  long old_body_length = e->body->length;
  long old_hdr_lines = e->lines;

  struct Message *dest = mx_msg_open_new(m, e, MUTT_MSG_NO_FLAGS);
  if (!dest)
    return -1;

  int rc = mutt_copy_message(dest->fp, m, e, MUTT_CM_UPDATE, CH_UPDATE | CH_UPDATE_LEN, 0);
  if (rc == 0)
  {
    char oldpath[PATH_MAX];
    char partpath[PATH_MAX];
    snprintf(oldpath, sizeof(oldpath), "%s/%s", mailbox_path(m), e->path);
    mutt_str_copy(partpath, e->path, sizeof(partpath));

    if (m->type == MUTT_MAILDIR)
      rc = maildir_commit_message(m, dest, e);
    else
      rc = mh_commit_msg(m, dest, e, false);

    mx_msg_close(m, &dest);

    if (rc == 0)
    {
      unlink(oldpath);
      restore = false;
    }

    /* Try to move the new message to the old place.
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
     * lose flag modifications.  */

    if ((m->type == MUTT_MH) && (rc == 0))
    {
      char newpath[PATH_MAX];
      snprintf(newpath, sizeof(newpath), "%s/%s", mailbox_path(m), e->path);
      rc = mutt_file_safe_rename(newpath, oldpath);
      if (rc == 0)
        mutt_str_replace(&e->path, partpath);
    }
  }
  else
    mx_msg_close(m, &dest);

  if ((rc == -1) && restore)
  {
    e->body->offset = old_body_offset;
    e->body->length = old_body_length;
    e->lines = old_hdr_lines;
  }

  mutt_body_free(&e->body->parts);
  return rc;
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
 * mh_read_dir - Read a MH/maildir style mailbox
 * @param m      Mailbox
 * @param subdir NULL for MH mailboxes,
 *               otherwise the subdir of the maildir mailbox to read from
 * @retval  0 Success
 * @retval -1 Failure
 */
int mh_read_dir(struct Mailbox *m, const char *subdir)
{
  if (!m)
    return -1;

  struct MdEmail *md = NULL;
  struct MhSequences mhs = { 0 };
  struct MdEmail **last = NULL;
  struct Progress progress;

  if (m->verbose)
  {
    char msg[PATH_MAX];
    snprintf(msg, sizeof(msg), _("Scanning %s..."), mailbox_path(m));
    mutt_progress_init(&progress, msg, MUTT_PROGRESS_READ, 0);
  }

  struct MaildirMboxData *mdata = maildir_mdata_get(m);
  if (!mdata)
  {
    mdata = maildir_mdata_new();
    m->mdata = mdata;
    m->mdata_free = maildir_mdata_free;
  }

  maildir_update_mtime(m);

  md = NULL;
  last = &md;
  int count = 0;
  if (maildir_parse_dir(m, &last, subdir, &count, &progress) < 0)
    return -1;

  if (m->verbose)
  {
    char msg[PATH_MAX];
    snprintf(msg, sizeof(msg), _("Reading %s..."), mailbox_path(m));
    mutt_progress_init(&progress, msg, MUTT_PROGRESS_READ, count);
  }
  maildir_delayed_parsing(m, &md, &progress);

  if (m->type == MUTT_MH)
  {
    if (mh_seq_read(&mhs, mailbox_path(m)) < 0)
    {
      maildir_free(&md);
      return -1;
    }
    mh_update_maildir(md, &mhs);
    mh_seq_free(&mhs);
  }

  maildir_move_to_mailbox(m, &md);

  if (!mdata->mh_umask)
    mdata->mh_umask = mh_umask(m);

  return 0;
}

/**
 * mh_sync_mailbox_message - Save changes to the mailbox
 * @param m     Mailbox
 * @param msgno Index number
 * @param hc    Header cache handle
 * @retval  0 Success
 * @retval -1 Error
 */
int mh_sync_mailbox_message(struct Mailbox *m, int msgno, struct HeaderCache *hc)
{
  if (!m || !m->emails || (msgno >= m->msg_count))
    return -1;

  struct Email *e = m->emails[msgno];
  if (!e)
    return -1;

  if (e->deleted && ((m->type != MUTT_MAILDIR) || !C_MaildirTrash))
  {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", mailbox_path(m), e->path);
    if ((m->type == MUTT_MAILDIR) || (C_MhPurge && (m->type == MUTT_MH)))
    {
#ifdef USE_HCACHE
      if (hc)
      {
        const char *key = NULL;
        size_t keylen;
        if (m->type == MUTT_MH)
        {
          key = e->path;
          keylen = strlen(key);
        }
        else
        {
          key = e->path + 3;
          keylen = maildir_hcache_keylen(key);
        }
        mutt_hcache_delete_record(hc, key, keylen);
      }
#endif
      unlink(path);
    }
    else if (m->type == MUTT_MH)
    {
      /* MH just moves files out of the way when you delete them */
      if (*e->path != ',')
      {
        char tmp[PATH_MAX];
        snprintf(tmp, sizeof(tmp), "%s/,%s", mailbox_path(m), e->path);
        unlink(tmp);
        rename(path, tmp);
      }
    }
  }
  else if (e->changed || e->attach_del ||
           ((m->type == MUTT_MAILDIR) && (C_MaildirTrash || e->trash) &&
            (e->deleted != e->trash)))
  {
    if (m->type == MUTT_MAILDIR)
    {
      if (maildir_sync_message(m, msgno) == -1)
        return -1;
    }
    else
    {
      if (mh_sync_message(m, msgno) == -1)
        return -1;
    }
  }

#ifdef USE_HCACHE
  if (hc && e->changed)
  {
    const char *key = NULL;
    size_t keylen;
    if (m->type == MUTT_MH)
    {
      key = e->path;
      keylen = strlen(key);
    }
    else
    {
      key = e->path + 3;
      keylen = maildir_hcache_keylen(key);
    }
    mutt_hcache_store(hc, key, keylen, e, 0);
  }
#endif

  return 0;
}

/**
 * mh_cmp_path - Compare two Maildirs by path
 * @param a First  Maildir
 * @param b Second Maildir
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int mh_cmp_path(struct MdEmail *a, struct MdEmail *b)
{
  return strcmp(a->email->path, b->email->path);
}

/**
 * maildir_mh_open_message - Open a Maildir or MH message
 * @param m          Mailbox
 * @param msg        Message to open
 * @param msgno      Index number
 * @param is_maildir true, if a Maildir
 * @retval  0 Success
 * @retval -1 Failure
 */
int maildir_mh_open_message(struct Mailbox *m, struct Message *msg, int msgno, bool is_maildir)
{
  if (!m || !m->emails || (msgno >= m->msg_count))
    return -1;

  struct Email *e = m->emails[msgno];
  if (!e)
    return -1;

  char path[PATH_MAX];

  snprintf(path, sizeof(path), "%s/%s", mailbox_path(m), e->path);

  msg->fp = fopen(path, "r");
  if (!msg->fp && (errno == ENOENT) && is_maildir)
    msg->fp = maildir_open_find_message(mailbox_path(m), e->path, NULL);

  if (!msg->fp)
  {
    mutt_perror(path);
    mutt_debug(LL_DEBUG1, "fopen: %s: %s (errno %d)\n", path, strerror(errno), errno);
    return -1;
  }

  return 0;
}

/**
 * mh_msg_save_hcache - Save message to the header cache - Implements MxOps::msg_save_hcache()
 */
int mh_msg_save_hcache(struct Mailbox *m, struct Email *e)
{
  int rc = 0;
#ifdef USE_HCACHE
  struct HeaderCache *hc = mutt_hcache_open(C_HeaderCache, mailbox_path(m), NULL);
  rc = mutt_hcache_store(hc, e->path, strlen(e->path), e, 0);
  mutt_hcache_close(hc);
#endif
  return rc;
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
  char buf[PATH_MAX];
  struct stat st, st_cur;
  bool modified = false, occult = false, flags_changed = false;
  int num_new = 0;
  struct MdEmail *md = NULL, *p = NULL;
  struct MdEmail **last = NULL;
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

  if (mh_seq_read(&mhs, mailbox_path(m)) < 0)
    return -1;
  mh_update_maildir(md, &mhs);
  mh_seq_free(&mhs);

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
 * mh_mbox_sync - Save changes to the Mailbox - Implements MxOps::mbox_sync()
 * @retval #MUTT_REOPENED  mailbox has been externally modified
 * @retval #MUTT_NEW_MAIL  new mail has arrived
 * @retval  0 Success
 * @retval -1 Error
 *
 * @note The flag retvals come from a call to a backend sync function
 */
int mh_mbox_sync(struct Mailbox *m)
{
  int i, j;
  struct HeaderCache *hc = NULL;
  struct Progress progress;
  int check;

  if (m->type == MUTT_MH)
    check = mh_mbox_check(m);
  else
    check = maildir_mbox_check(m);

  if (check < 0)
    return check;

#ifdef USE_HCACHE
  if ((m->type == MUTT_MAILDIR) || (m->type == MUTT_MH))
    hc = mutt_hcache_open(C_HeaderCache, mailbox_path(m), NULL);
#endif

  if (m->verbose)
  {
    char msg[PATH_MAX];
    snprintf(msg, sizeof(msg), _("Writing %s..."), mailbox_path(m));
    mutt_progress_init(&progress, msg, MUTT_PROGRESS_WRITE, m->msg_count);
  }

  for (i = 0; i < m->msg_count; i++)
  {
    if (m->verbose)
      mutt_progress_update(&progress, i, -1);

    if (mh_sync_mailbox_message(m, i, hc) == -1)
      goto err;
  }

#ifdef USE_HCACHE
  if ((m->type == MUTT_MAILDIR) || (m->type == MUTT_MH))
    mutt_hcache_close(hc);
#endif

  if (m->type == MUTT_MH)
    mh_seq_update(m);

  /* XXX race condition? */

  maildir_update_mtime(m);

  /* adjust indices */

  if (m->msg_deleted)
  {
    for (i = 0, j = 0; i < m->msg_count; i++)
    {
      struct Email *e = m->emails[i];
      if (!e)
        break;

      if (!e->deleted || ((m->type == MUTT_MAILDIR) && C_MaildirTrash))
        e->index = j++;
    }
  }

  return check;

err:
#ifdef USE_HCACHE
  if ((m->type == MUTT_MAILDIR) || (m->type == MUTT_MH))
    mutt_hcache_close(hc);
#endif
  return -1;
}

/**
 * mh_mbox_close - Close a Mailbox - Implements MxOps::mbox_close()
 * @retval 0 Always
 */
int mh_mbox_close(struct Mailbox *m)
{
  return 0;
}

/**
 * mh_msg_open - Open an email message in a Mailbox - Implements MxOps::msg_open()
 */
static int mh_msg_open(struct Mailbox *m, struct Message *msg, int msgno)
{
  return maildir_mh_open_message(m, msg, msgno, false);
}

/**
 * mh_msg_open_new - Open a new message in a Mailbox - Implements MxOps::msg_open_new()
 *
 * Open a new (temporary) message in an MH folder.
 */
static int mh_msg_open_new(struct Mailbox *m, struct Message *msg, const struct Email *e)
{
  return mh_mkstemp(m, &msg->fp, &msg->path);
}

/**
 * mh_msg_commit - Save changes to an email - Implements MxOps::msg_commit()
 */
static int mh_msg_commit(struct Mailbox *m, struct Message *msg)
{
  return mh_commit_msg(m, msg, NULL, true);
}

/**
 * mh_msg_close - Close an email - Implements MxOps::msg_close()
 *
 * @note May also return EOF Failure, see errno
 */
int mh_msg_close(struct Mailbox *m, struct Message *msg)
{
  return mutt_file_fclose(&msg->fp);
}

/**
 * mh_path_probe - Is this an mh Mailbox? - Implements MxOps::path_probe()
 */
static enum MailboxType mh_path_probe(const char *path, const struct stat *st)
{
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
  .path_is_empty    = mh_check_empty,
};
// clang-format on
