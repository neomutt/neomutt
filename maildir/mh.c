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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "lib.h"
#include "progress/lib.h"
#include "copy.h"
#include "edata.h"
#include "errno.h"
#include "mdata.h"
#include "mdemail.h"
#include "monitor.h"
#include "mutt_globals.h"
#include "mx.h"
#include "sequence.h"
#ifdef USE_HCACHE
#include "hcache/lib.h"
#endif

/**
 * mh_mkstemp - Create a temporary file
 * @param[in]  m   Mailbox to create the file in
 * @param[out] fp  File handle
 * @param[out] tgt File name
 * @retval true Success
 * @retval false Failure
 */
bool mh_mkstemp(struct Mailbox *m, FILE **fp, char **tgt)
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
        return false;
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
    return false;
  }

  return true;
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
static enum MxStatus mh_mbox_check_stats(struct Mailbox *m, uint8_t flags)
{
  struct MhSequences mhs = { 0 };
  DIR *dirp = NULL;
  struct dirent *de = NULL;

  /* when $mail_check_recent is set and the .mh_sequences file hasn't changed
   * since the last m visit, there is no "new mail" */
  const bool c_mail_check_recent =
      cs_subset_bool(NeoMutt->sub, "mail_check_recent");
  if (c_mail_check_recent && (mh_seq_changed(m) <= 0))
  {
    return MX_STATUS_OK;
  }

  if (mh_seq_read(&mhs, mailbox_path(m)) < 0)
    return MX_STATUS_ERROR;

  m->msg_count = 0;
  m->msg_unread = 0;
  m->msg_flagged = 0;

  enum MxStatus rc = MX_STATUS_OK;
  bool check_new = true;
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
        if (!c_mail_check_recent || (mh_already_notified(m, i) == 0))
        {
          m->has_new = true;
          rc = MX_STATUS_NEW_MAIL;
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
 * @param mda Maildir array to update
 * @param mhs Sequences
 */
void mh_update_maildir(struct MdEmailArray *mda, struct MhSequences *mhs)
{
  struct MdEmail *md = NULL;
  struct MdEmail **mdp = NULL;
  ARRAY_FOREACH(mdp, mda)
  {
    md = *mdp;
    char *p = strrchr(md->email->path, '/');
    if (p)
      p++;
    else
      p = md->email->path;

    int i = 0;
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

  int rc = mutt_copy_message(dest->fp, m, e, dest, MUTT_CM_UPDATE,
                             CH_UPDATE | CH_UPDATE_LEN, 0);
  if (rc == 0)
  {
    char oldpath[PATH_MAX];
    char partpath[PATH_MAX];
    snprintf(oldpath, sizeof(oldpath), "%s/%s", mailbox_path(m), e->path);
    mutt_str_copy(partpath, e->path, sizeof(partpath));

    rc = mh_commit_msg(m, dest, e, false);

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
    if (rc == 0)
    {
      char newpath[PATH_MAX];
      snprintf(newpath, sizeof(newpath), "%s/%s", mailbox_path(m), e->path);
      rc = mutt_file_safe_rename(newpath, oldpath);
      if (rc == 0)
        mutt_str_replace(&e->path, partpath);
    }
  }
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
 * mh_update_mtime - Update our record of the Maildir modification time
 * @param m Mailbox
 */
void mh_update_mtime(struct Mailbox *m)
{
  char buf[PATH_MAX];
  struct stat st;
  struct MaildirMboxData *mdata = maildir_mdata_get(m);

  snprintf(buf, sizeof(buf), "%s/.mh_sequences", mailbox_path(m));
  if (stat(buf, &st) == 0)
    mutt_file_get_stat_timespec(&mdata->mtime_cur, &st, MUTT_STAT_MTIME);

  mutt_str_copy(buf, mailbox_path(m), sizeof(buf));

  if (stat(buf, &st) == 0)
    mutt_file_get_stat_timespec(&m->mtime, &st, MUTT_STAT_MTIME);
}

/**
 * mh_parse_dir - Read a Maildir mailbox
 * @param[in]  m        Mailbox
 * @param[out] mda      Array for results
 * @param[in]  progress Progress bar
 * @retval  0 Success
 * @retval -1 Error
 * @retval -2 Aborted
 */
int mh_parse_dir(struct Mailbox *m, struct MdEmailArray *mda, struct Progress *progress)
{
  struct dirent *de = NULL;
  int rc = 0;
  struct MdEmail *entry = NULL;
  struct Email *e = NULL;

  struct Buffer *buf = mutt_buffer_pool_get();
  mutt_buffer_strcpy(buf, mailbox_path(m));

  DIR *dirp = opendir(mutt_buffer_string(buf));
  if (!dirp)
  {
    rc = -1;
    goto cleanup;
  }

  while (((de = readdir(dirp))) && !SigInt)
  {
    if (!mh_valid_message(de->d_name))
      continue;

    /* FOO - really ignore the return value? */
    mutt_debug(LL_DEBUG2, "queueing %s\n", de->d_name);

    e = email_new();
    e->edata = maildir_edata_new();
    e->edata_free = maildir_edata_free;

    if (m->verbose && progress)
      progress_update(progress, ARRAY_SIZE(mda) + 1, -1);

    e->path = mutt_str_dup(de->d_name);

    entry = maildir_entry_new();
    entry->email = e;
    ARRAY_ADD(mda, entry);
  }

  closedir(dirp);

  if (SigInt)
  {
    SigInt = false;
    return -2; /* action aborted */
  }

cleanup:
  mutt_buffer_pool_release(&buf);

  return rc;
}

/**
 * mh_cmp_path - Compare two Maildirs by path - Implements ::sort_t
 */
int mh_cmp_path(const void *a, const void *b)
{
  struct MdEmail const *const *pa = (struct MdEmail const *const *) a;
  struct MdEmail const *const *pb = (struct MdEmail const *const *) b;
  return strcmp((*pa)->email->path, (*pb)->email->path);
}

/**
 * mh_parse_message - Actually parse an MH message
 * @param fname  Message filename
 * @param e      Email to populate (OPTIONAL)
 * @retval ptr Populated Email
 *
 * This may also be used to fill out a fake header structure generated by lazy
 * maildir parsing.
 */
struct Email *mh_parse_message(const char *fname, struct Email *e)
{
  FILE *fp = fopen(fname, "r");
  if (!fp)
    return NULL;

  if (!e)
  {
    e = email_new();
    e->edata = maildir_edata_new();
    e->edata_free = maildir_edata_free;
  }
  e->env = mutt_rfc822_read_header(fp, e, false, false);

  struct stat st;
  fstat(fileno(fp), &st);

  if (!e->received)
    e->received = e->date_sent;

  /* always update the length since we have fresh information available. */
  e->body->length = st.st_size - e->body->offset;
  e->index = -1;

  mutt_file_fclose(&fp);
  return e;
}

/**
 * mh_delayed_parsing - This function does the second parsing pass
 * @param[in]  m   Mailbox
 * @param[out] mda Maildir array to parse
 * @param[in]  progress Progress bar
 */
void mh_delayed_parsing(struct Mailbox *m, struct MdEmailArray *mda, struct Progress *progress)
{
  char fn[PATH_MAX];

#ifdef USE_HCACHE
  const char *const c_header_cache =
      cs_subset_path(NeoMutt->sub, "header_cache");
  struct HeaderCache *hc = mutt_hcache_open(c_header_cache, mailbox_path(m), NULL);
#endif

  struct MdEmail *md = NULL;
  struct MdEmail **mdp = NULL;
  ARRAY_FOREACH(mdp, mda)
  {
    md = *mdp;
    if (!md || !md->email || md->header_parsed)
      continue;

    if (m->verbose && progress)
      progress_update(progress, ARRAY_FOREACH_IDX, -1);

    snprintf(fn, sizeof(fn), "%s/%s", mailbox_path(m), md->email->path);

#ifdef USE_HCACHE
    struct stat lastchanged = { 0 };
    int rc = 0;
    const bool c_maildir_header_cache_verify =
        cs_subset_bool(NeoMutt->sub, "maildir_header_cache_verify");
    if (c_maildir_header_cache_verify)
    {
      rc = stat(fn, &lastchanged);
    }

    const char *key = md->email->path;
    size_t keylen = strlen(key);
    struct HCacheEntry hce = mutt_hcache_fetch(hc, key, keylen, 0);

    if (hce.email && (rc == 0) && (lastchanged.st_mtime <= hce.uidvalidity))
    {
      hce.email->edata = maildir_edata_new();
      hce.email->edata_free = maildir_edata_free;
      hce.email->old = md->email->old;
      hce.email->path = mutt_str_dup(md->email->path);
      email_free(&md->email);
      md->email = hce.email;
    }
    else
#endif
    {
      if (mh_parse_message(fn, md->email))
      {
        md->header_parsed = true;
#ifdef USE_HCACHE
        key = md->email->path;
        keylen = strlen(key);
        mutt_hcache_store(hc, key, keylen, md->email, 0);
#endif
      }
      else
        email_free(&md->email);
    }
  }
#ifdef USE_HCACHE
  mutt_hcache_close(hc);
#endif

  const short c_sort = cs_subset_sort(NeoMutt->sub, "sort");
  if (m && mda && (ARRAY_SIZE(mda) > 0) && (c_sort == SORT_ORDER))
  {
    mutt_debug(LL_DEBUG3, "maildir: sorting %s into natural order\n", mailbox_path(m));
    ARRAY_SORT(mda, mh_cmp_path);
  }
}

/**
 * mh_read_dir - Read an MH mailbox
 * @param m Mailbox
 * @retval true Success
 * @retval false Error
 */
static bool mh_read_dir(struct Mailbox *m)
{
  if (!m)
    return false;

  struct MhSequences mhs = { 0 };
  struct Progress *progress = NULL;

  if (m->verbose)
  {
    char msg[PATH_MAX];
    snprintf(msg, sizeof(msg), _("Scanning %s..."), mailbox_path(m));
    progress = progress_new(msg, MUTT_PROGRESS_READ, 0);
  }

  struct MaildirMboxData *mdata = maildir_mdata_get(m);
  if (!mdata)
  {
    mdata = maildir_mdata_new();
    m->mdata = mdata;
    m->mdata_free = maildir_mdata_free;
  }

  mh_update_mtime(m);

  struct MdEmailArray mda = ARRAY_HEAD_INITIALIZER;
  int rc = mh_parse_dir(m, &mda, progress);
  progress_free(&progress);
  if (rc < 0)
    return false;

  if (m->verbose)
  {
    char msg[PATH_MAX];
    snprintf(msg, sizeof(msg), _("Reading %s..."), mailbox_path(m));
    progress = progress_new(msg, MUTT_PROGRESS_READ, ARRAY_SIZE(&mda));
  }
  mh_delayed_parsing(m, &mda, progress);
  progress_free(&progress);

  if (mh_seq_read(&mhs, mailbox_path(m)) < 0)
  {
    maildirarray_clear(&mda);
    return false;
  }
  mh_update_maildir(&mda, &mhs);
  mh_seq_free(&mhs);

  maildir_move_to_mailbox(m, &mda);

  if (!mdata->mh_umask)
    mdata->mh_umask = mh_umask(m);

  return true;
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

  if (e->deleted)
  {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", mailbox_path(m), e->path);
    const bool c_mh_purge = cs_subset_bool(NeoMutt->sub, "mh_purge");
    if (c_mh_purge)
    {
#ifdef USE_HCACHE
      if (hc)
      {
        const char *key = e->path;
        size_t keylen = strlen(key);
        mutt_hcache_delete_record(hc, key, keylen);
      }
#endif
      unlink(path);
    }
    else
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
  else if (e->changed || e->attach_del)
  {
    if (mh_sync_message(m, msgno) == -1)
      return -1;
  }

#ifdef USE_HCACHE
  if (hc && e->changed)
  {
    const char *key = e->path;
    size_t keylen = strlen(key);
    mutt_hcache_store(hc, key, keylen, e, 0);
  }
#endif

  return 0;
}

/**
 * mh_msg_save_hcache - Save message to the header cache - Implements MxOps::msg_save_hcache()
 */
int mh_msg_save_hcache(struct Mailbox *m, struct Email *e)
{
  int rc = 0;
#ifdef USE_HCACHE
  const char *const c_header_cache =
      cs_subset_path(NeoMutt->sub, "header_cache");
  struct HeaderCache *hc = mutt_hcache_open(c_header_cache, mailbox_path(m), NULL);
  rc = mutt_hcache_store(hc, e->path, strlen(e->path), e, 0);
  mutt_hcache_close(hc);
#endif
  return rc;
}

/**
 * mh_ac_owns_path - Check whether an Account own a Mailbox path - Implements MxOps::ac_owns_path()
 */
bool mh_ac_owns_path(struct Account *a, const char *path)
{
  return true;
}

/**
 * mh_ac_add - Add a Mailbox to an Account - Implements MxOps::ac_add()
 */
bool mh_ac_add(struct Account *a, struct Mailbox *m)
{
  return true;
}

/**
 * mh_mbox_open - Open a Mailbox - Implements MxOps::mbox_open()
 */
static enum MxOpenReturns mh_mbox_open(struct Mailbox *m)
{
  return mh_read_dir(m) ? MX_OPEN_OK : MX_OPEN_ERROR;
}

/**
 * mh_mbox_open_append - Open a Mailbox for appending - Implements MxOps::mbox_open_append()
 */
static bool mh_mbox_open_append(struct Mailbox *m, OpenMailboxFlags flags)
{
  if (!(flags & (MUTT_APPENDNEW | MUTT_NEWFOLDER)))
    return true;

  if (mutt_file_mkdir(mailbox_path(m), S_IRWXU))
  {
    mutt_perror(mailbox_path(m));
    return false;
  }

  char tmp[PATH_MAX];
  snprintf(tmp, sizeof(tmp), "%s/.mh_sequences", mailbox_path(m));
  const int i = creat(tmp, S_IRWXU);
  if (i == -1)
  {
    mutt_perror(tmp);
    rmdir(mailbox_path(m));
    return false;
  }
  close(i);

  return true;
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
enum MxStatus mh_mbox_check(struct Mailbox *m)
{
  char buf[PATH_MAX];
  struct stat st, st_cur;
  bool modified = false, occult = false, flags_changed = false;
  int num_new = 0;
  struct MhSequences mhs = { 0 };
  struct HashTable *fnames = NULL;
  struct MaildirMboxData *mdata = maildir_mdata_get(m);

  const bool c_check_new = cs_subset_bool(NeoMutt->sub, "check_new");
  if (!c_check_new)
    return MX_STATUS_OK;

  mutt_str_copy(buf, mailbox_path(m), sizeof(buf));
  if (stat(buf, &st) == -1)
    return MX_STATUS_ERROR;

  /* create .mh_sequences when there isn't one. */
  snprintf(buf, sizeof(buf), "%s/.mh_sequences", mailbox_path(m));
  int rc = stat(buf, &st_cur);
  if ((rc == -1) && (errno == ENOENT))
  {
    char *tmp = NULL;
    FILE *fp = NULL;

    if (mh_mkstemp(m, &fp, &tmp))
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
    return MX_STATUS_OK;

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

  struct MdEmailArray mda = ARRAY_HEAD_INITIALIZER;

  mh_parse_dir(m, &mda, NULL);
  mh_delayed_parsing(m, &mda, NULL);

  if (mh_seq_read(&mhs, mailbox_path(m)) < 0)
    return MX_STATUS_ERROR;
  mh_update_maildir(&mda, &mhs);
  mh_seq_free(&mhs);

  /* check for modifications and adjust flags */
  fnames = mutt_hash_new(ARRAY_SIZE(&mda), MUTT_HASH_NO_FLAGS);

  struct MdEmail *md = NULL;
  struct MdEmail **mdp = NULL;
  ARRAY_FOREACH(mdp, &mda)
  {
    md = *mdp;
    /* the hash key must survive past the header, which is freed below. */
    md->canon_fname = mutt_str_dup(md->email->path);
    mutt_hash_insert(fnames, md->canon_fname, md);
  }

  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;

    e->active = false;

    md = mutt_hash_find(fnames, e->path);
    if (md && md->email && email_cmp_strict(e, md->email))
    {
      e->active = true;
      /* found the right message */
      if (!e->changed)
        if (maildir_update_flags(m, e, md->email))
          flags_changed = true;

      email_free(&md->email);
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
  num_new = maildir_move_to_mailbox(m, &mda);
  if (num_new > 0)
  {
    mailbox_changed(m, NT_MAILBOX_INVALID);
    m->changed = true;
  }

  ARRAY_FREE(&mda);
  if (occult)
    return MX_STATUS_REOPENED;
  if (num_new > 0)
    return MX_STATUS_NEW_MAIL;
  if (flags_changed)
    return MX_STATUS_FLAGS;
  return MX_STATUS_OK;
}

/**
 * mh_mbox_sync - Save changes to the Mailbox - Implements MxOps::mbox_sync()
 * @retval #MX_STATUS_REOPENED  mailbox has been externally modified
 * @retval #MX_STATUS_NEW_MAIL  new mail has arrived
 * @retval  0 Success
 * @retval -1 Error
 *
 * @note The flag retvals come from a call to a backend sync function
 */
enum MxStatus mh_mbox_sync(struct Mailbox *m)
{
  enum MxStatus check = mh_mbox_check(m);
  if (check == MX_STATUS_ERROR)
    return check;

  struct HeaderCache *hc = NULL;
#ifdef USE_HCACHE
  const char *const c_header_cache =
      cs_subset_path(NeoMutt->sub, "header_cache");
  if (m->type == MUTT_MH)
    hc = mutt_hcache_open(c_header_cache, mailbox_path(m), NULL);
#endif

  struct Progress *progress = NULL;
  if (m->verbose)
  {
    char msg[PATH_MAX];
    snprintf(msg, sizeof(msg), _("Writing %s..."), mailbox_path(m));
    progress = progress_new(msg, MUTT_PROGRESS_WRITE, m->msg_count);
  }

  for (int i = 0; i < m->msg_count; i++)
  {
    if (m->verbose)
      progress_update(progress, i, -1);

    if (mh_sync_mailbox_message(m, i, hc) == -1)
    {
      progress_free(&progress);
      goto err;
    }
  }
  progress_free(&progress);

#ifdef USE_HCACHE
  if (m->type == MUTT_MH)
    mutt_hcache_close(hc);
#endif

  mh_seq_update(m);

  /* XXX race condition? */

  mh_update_mtime(m);

  /* adjust indices */

  if (m->msg_deleted)
  {
    for (int i = 0, j = 0; i < m->msg_count; i++)
    {
      struct Email *e = m->emails[i];
      if (!e)
        break;

      if (!e->deleted)
        e->index = j++;
    }
  }

  return check;

err:
#ifdef USE_HCACHE
  if (m->type == MUTT_MH)
    mutt_hcache_close(hc);
#endif
  return MX_STATUS_ERROR;
}

/**
 * mh_mbox_close - Close a Mailbox - Implements MxOps::mbox_close()
 * @retval MX_STATUS_OK Always
 */
enum MxStatus mh_mbox_close(struct Mailbox *m)
{
  return MX_STATUS_OK;
}

/**
 * mh_msg_open - Open an email message in a Mailbox - Implements MxOps::msg_open()
 */
static bool mh_msg_open(struct Mailbox *m, struct Message *msg, int msgno)
{
  if (!m || !m->emails || (msgno >= m->msg_count))
    return false;

  struct Email *e = m->emails[msgno];
  if (!e)
    return false;

  char path[PATH_MAX];

  snprintf(path, sizeof(path), "%s/%s", mailbox_path(m), e->path);

  msg->fp = fopen(path, "r");
  if (!msg->fp)
  {
    mutt_perror(path);
    mutt_debug(LL_DEBUG1, "fopen: %s: %s (errno %d)\n", path, strerror(errno), errno);
    return false;
  }

  return true;
}

/**
 * mh_msg_open_new - Open a new message in a Mailbox - Implements MxOps::msg_open_new()
 *
 * Open a new (temporary) message in an MH folder.
 */
static bool mh_msg_open_new(struct Mailbox *m, struct Message *msg, const struct Email *e)
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
 * mh_path_canon - Canonicalise a Mailbox path - Implements MxOps::path_canon()
 */
int mh_path_canon(char *buf, size_t buflen)
{
  mutt_path_canon(buf, buflen, HomeDir, true);
  return 0;
}

/**
 * mh_path_parent - Find the parent of a Mailbox path - Implements MxOps::path_parent()
 */
int mh_path_parent(char *buf, size_t buflen)
{
  if (mutt_path_parent(buf, buflen))
    return 0;

  if (buf[0] == '~')
    mutt_path_canon(buf, buflen, HomeDir, true);

  if (mutt_path_parent(buf, buflen))
    return 0;

  return -1;
}

/**
 * mh_path_pretty - Abbreviate a Mailbox path - Implements MxOps::path_pretty()
 */
int mh_path_pretty(char *buf, size_t buflen, const char *folder)
{
  if (mutt_path_abbr_folder(buf, buflen, folder))
    return 0;

  if (mutt_path_pretty(buf, buflen, HomeDir, false))
    return 0;

  return -1;
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
  .ac_owns_path     = mh_ac_owns_path,
  .ac_add           = mh_ac_add,
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
  .path_canon       = mh_path_canon,
  .path_pretty      = mh_path_pretty,
  .path_parent      = mh_path_parent,
  .path_is_empty    = mh_check_empty,
};
// clang-format on
