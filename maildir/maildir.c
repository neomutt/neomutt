/**
 * @file
 * Maildir local mailbox type
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
 * @page maildir_maildir Maildir local mailbox type
 *
 * Maildir local mailbox type
 *
 * Implementation: #MxMaildirOps
 */

#include "config.h"
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
#include <utime.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "lib.h"
#include "progress/lib.h"
#include "copy.h"
#include "edata.h"
#include "globals.h"
#include "mdata.h"
#include "mdemail.h"
#include "mx.h"
#include "protos.h"
#include "sort.h"
#ifdef USE_INOTIFY
#include "monitor.h"
#endif
#ifdef USE_HCACHE
#include "hcache/lib.h"
#else
struct HeaderCache;
#endif
#ifdef USE_NOTMUCH
#include "notmuch/lib.h"
#endif

struct Progress;

// Flags for maildir_check()
#define MMC_NO_DIRS 0        ///< No directories changed
#define MMC_NEW_DIR (1 << 0) ///< 'new' directory changed
#define MMC_CUR_DIR (1 << 1) ///< 'cur' directory changed

/**
 * maildir_umask - Create a umask from the mailbox directory
 * @param  m   Mailbox
 * @retval num Umask
 */
mode_t maildir_umask(struct Mailbox *m)
{
  struct MaildirMboxData *mdata = maildir_mdata_get(m);
  if (mdata && mdata->umask)
    return mdata->umask;

  struct stat st = { 0 };
  if (stat(mailbox_path(m), &st) != 0)
  {
    mutt_debug(LL_DEBUG1, "stat failed on %s\n", mailbox_path(m));
    return 077;
  }

  return 0777 & ~st.st_mode;
}

/**
 * maildir_hcache_key - Get the header cache key for an Email
 * @param e Email
 * @retval str Header cache key string
 */
static inline const char *maildir_hcache_key(struct Email *e)
{
  return e->path + 4;
}

/**
 * maildir_email_new - Create a Maildir Email
 * @retval ptr Newly created Email
 *
 * Create a new Email and attach MaildirEmailData.
 *
 * @note This should be freed using email_free()
 */
struct Email *maildir_email_new(void)
{
  struct Email *e = email_new();
  e->edata = maildir_edata_new();
  e->edata_free = maildir_edata_free;

  return e;
}

/**
 * maildir_check_dir - Check for new mail / mail counts
 * @param m           Mailbox to check
 * @param dir_name    Path to Mailbox
 * @param check_new   if true, check for new mail
 * @param check_stats if true, count total, new, and flagged messages
 *
 * Checks the specified maildir subdir (cur or new) for new mail or mail counts.
 */
static void maildir_check_dir(struct Mailbox *m, const char *dir_name,
                              bool check_new, bool check_stats)
{
  DIR *dir = NULL;
  struct dirent *de = NULL;
  char *p = NULL;
  struct stat st = { 0 };

  struct Buffer *path = buf_pool_get();
  struct Buffer *msgpath = buf_pool_get();
  buf_printf(path, "%s/%s", mailbox_path(m), dir_name);

  /* when $mail_check_recent is set, if the new/ directory hasn't been modified since
   * the user last exited the mailbox, then we know there is no recent mail.  */
  const bool c_mail_check_recent = cs_subset_bool(NeoMutt->sub, "mail_check_recent");
  if (check_new && c_mail_check_recent)
  {
    if ((stat(buf_string(path), &st) == 0) &&
        (mutt_file_stat_timespec_compare(&st, MUTT_STAT_MTIME, &m->last_visited) < 0))
    {
      check_new = false;
    }
  }

  if (!(check_new || check_stats))
    goto cleanup;

  dir = mutt_file_opendir(buf_string(path), MUTT_OPENDIR_CREATE);
  if (!dir)
  {
    m->type = MUTT_UNKNOWN;
    goto cleanup;
  }

  const char c_maildir_field_delimiter = *cc_maildir_field_delimiter();

  char delimiter_version[8] = { 0 };
  snprintf(delimiter_version, sizeof(delimiter_version), "%c2,", c_maildir_field_delimiter);
  while ((de = readdir(dir)))
  {
    if (*de->d_name == '.')
      continue;

    p = strstr(de->d_name, delimiter_version);
    if (p && strchr(p + 3, 'T'))
      continue;

    if (check_stats)
    {
      m->msg_count++;
      if (p && strchr(p + 3, 'F'))
        m->msg_flagged++;
    }
    if (!p || !strchr(p + 3, 'S'))
    {
      if (check_stats)
        m->msg_unread++;
      if (check_new)
      {
        if (c_mail_check_recent)
        {
          buf_printf(msgpath, "%s/%s", buf_string(path), de->d_name);
          /* ensure this message was received since leaving this m */
          if ((stat(buf_string(msgpath), &st) == 0) &&
              (mutt_file_stat_timespec_compare(&st, MUTT_STAT_CTIME, &m->last_visited) <= 0))
          {
            continue;
          }
        }
        m->has_new = true;
        if (check_stats)
        {
          m->msg_new++;
        }
        else
        {
          break;
        }
      }
    }
  }

  closedir(dir);

cleanup:
  buf_pool_release(&path);
  buf_pool_release(&msgpath);
}

/**
 * maildir_sort_flags - Compare two flag characters - Implements ::sort_t - @ingroup sort_api
 */
static int maildir_sort_flags(const void *a, const void *b, void *sdata)
{
  return mutt_numeric_cmp(*((const char *) a), *((const char *) b));
}

/**
 * maildir_gen_flags - Generate the Maildir flags for an email
 * @param dest    Buffer for the result
 * @param destlen Length of buffer
 * @param e     Email
 */
void maildir_gen_flags(char *dest, size_t destlen, struct Email *e)
{
  *dest = '\0';

  const char *flags = NULL;

  struct MaildirEmailData *edata = maildir_edata_get(e);
  if (edata)
    flags = edata->maildir_flags;

  /* The maildir specification requires that all files in the cur
   * subdirectory have the :unique string appended, regardless of whether
   * or not there are any flags.  If .old is set, we know that this message
   * will end up in the cur directory, so we include it in the following
   * test even though there is no associated flag.  */

  if (e->flagged || e->replied || e->read || e->deleted || e->old || flags)
  {
    char tmp[1024] = { 0 };
    snprintf(tmp, sizeof(tmp), "%s%s%s%s%s", e->flagged ? "F" : "", e->replied ? "R" : "",
             e->read ? "S" : "", e->deleted ? "T" : "", NONULL(flags));
    if (flags)
      mutt_qsort_r(tmp, strlen(tmp), 1, maildir_sort_flags, NULL);

    const char c_maildir_field_delimiter = *cc_maildir_field_delimiter();
    snprintf(dest, destlen, "%c2,%s", c_maildir_field_delimiter, tmp);
  }
}

/**
 * maildir_commit_message - Commit a message to a maildir folder
 * @param m   Mailbox
 * @param msg Message to commit
 * @param e   Email
 * @retval  0 Success
 * @retval -1 Failure
 *
 * msg->path contains the file name of a file in tmp/. We take the
 * flags from this file's name.
 *
 * m is the mail folder we commit to.
 *
 * e is a header structure to which we write the message's new
 * file name.  This is used in the maildir folder sync routines.
 * When this routine is invoked from mx_msg_commit(), e is NULL.
 *
 * msg->path looks like this:
 *
 *    tmp/{cur,new}.neomutt-HOSTNAME-PID-COUNTER:flags
 *
 * See also maildir_msg_open_new().
 */
static int maildir_commit_message(struct Mailbox *m, struct Message *msg, struct Email *e)
{
  char subdir[4] = { 0 };
  char suffix[16] = { 0 };
  int rc = 0;

  if (mutt_file_fsync_close(&msg->fp))
  {
    mutt_perror(_("Could not flush message to disk"));
    return -1;
  }

  /* extract the subdir */
  char *s = strrchr(msg->path, '/') + 1;
  mutt_str_copy(subdir, s, 4);

  /* extract the flags */
  const char c_maildir_field_delimiter = *cc_maildir_field_delimiter();
  s = strchr(s, c_maildir_field_delimiter);
  if (s)
    mutt_str_copy(suffix, s, sizeof(suffix));
  else
    suffix[0] = '\0';

  /* construct a new file name. */
  struct Buffer *path = buf_pool_get();
  struct Buffer *full = buf_pool_get();
  while (true)
  {
    buf_printf(path, "%s/%lld.R%" PRIu64 ".%s%s", subdir, (long long) mutt_date_now(),
               mutt_rand64(), NONULL(ShortHostname), suffix);
    buf_printf(full, "%s/%s", mailbox_path(m), buf_string(path));

    mutt_debug(LL_DEBUG2, "renaming %s to %s\n", msg->path, buf_string(full));

    if (mutt_file_safe_rename(msg->path, buf_string(full)) == 0)
    {
      /* Adjust the mtime on the file to match the time at which this
       * message was received.  Currently this is only set when copying
       * messages between mailboxes, so we test to ensure that it is
       * actually set.  */
      if (msg->received != 0)
      {
        struct utimbuf ut = { 0 };
        int rc_utime;

        ut.actime = msg->received;
        ut.modtime = msg->received;
        do
        {
          rc_utime = utime(buf_string(full), &ut);
        } while ((rc_utime == -1) && (errno == EINTR));
        if (rc_utime == -1)
        {
          mutt_perror(_("maildir_commit_message(): unable to set time on file"));
          rc = -1;
          goto cleanup;
        }
      }

#ifdef USE_NOTMUCH
      if (m->type == MUTT_NOTMUCH)
        nm_update_filename(m, e->path, buf_string(full), e);
#endif
      if (e)
        mutt_str_replace(&e->path, buf_string(path));
      mutt_str_replace(&msg->committed_path, buf_string(full));
      FREE(&msg->path);

      goto cleanup;
    }
    else if (errno != EEXIST)
    {
      mutt_perror("%s", mailbox_path(m));
      rc = -1;
      goto cleanup;
    }
  }

cleanup:
  buf_pool_release(&path);
  buf_pool_release(&full);

  return rc;
}

/**
 * maildir_rewrite_message - Sync a message in an Maildir folder
 * @param m Mailbox
 * @param e Email
 * @retval  0 Success
 * @retval -1 Error
 */
static int maildir_rewrite_message(struct Mailbox *m, struct Email *e)
{
  if (!m || !e)
    return -1;

  bool restore = true;

  long old_body_offset = e->body->offset;
  long old_body_length = e->body->length;
  long old_hdr_lines = e->lines;

  struct Message *src = mx_msg_open(m, e);
  struct Message *dest = mx_msg_open_new(m, e, MUTT_MSG_NO_FLAGS);
  if (!src || !dest)
    return -1;

  int rc = mutt_copy_message(dest->fp, e, src, MUTT_CM_UPDATE, CH_UPDATE | CH_UPDATE_LEN, 0);
  if (rc == 0)
  {
    char oldpath[PATH_MAX] = { 0 };
    char partpath[PATH_MAX] = { 0 };
    snprintf(oldpath, sizeof(oldpath), "%s/%s", mailbox_path(m), e->path);
    mutt_str_copy(partpath, e->path, sizeof(partpath));

    rc = maildir_commit_message(m, dest, e);

    if (rc == 0)
    {
      unlink(oldpath);
      restore = false;
    }
  }
  mx_msg_close(m, &src);
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
 * maildir_sync_message - Sync an email to a Maildir folder
 * @param m Mailbox
 * @param e Email
 * @retval  0 Success
 * @retval -1 Error
 */
static int maildir_sync_message(struct Mailbox *m, struct Email *e)
{
  if (!m || !e)
    return -1;

  struct Buffer *newpath = NULL;
  struct Buffer *partpath = NULL;
  struct Buffer *fullpath = NULL;
  struct Buffer *oldpath = NULL;
  char suffix[16] = { 0 };
  int rc = 0;

  if (e->attach_del || e->env->changed)
  {
    /* when doing attachment deletion/rethreading, fall back to the maildir case. */
    if (maildir_rewrite_message(m, e) != 0)
      return -1;
    e->env->changed = false;
  }
  else
  {
    /* we just have to rename the file. */

    char *p = strrchr(e->path, '/');
    if (!p)
    {
      mutt_debug(LL_DEBUG1, "%s: unable to find subdir!\n", e->path);
      return -1;
    }
    p++;
    newpath = buf_pool_get();
    partpath = buf_pool_get();
    fullpath = buf_pool_get();
    oldpath = buf_pool_get();

    buf_strcpy(newpath, p);

    /* kill the previous flags */
    const char c_maildir_field_delimiter = *cc_maildir_field_delimiter();
    p = strchr(newpath->data, c_maildir_field_delimiter);
    if (p)
    {
      *p = '\0';
      newpath->dptr = p; /* fix buffer up, just to be safe */
    }

    maildir_gen_flags(suffix, sizeof(suffix), e);

    buf_printf(partpath, "%s/%s%s", (e->read || e->old) ? "cur" : "new",
               buf_string(newpath), suffix);
    buf_printf(fullpath, "%s/%s", mailbox_path(m), buf_string(partpath));
    buf_printf(oldpath, "%s/%s", mailbox_path(m), e->path);

    if (mutt_str_equal(buf_string(fullpath), buf_string(oldpath)))
    {
      /* message hasn't really changed */
      goto cleanup;
    }

    /* record that the message is possibly marked as trashed on disk */
    e->trash = e->deleted;

    struct stat st = { 0 };
    if (stat(buf_string(oldpath), &st) == -1)
    {
      mutt_debug(LL_DEBUG1, "File already removed (just continuing)");
      goto cleanup;
    }

    if (rename(buf_string(oldpath), buf_string(fullpath)) != 0)
    {
      mutt_perror("rename");
      rc = -1;
      goto cleanup;
    }
    mutt_str_replace(&e->path, buf_string(partpath));
  }

cleanup:
  buf_pool_release(&newpath);
  buf_pool_release(&partpath);
  buf_pool_release(&fullpath);
  buf_pool_release(&oldpath);

  return rc;
}

/**
 * maildir_update_mtime - Update our record of the Maildir modification time
 * @param m Mailbox
 */
static void maildir_update_mtime(struct Mailbox *m)
{
  char buf[PATH_MAX] = { 0 };
  struct stat st = { 0 };
  struct MaildirMboxData *mdata = maildir_mdata_get(m);

  snprintf(buf, sizeof(buf), "%s/%s", mailbox_path(m), "cur");
  if (stat(buf, &st) == 0)
    mutt_file_get_stat_timespec(&mdata->mtime_cur, &st, MUTT_STAT_MTIME);

  snprintf(buf, sizeof(buf), "%s/%s", mailbox_path(m), "new");
  if (stat(buf, &st) == 0)
    mutt_file_get_stat_timespec(&mdata->mtime, &st, MUTT_STAT_MTIME);
}

/**
 * maildir_sort_inode - Compare two Maildirs by inode number - Implements ::sort_t - @ingroup sort_api
 */
static int maildir_sort_inode(const void *a, const void *b, void *sdata)
{
  const struct MdEmail *ma = *(struct MdEmail **) a;
  const struct MdEmail *mb = *(struct MdEmail **) b;

  return mutt_numeric_cmp(ma->inode, mb->inode);
}

/**
 * maildir_parse_dir - Read a Maildir mailbox
 * @param[in]  m        Mailbox
 * @param[out] mda      Array for results
 * @param[in]  subdir   Subdirectory, e.g. 'new'
 * @param[in]  progress Progress bar
 * @retval  0 Success
 * @retval -1 Error
 * @retval -2 Aborted
 */
static int maildir_parse_dir(struct Mailbox *m, struct MdEmailArray *mda,
                             const char *subdir, struct Progress *progress)
{
  struct dirent *de = NULL;
  int rc = 0;
  bool is_old = false;
  struct MdEmail *entry = NULL;
  struct Email *e = NULL;

  struct Buffer *buf = buf_pool_get();

  buf_printf(buf, "%s/%s", mailbox_path(m), subdir);
  is_old = mutt_str_equal("cur", subdir);

  DIR *dir = mutt_file_opendir(buf_string(buf), MUTT_OPENDIR_CREATE);
  if (!dir)
  {
    rc = -1;
    goto cleanup;
  }

  while (((de = readdir(dir))) && !SigInt)
  {
    if (*de->d_name == '.')
      continue;

    mutt_debug(LL_DEBUG2, "queueing %s\n", de->d_name);

    e = maildir_email_new();
    e->old = is_old;
    maildir_parse_flags(e, de->d_name);

    progress_update(progress, ARRAY_SIZE(mda) + 1, -1);

    buf_printf(buf, "%s/%s", subdir, de->d_name);
    e->path = buf_strdup(buf);

    entry = maildir_entry_new();
    entry->email = e;
    entry->inode = de->d_ino;
    ARRAY_ADD(mda, entry);
  }

  closedir(dir);

  if (SigInt)
  {
    SigInt = false;
    return -2; /* action aborted */
  }

  ARRAY_SORT(mda, maildir_sort_inode, NULL);

cleanup:
  buf_pool_release(&buf);

  return rc;
}

#ifdef USE_HCACHE
/**
 * maildir_hcache_keylen - Calculate the length of the Maildir path
 * @param fn File name
 * @retval num Length in bytes
 *
 * @note This length excludes the flags, which will vary
 */
static size_t maildir_hcache_keylen(const char *fn)
{
  const char c_maildir_field_delimiter = *cc_maildir_field_delimiter();
  const char *p = strrchr(fn, c_maildir_field_delimiter);
  return p ? (size_t) (p - fn) : mutt_str_len(fn);
}
#endif

/**
 * maildir_delayed_parsing - This function does the second parsing pass
 * @param[in]  m   Mailbox
 * @param[out] mda Maildir array to parse
 * @param[in]  progress Progress bar
 */
static void maildir_delayed_parsing(struct Mailbox *m, struct MdEmailArray *mda,
                                    struct Progress *progress)
{
  char fn[PATH_MAX] = { 0 };

#ifdef USE_HCACHE
  const char *const c_header_cache = cs_subset_path(NeoMutt->sub, "header_cache");
  struct HeaderCache *hc = hcache_open(c_header_cache, mailbox_path(m), NULL);
  const bool c_maildir_header_cache_verify = cs_subset_bool(NeoMutt->sub, "maildir_header_cache_verify");
#endif

  struct MdEmail *md = NULL;
  struct MdEmail **mdp = NULL;
  ARRAY_FOREACH(mdp, mda)
  {
    md = *mdp;
    if (!md || !md->email || md->header_parsed)
      continue;

    progress_update(progress, ARRAY_FOREACH_IDX, -1);

    snprintf(fn, sizeof(fn), "%s/%s", mailbox_path(m), md->email->path);

#ifdef USE_HCACHE
    struct stat st_lastchanged = { 0 };
    int rc = 0;

    const char *key = maildir_hcache_key(md->email);
    size_t keylen = maildir_hcache_keylen(key);
    struct HCacheEntry hce = { 0 };

    if (hc)
    {
      hce = hcache_fetch(hc, key, keylen, 0);
    }

    if (hce.email && c_maildir_header_cache_verify)
    {
      rc = stat(fn, &st_lastchanged);
    }

    if (hce.email && (rc == 0) && (st_lastchanged.st_mtime <= hce.uidvalidity))
    {
      hce.email->edata = maildir_edata_new();
      hce.email->edata_free = maildir_edata_free;
      hce.email->old = md->email->old;
      hce.email->path = mutt_str_dup(md->email->path);
      email_free(&md->email);
      md->email = hce.email;
      maildir_parse_flags(md->email, fn);
    }
    else
#endif
    {
      if (maildir_parse_message(m->type, fn, md->email->old, md->email))
      {
        md->header_parsed = true;
#ifdef USE_HCACHE
        key = maildir_hcache_key(md->email);
        keylen = maildir_hcache_keylen(key);
        hcache_store(hc, key, keylen, md->email, 0);
#endif
      }
      else
      {
        email_free(&md->email);
      }
    }
  }
#ifdef USE_HCACHE
  hcache_close(&hc);
#endif
}

/**
 * maildir_move_to_mailbox - Copy the Maildir list to the Mailbox
 * @param[in]  m   Mailbox
 * @param[out] mda Maildir array to copy, then free
 * @retval num Number of new emails
 * @retval 0   Error
 */
int maildir_move_to_mailbox(struct Mailbox *m, const struct MdEmailArray *mda)
{
  if (!m)
    return 0;

  int oldmsgcount = m->msg_count;

  struct MdEmail *md = NULL;
  struct MdEmail **mdp = NULL;
  ARRAY_FOREACH(mdp, mda)
  {
    md = *mdp;
    mutt_debug(LL_DEBUG2, "Considering %s\n", NONULL(md->canon_fname));
    if (!md->email)
      continue;

    mutt_debug(LL_DEBUG2, "Adding header structure. Flags: %s%s%s%s%s\n",
               md->email->flagged ? "f" : "", md->email->deleted ? "D" : "",
               md->email->replied ? "r" : "", md->email->old ? "O" : "",
               md->email->read ? "R" : "");
    mx_alloc_memory(m, m->msg_count);

    m->emails[m->msg_count] = md->email;
    m->emails[m->msg_count]->index = m->msg_count;
    mailbox_size_add(m, md->email);

    md->email = NULL;
    m->msg_count++;
  }

  int num = 0;
  if (m->msg_count > oldmsgcount)
    num = m->msg_count - oldmsgcount;

  return num;
}

/**
 * maildir_read_dir - Read a Maildir style mailbox
 * @param m      Mailbox
 * @param subdir Subdir of the maildir mailbox to read from
 * @retval  0 Success
 * @retval -1 Failure
 */
static int maildir_read_dir(struct Mailbox *m, const char *subdir)
{
  if (!m)
    return -1;

  struct Progress *progress = NULL;

  if (m->verbose)
  {
    char msg[PATH_MAX] = { 0 };
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

  struct MdEmailArray mda = ARRAY_HEAD_INITIALIZER;
  int rc = maildir_parse_dir(m, &mda, subdir, progress);
  progress_free(&progress);
  if (rc < 0)
    return -1;

  if (m->verbose)
  {
    char msg[PATH_MAX] = { 0 };
    snprintf(msg, sizeof(msg), _("Reading %s..."), mailbox_path(m));
    progress = progress_new(msg, MUTT_PROGRESS_READ, ARRAY_SIZE(&mda));
  }
  maildir_delayed_parsing(m, &mda, progress);
  progress_free(&progress);

  maildir_move_to_mailbox(m, &mda);
  maildirarray_clear(&mda);

  if (!mdata->umask)
    mdata->umask = maildir_umask(m);

  return 0;
}

/**
 * maildir_canon_filename - Generate the canonical filename for a Maildir folder
 * @param dest   Buffer for the result
 * @param src    Buffer containing source filename
 *
 * @note         maildir filename is defined as: \<base filename\>:2,\<flags\>
 *               but \<base filename\> may contain additional comma separated
 *               fields. Additionally, `:` may be replaced as the field
 *               delimiter by a user defined alternative.
 */
static void maildir_canon_filename(struct Buffer *dest, const char *src)
{
  if (!dest || !src)
    return;

  char *t = strrchr(src, '/');
  if (t)
    src = t + 1;

  buf_strcpy(dest, src);

  const char c_maildir_field_delimiter = *cc_maildir_field_delimiter();

  char searchable_bytes[8] = { 0 };
  snprintf(searchable_bytes, sizeof(searchable_bytes), ",%c", c_maildir_field_delimiter);
  char *u = strpbrk(dest->data, searchable_bytes);

  if (u)
  {
    *u = '\0';
    dest->dptr = u;
  }
}

/**
 * maildir_open_find_message_dir - Find a message in a maildir folder
 * @param[in]  folder    Base folder
 * @param[in]  unique    Unique part of filename
 * @param[in]  subfolder Subfolder to search, e.g. 'cur'
 * @param[out] newname   File's new name
 * @retval ptr File handle
 *
 * These functions try to find a message in a maildir folder when it
 * has moved under our feet.  Note that this code is rather expensive, but
 * then again, it's called rarely.
 */
static FILE *maildir_open_find_message_dir(const char *folder, const char *unique,
                                           const char *subfolder, char **newname)
{
  struct Buffer *dirname = buf_pool_get();
  struct Buffer *tunique = buf_pool_get();
  struct Buffer *fname = buf_pool_get();

  struct dirent *de = NULL;

  FILE *fp = NULL;
  int oe = ENOENT;

  buf_printf(dirname, "%s/%s", folder, subfolder);

  DIR *dir = mutt_file_opendir(buf_string(dirname), MUTT_OPENDIR_CREATE);
  if (!dir)
  {
    errno = ENOENT;
    goto cleanup;
  }

  while ((de = readdir(dir)))
  {
    maildir_canon_filename(tunique, de->d_name);

    if (mutt_str_equal(buf_string(tunique), unique))
    {
      buf_printf(fname, "%s/%s/%s", folder, subfolder, de->d_name);
      fp = fopen(buf_string(fname), "r");
      oe = errno;
      break;
    }
  }

  closedir(dir);

  if (newname && fp)
    *newname = buf_strdup(fname);

  errno = oe;

cleanup:
  buf_pool_release(&dirname);
  buf_pool_release(&tunique);
  buf_pool_release(&fname);

  return fp;
}

/**
 * maildir_parse_flags - Parse Maildir file flags
 * @param e    Email
 * @param path Path to email file
 */
void maildir_parse_flags(struct Email *e, const char *path)
{
  char *q = NULL;

  e->flagged = false;
  e->read = false;
  e->replied = false;

  struct MaildirEmailData *edata = maildir_edata_get(e);

  const char c_maildir_field_delimiter = *cc_maildir_field_delimiter();
  char *p = strrchr(path, c_maildir_field_delimiter);
  if (p && mutt_str_startswith(p + 1, "2,"))
  {
    p += 3;

    mutt_str_replace(&edata->maildir_flags, p);
    q = edata->maildir_flags;

    while (*p)
    {
      switch (*p)
      {
        case 'F': // Flagged
          e->flagged = true;
          break;

        case 'R': // Replied
          e->replied = true;
          break;

        case 'S': // Seen
          e->read = true;
          break;

        case 'T': // Trashed
        {
          const bool c_flag_safe = cs_subset_bool(NeoMutt->sub, "flag_safe");
          if (!e->flagged || !c_flag_safe)
          {
            e->trash = true;
            e->deleted = true;
          }
          break;
        }

        default:
          *q++ = *p;
          break;
      }
      p++;
    }
  }

  if (q == edata->maildir_flags)
    FREE(&edata->maildir_flags);
  else if (q)
    *q = '\0';
}

/**
 * maildir_parse_stream - Parse a Maildir message
 * @param type   Mailbox type, e.g. #MUTT_MAILDIR
 * @param fp     Message file handle
 * @param fname  Message filename
 * @param is_old true, if the email is old (read)
 * @param e      Email
 * @retval true Success
 *
 * Actually parse a maildir message.  This may also be used to fill
 * out a fake header structure generated by lazy maildir parsing.
 */
bool maildir_parse_stream(enum MailboxType type, FILE *fp, const char *fname,
                          bool is_old, struct Email *e)
{
  if (!fp || !fname || !e)
    return false;

  const long size = mutt_file_get_size_fp(fp);
  if (size == 0)
    return false;

  e->env = mutt_rfc822_read_header(fp, e, false, false);

  if (e->received == 0)
    e->received = e->date_sent;

  /* always update the length since we have fresh information available. */
  e->body->length = size - e->body->offset;

  e->index = -1;

  if (type == MUTT_MAILDIR)
  {
    /* maildir stores its flags in the filename, so ignore the
     * flags in the header of the message */

    e->old = is_old;
    maildir_parse_flags(e, fname);
  }
  return e;
}

/**
 * maildir_parse_message - Actually parse a maildir message
 * @param type   Mailbox type, e.g. #MUTT_MAILDIR
 * @param fname  Message filename
 * @param is_old true, if the email is old (read)
 * @param e      Email to populate
 * @retval true Success
 *
 * This may also be used to fill out a fake header structure generated by lazy
 * maildir parsing.
 */
bool maildir_parse_message(enum MailboxType type, const char *fname,
                           bool is_old, struct Email *e)
{
  if (!fname || !e)
    return false;

  FILE *fp = fopen(fname, "r");
  if (!fp)
    return false;

  bool rc = maildir_parse_stream(type, fp, fname, is_old, e);
  mutt_file_fclose(&fp);
  return rc;
}

/**
 * maildir_sync_mailbox_message - Save changes to the mailbox
 * @param m     Mailbox
 * @param e     Email
 * @param hc    Header cache handle
 * @retval true Success
 * @retval false Error
 */
bool maildir_sync_mailbox_message(struct Mailbox *m, struct Email *e, struct HeaderCache *hc)
{
  if (!e)
    return false;

  const bool c_maildir_trash = cs_subset_bool(NeoMutt->sub, "maildir_trash");
  if (e->deleted && !c_maildir_trash)
  {
    char path[PATH_MAX] = { 0 };
    snprintf(path, sizeof(path), "%s/%s", mailbox_path(m), e->path);
#ifdef USE_HCACHE
    if (hc)
    {
      const char *key = maildir_hcache_key(e);
      size_t keylen = maildir_hcache_keylen(key);
      hcache_delete_record(hc, key, keylen);
    }
#endif
    unlink(path);
  }
  else if (e->changed || e->attach_del ||
           ((c_maildir_trash || e->trash) && (e->deleted != e->trash)))
  {
    if (maildir_sync_message(m, e) == -1)
      return false;
  }

#ifdef USE_HCACHE
  if (hc && e->changed)
  {
    const char *key = maildir_hcache_key(e);
    size_t keylen = maildir_hcache_keylen(key);
    hcache_store(hc, key, keylen, e, 0);
  }
#endif

  return true;
}

/**
 * maildir_open_find_message - Find a message by name
 * @param[in]  folder  Maildir path
 * @param[in]  msg     Email path
 * @param[out] newname New name, if it has moved
 * @retval ptr File handle
 */
FILE *maildir_open_find_message(const char *folder, const char *msg, char **newname)
{
  static unsigned int new_hits = 0, cur_hits = 0; /* simple dynamic optimization */

  struct Buffer *unique = buf_pool_get();
  maildir_canon_filename(unique, msg);

  FILE *fp = maildir_open_find_message_dir(folder, buf_string(unique),
                                           (new_hits > cur_hits) ? "new" : "cur", newname);
  if (fp || (errno != ENOENT))
  {
    if ((new_hits < UINT_MAX) && (cur_hits < UINT_MAX))
    {
      new_hits += ((new_hits > cur_hits) ? 1 : 0);
      cur_hits += ((new_hits > cur_hits) ? 0 : 1);
    }

    goto cleanup;
  }
  fp = maildir_open_find_message_dir(folder, buf_string(unique),
                                     (new_hits > cur_hits) ? "cur" : "new", newname);
  if (fp || (errno != ENOENT))
  {
    if ((new_hits < UINT_MAX) && (cur_hits < UINT_MAX))
    {
      new_hits += ((new_hits > cur_hits) ? 0 : 1);
      cur_hits += ((new_hits > cur_hits) ? 1 : 0);
    }

    goto cleanup;
  }

  fp = NULL;

cleanup:
  buf_pool_release(&unique);

  return fp;
}

/**
 * maildir_check_empty - Is the mailbox empty
 * @param path Mailbox to check
 * @retval 1 Mailbox is empty
 * @retval 0 Mailbox contains mail
 * @retval -1 Error
 */
int maildir_check_empty(struct Buffer *path)
{
  DIR *dir = NULL;
  struct dirent *de = NULL;
  int rc = 1; /* assume empty until we find a message */
  char realpath[PATH_MAX] = { 0 };
  int iter = 0;

  /* Strategy here is to look for any file not beginning with a period */

  do
  {
    /* we do "cur" on the first iteration since it's more likely that we'll
     * find old messages without having to scan both subdirs */
    snprintf(realpath, sizeof(realpath), "%s/%s", buf_string(path),
             (iter == 0) ? "cur" : "new");
    dir = mutt_file_opendir(realpath, MUTT_OPENDIR_CREATE);
    if (!dir)
      return -1;
    while ((de = readdir(dir)))
    {
      if (*de->d_name != '.')
      {
        rc = 0;
        break;
      }
    }
    closedir(dir);
    iter++;
  } while (rc && iter < 2);

  return rc;
}

/**
 * maildir_ac_owns_path - Check whether an Account owns a Mailbox path - Implements MxOps::ac_owns_path() - @ingroup mx_ac_owns_path
 */
static bool maildir_ac_owns_path(struct Account *a, const char *path)
{
  return true;
}

/**
 * maildir_ac_add - Add a Mailbox to an Account - Implements MxOps::ac_add() - @ingroup mx_ac_add
 */
static bool maildir_ac_add(struct Account *a, struct Mailbox *m)
{
  return true;
}

/**
 * maildir_mbox_open - Open a Mailbox - Implements MxOps::mbox_open() - @ingroup mx_mbox_open
 */
static enum MxOpenReturns maildir_mbox_open(struct Mailbox *m)
{
  if ((maildir_read_dir(m, "new") == -1) || (maildir_read_dir(m, "cur") == -1))
    return MX_OPEN_ERROR;

  return MX_OPEN_OK;
}

/**
 * maildir_mbox_open_append - Open a Mailbox for appending - Implements MxOps::mbox_open_append() - @ingroup mx_mbox_open_append
 */
static bool maildir_mbox_open_append(struct Mailbox *m, OpenMailboxFlags flags)
{
  if (!(flags & (MUTT_APPEND | MUTT_APPENDNEW | MUTT_NEWFOLDER)))
  {
    return true;
  }

  errno = 0;
  if ((mutt_file_mkdir(mailbox_path(m), S_IRWXU) != 0) && (errno != EEXIST))
  {
    mutt_perror("%s", mailbox_path(m));
    return false;
  }

  char tmp[PATH_MAX] = { 0 };
  snprintf(tmp, sizeof(tmp), "%s/cur", mailbox_path(m));
  errno = 0;
  if ((mkdir(tmp, S_IRWXU) != 0) && (errno != EEXIST))
  {
    mutt_perror("%s", tmp);
    rmdir(mailbox_path(m));
    return false;
  }

  snprintf(tmp, sizeof(tmp), "%s/new", mailbox_path(m));
  errno = 0;
  if ((mkdir(tmp, S_IRWXU) != 0) && (errno != EEXIST))
  {
    mutt_perror("%s", tmp);
    snprintf(tmp, sizeof(tmp), "%s/cur", mailbox_path(m));
    rmdir(tmp);
    rmdir(mailbox_path(m));
    return false;
  }

  snprintf(tmp, sizeof(tmp), "%s/tmp", mailbox_path(m));
  errno = 0;
  if ((mkdir(tmp, S_IRWXU) != 0) && (errno != EEXIST))
  {
    mutt_perror("%s", tmp);
    snprintf(tmp, sizeof(tmp), "%s/cur", mailbox_path(m));
    rmdir(tmp);
    snprintf(tmp, sizeof(tmp), "%s/new", mailbox_path(m));
    rmdir(tmp);
    rmdir(mailbox_path(m));
    return false;
  }

  return true;
}

/**
 * maildir_update_flags - Update the mailbox flags
 * @param m     Mailbox
 * @param e_old Old Email
 * @param e_new New Email
 * @retval true  The flags changed
 * @retval false Otherwise
 */
bool maildir_update_flags(struct Mailbox *m, struct Email *e_old, struct Email *e_new)
{
  if (!m)
    return false;

  /* save the global state here so we can reset it at the
   * end of list block if required.  */
  bool context_changed = m->changed;

  /* user didn't modify this message.  alter the flags to match the
   * current state on disk.  This may not actually do
   * anything. mutt_set_flag() will just ignore the call if the status
   * bits are already properly set, but it is still faster not to pass
   * through it */
  if (e_old->flagged != e_new->flagged)
    mutt_set_flag(m, e_old, MUTT_FLAG, e_new->flagged, true);
  if (e_old->replied != e_new->replied)
    mutt_set_flag(m, e_old, MUTT_REPLIED, e_new->replied, true);
  if (e_old->read != e_new->read)
    mutt_set_flag(m, e_old, MUTT_READ, e_new->read, true);
  if (e_old->old != e_new->old)
    mutt_set_flag(m, e_old, MUTT_OLD, e_new->old, true);

  /* mutt_set_flag() will set this, but we don't need to
   * sync the changes we made because we just updated the
   * context to match the current on-disk state of the
   * message.  */
  bool header_changed = e_old->changed;
  e_old->changed = false;

  /* if the mailbox was not modified before we made these
   * changes, unset the changed flag since nothing needs to
   * be synchronized.  */
  if (!context_changed)
    m->changed = false;

  return header_changed;
}

/**
 * maildir_check - Check for new mail
 * @param m Mailbox
 * @retval enum #MxStatus
 *
 * This function handles arrival of new mail and reopening of maildir folders.
 * The basic idea here is we check to see if either the new or cur
 * subdirectories have changed, and if so, we scan them for the list of files.
 * We check for newly added messages, and then merge the flags messages we
 * already knew about.  We don't treat either subdirectory differently, as mail
 * could be copied directly into the cur directory from another agent.
 */
static enum MxStatus maildir_check(struct Mailbox *m)
{
  struct stat st_new = { 0 }; /* status of the "new" subdirectory */
  struct stat st_cur = { 0 }; /* status of the "cur" subdirectory */
  int changed = MMC_NO_DIRS;  /* which subdirectories have changed */
  bool occult = false;        /* messages were removed from the mailbox */
  int num_new = 0;            /* number of new messages added to the mailbox */
  bool flags_changed = false; /* message flags were changed in the mailbox */
  struct HashTable *hash_names = NULL; // Hash Table: "base-filename" -> MdEmail
  struct MaildirMboxData *mdata = maildir_mdata_get(m);

  /* XXX seems like this check belongs in mx_mbox_check() rather than here.  */
  const bool c_check_new = cs_subset_bool(NeoMutt->sub, "check_new");
  if (!c_check_new)
    return MX_STATUS_OK;

  struct Buffer *buf = buf_pool_get();
  buf_printf(buf, "%s/new", mailbox_path(m));
  if (stat(buf_string(buf), &st_new) == -1)
  {
    buf_pool_release(&buf);
    return MX_STATUS_ERROR;
  }

  buf_printf(buf, "%s/cur", mailbox_path(m));
  if (stat(buf_string(buf), &st_cur) == -1)
  {
    buf_pool_release(&buf);
    return MX_STATUS_ERROR;
  }

  /* determine which subdirectories need to be scanned */
  if (mutt_file_stat_timespec_compare(&st_new, MUTT_STAT_MTIME, &mdata->mtime) > 0)
    changed = MMC_NEW_DIR;
  if (mutt_file_stat_timespec_compare(&st_cur, MUTT_STAT_MTIME, &mdata->mtime_cur) > 0)
    changed |= MMC_CUR_DIR;

  if (changed == MMC_NO_DIRS)
  {
    buf_pool_release(&buf);
    return MX_STATUS_OK; /* nothing to do */
  }

  /* Update the modification times on the mailbox.
   *
   * The monitor code notices changes in the open mailbox too quickly.
   * In practice, this sometimes leads to all the new messages not being
   * noticed during the SAME group of mtime stat updates.  To work around
   * the problem, don't update the stat times for a monitor caused check. */
#ifdef USE_INOTIFY
  if (MonitorContextChanged)
  {
    MonitorContextChanged = false;
  }
  else
#endif
  {
    mutt_file_get_stat_timespec(&mdata->mtime_cur, &st_cur, MUTT_STAT_MTIME);
    mutt_file_get_stat_timespec(&mdata->mtime, &st_new, MUTT_STAT_MTIME);
  }

  /* do a fast scan of just the filenames in
   * the subdirectories that have changed.  */
  struct MdEmailArray mda = ARRAY_HEAD_INITIALIZER;
  if (changed & MMC_NEW_DIR)
    maildir_parse_dir(m, &mda, "new", NULL);
  if (changed & MMC_CUR_DIR)
    maildir_parse_dir(m, &mda, "cur", NULL);

  /* we create a hash table keyed off the canonical (sans flags) filename
   * of each message we scanned.  This is used in the loop over the
   * existing messages below to do some correlation.  */
  hash_names = mutt_hash_new(ARRAY_SIZE(&mda), MUTT_HASH_NO_FLAGS);

  struct MdEmail *md = NULL;
  struct MdEmail **mdp = NULL;
  ARRAY_FOREACH(mdp, &mda)
  {
    md = *mdp;
    maildir_canon_filename(buf, md->email->path);
    md->canon_fname = buf_strdup(buf);
    mutt_hash_insert(hash_names, md->canon_fname, md);
  }

  /* check for modifications and adjust flags */
  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;

    maildir_canon_filename(buf, e->path);
    md = mutt_hash_find(hash_names, buf_string(buf));
    if (md && md->email)
    {
      /* message already exists, merge flags */

      /* check to see if the message has moved to a different
       * subdirectory.  If so, update the associated filename.  */
      if (!mutt_str_equal(e->path, md->email->path))
        mutt_str_replace(&e->path, md->email->path);

      /* if the user hasn't modified the flags on this message, update
       * the flags we just detected.  */
      if (!e->changed)
        if (maildir_update_flags(m, e, md->email))
          flags_changed = true;

      if (e->deleted == e->trash)
      {
        if (e->deleted != md->email->deleted)
        {
          e->deleted = md->email->deleted;
          flags_changed = true;
        }
      }
      e->trash = md->email->trash;

      /* this is a duplicate of an existing email, so remove it */
      email_free(&md->email);
    }
    /* This message was not in the list of messages we just scanned.
     * Check to see if we have enough information to know if the
     * message has disappeared out from underneath us.  */
    else if (((changed & MMC_NEW_DIR) && mutt_strn_equal(e->path, "new/", 4)) ||
             ((changed & MMC_CUR_DIR) && mutt_strn_equal(e->path, "cur/", 4)))
    {
      /* This message disappeared, so we need to simulate a "reopen"
       * event.  We know it disappeared because we just scanned the
       * subdirectory it used to reside in.  */
      occult = true;
      e->deleted = true;
      e->purge = true;
    }
    else
    {
      /* This message resides in a subdirectory which was not
       * modified, so we assume that it is still present and
       * unchanged.  */
    }
  }

  /* destroy the file name hash */
  mutt_hash_free(&hash_names);

  /* If we didn't just get new mail, update the tables. */
  if (occult)
    mailbox_changed(m, NT_MAILBOX_RESORT);

  /* do any delayed parsing we need to do. */
  maildir_delayed_parsing(m, &mda, NULL);

  /* Incorporate new messages */
  num_new = maildir_move_to_mailbox(m, &mda);
  maildirarray_clear(&mda);

  if (num_new > 0)
  {
    mailbox_changed(m, NT_MAILBOX_INVALID);
    m->changed = true;
  }

  buf_pool_release(&buf);

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
 * maildir_mbox_check - Check for new mail - Implements MxOps::mbox_check() - @ingroup mx_mbox_check
 */
static enum MxStatus maildir_mbox_check(struct Mailbox *m)
{
  return maildir_check(m);
}

/**
 * maildir_mbox_check_stats - Check the Mailbox statistics - Implements MxOps::mbox_check_stats() - @ingroup mx_mbox_check_stats
 */
static enum MxStatus maildir_mbox_check_stats(struct Mailbox *m, uint8_t flags)
{
  bool check_stats = flags & MUTT_MAILBOX_CHECK_FORCE_STATS;
  bool check_new = true;

  if (check_stats)
  {
    m->msg_new = 0;
    m->msg_count = 0;
    m->msg_unread = 0;
    m->msg_flagged = 0;
  }

  maildir_check_dir(m, "new", check_new, check_stats);

  const bool c_maildir_check_cur = cs_subset_bool(NeoMutt->sub, "maildir_check_cur");
  check_new = !m->has_new && c_maildir_check_cur;
  if (check_new || check_stats)
    maildir_check_dir(m, "cur", check_new, check_stats);

  return m->msg_new ? MX_STATUS_NEW_MAIL : MX_STATUS_OK;
}

/**
 * maildir_mbox_sync - Save changes to the Mailbox - Implements MxOps::mbox_sync() - @ingroup mx_mbox_sync
 * @retval enum #MxStatus
 *
 * @note The flag retvals come from a call to a backend sync function
 */
static enum MxStatus maildir_mbox_sync(struct Mailbox *m)
{
  enum MxStatus check = maildir_check(m);
  if (check == MX_STATUS_ERROR)
    return check;

  struct HeaderCache *hc = NULL;
#ifdef USE_HCACHE
  const char *const c_header_cache = cs_subset_path(NeoMutt->sub, "header_cache");
  if (m->type == MUTT_MAILDIR)
    hc = hcache_open(c_header_cache, mailbox_path(m), NULL);
#endif

  struct Progress *progress = NULL;
  if (m->verbose)
  {
    char msg[PATH_MAX] = { 0 };
    snprintf(msg, sizeof(msg), _("Writing %s..."), mailbox_path(m));
    progress = progress_new(msg, MUTT_PROGRESS_WRITE, m->msg_count);
  }

  for (int i = 0; i < m->msg_count; i++)
  {
    progress_update(progress, i, -1);

    struct Email *e = m->emails[i];
    if (!maildir_sync_mailbox_message(m, e, hc))
    {
      progress_free(&progress);
      goto err;
    }
  }
  progress_free(&progress);

#ifdef USE_HCACHE
  if (m->type == MUTT_MAILDIR)
    hcache_close(&hc);
#endif

  /* XXX race condition? */

  maildir_update_mtime(m);

  /* adjust indices */

  if (m->msg_deleted)
  {
    const bool c_maildir_trash = cs_subset_bool(NeoMutt->sub, "maildir_trash");
    for (int i = 0, j = 0; i < m->msg_count; i++)
    {
      struct Email *e = m->emails[i];
      if (!e)
        break;

      if (!e->deleted || c_maildir_trash)
        e->index = j++;
    }
  }

  return check;

err:
#ifdef USE_HCACHE
  if (m->type == MUTT_MAILDIR)
    hcache_close(&hc);
#endif
  return MX_STATUS_ERROR;
}

/**
 * maildir_mbox_close - Close a Mailbox - Implements MxOps::mbox_close() - @ingroup mx_mbox_close
 * @retval #MX_STATUS_OK Always
 */
static enum MxStatus maildir_mbox_close(struct Mailbox *m)
{
  return MX_STATUS_OK;
}

/**
 * maildir_msg_open - Open an email message in a Mailbox - Implements MxOps::msg_open() - @ingroup mx_msg_open
 */
static bool maildir_msg_open(struct Mailbox *m, struct Message *msg, struct Email *e)
{
  char path[PATH_MAX] = { 0 };

  snprintf(path, sizeof(path), "%s/%s", mailbox_path(m), e->path);

  msg->fp = fopen(path, "r");
  if (!msg->fp && (errno == ENOENT))
    msg->fp = maildir_open_find_message(mailbox_path(m), e->path, NULL);

  if (!msg->fp)
  {
    mutt_perror("%s", path);
    mutt_debug(LL_DEBUG1, "fopen: %s: %s (errno %d)\n", path, strerror(errno), errno);
    return false;
  }

  return true;
}

/**
 * maildir_msg_open_new - Open a new message in a Mailbox - Implements MxOps::msg_open_new() - @ingroup mx_msg_open_new
 *
 * Open a new (temporary) message in a maildir folder.
 *
 * @note This uses _almost_ the maildir file name format,
 * but with a {cur,new} prefix.
 */
bool maildir_msg_open_new(struct Mailbox *m, struct Message *msg, const struct Email *e)
{
  int fd;
  char path[PATH_MAX] = { 0 };
  char suffix[16] = { 0 };
  char subdir[16] = { 0 };

  if (e)
  {
    struct Email tmp = *e;
    tmp.deleted = false;
    tmp.edata = NULL;
    maildir_gen_flags(suffix, sizeof(suffix), &tmp);
  }
  else
  {
    *suffix = '\0';
  }

  if (e && (e->read || e->old))
    mutt_str_copy(subdir, "cur", sizeof(subdir));
  else
    mutt_str_copy(subdir, "new", sizeof(subdir));

  mode_t omask = umask(maildir_umask(m));
  while (true)
  {
    snprintf(path, sizeof(path), "%s/tmp/%s.%lld.R%" PRIu64 ".%s%s",
             mailbox_path(m), subdir, (long long) mutt_date_now(),
             mutt_rand64(), NONULL(ShortHostname), suffix);

    mutt_debug(LL_DEBUG2, "Trying %s\n", path);

    fd = open(path, O_WRONLY | O_EXCL | O_CREAT, 0666);
    if (fd == -1)
    {
      if (errno != EEXIST)
      {
        umask(omask);
        mutt_perror("%s", path);
        return false;
      }
    }
    else
    {
      mutt_debug(LL_DEBUG2, "Success\n");
      msg->path = mutt_str_dup(path);
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
    return false;
  }

  return true;
}

/**
 * maildir_msg_commit - Save changes to an email - Implements MxOps::msg_commit() - @ingroup mx_msg_commit
 */
static int maildir_msg_commit(struct Mailbox *m, struct Message *msg)
{
  return maildir_commit_message(m, msg, NULL);
}

/**
 * maildir_msg_close - Close an email - Implements MxOps::msg_close() - @ingroup mx_msg_close
 *
 * @note May also return EOF Failure, see errno
 */
static int maildir_msg_close(struct Mailbox *m, struct Message *msg)
{
  return mutt_file_fclose(&msg->fp);
}

/**
 * maildir_msg_save_hcache - Save message to the header cache - Implements MxOps::msg_save_hcache() - @ingroup mx_msg_save_hcache
 */
static int maildir_msg_save_hcache(struct Mailbox *m, struct Email *e)
{
  int rc = 0;
#ifdef USE_HCACHE
  const char *const c_header_cache = cs_subset_path(NeoMutt->sub, "header_cache");
  struct HeaderCache *hc = hcache_open(c_header_cache, mailbox_path(m), NULL);
  const char *key = maildir_hcache_key(e);
  int keylen = maildir_hcache_keylen(key);
  rc = hcache_store(hc, key, keylen, e, 0);
  hcache_close(&hc);
#endif
  return rc;
}

/**
 * maildir_path_canon - Canonicalise a Mailbox path - Implements MxOps::path_canon() - @ingroup mx_path_canon
 */
static int maildir_path_canon(struct Buffer *path)
{
  mutt_path_canon(path, HomeDir, true);
  return 0;
}

/**
 * maildir_path_parent - Find the parent of a Mailbox path - Implements MxOps::path_parent() - @ingroup mx_path_parent
 */
static int maildir_path_parent(struct Buffer *path)
{
  if (mutt_path_parent(path))
    return 0;

  if (buf_at(path, 0) == '~')
    mutt_path_canon(path, HomeDir, true);

  if (mutt_path_parent(path))
    return 0;

  return -1;
}

/**
 * maildir_path_probe - Is this a Maildir Mailbox? - Implements MxOps::path_probe() - @ingroup mx_path_probe
 */
static enum MailboxType maildir_path_probe(const char *path, const struct stat *st)
{
  if (!st || !S_ISDIR(st->st_mode))
    return MUTT_UNKNOWN;

  char sub[PATH_MAX] = { 0 };
  struct stat stsub = { 0 };
  char *subs[] = { "cur", "new" };
  for (size_t i = 0; i < mutt_array_size(subs); ++i)
  {
    snprintf(sub, sizeof(sub), "%s/%s", path, subs[i]);
    if ((stat(sub, &stsub) == 0) && S_ISDIR(stsub.st_mode))
      return MUTT_MAILDIR;
  }

  return MUTT_UNKNOWN;
}

/**
 * MxMaildirOps - Maildir Mailbox - Implements ::MxOps - @ingroup mx_api
 */
const struct MxOps MxMaildirOps = {
  // clang-format off
  .type            = MUTT_MAILDIR,
  .name             = "maildir",
  .is_local         = true,
  .ac_owns_path     = maildir_ac_owns_path,
  .ac_add           = maildir_ac_add,
  .mbox_open        = maildir_mbox_open,
  .mbox_open_append = maildir_mbox_open_append,
  .mbox_check       = maildir_mbox_check,
  .mbox_check_stats = maildir_mbox_check_stats,
  .mbox_sync        = maildir_mbox_sync,
  .mbox_close       = maildir_mbox_close,
  .msg_open         = maildir_msg_open,
  .msg_open_new     = maildir_msg_open_new,
  .msg_commit       = maildir_msg_commit,
  .msg_close        = maildir_msg_close,
  .msg_padding_size = NULL,
  .msg_save_hcache  = maildir_msg_save_hcache,
  .tags_edit        = NULL,
  .tags_commit      = NULL,
  .path_probe       = maildir_path_probe,
  .path_canon       = maildir_path_canon,
  .path_parent      = maildir_path_parent,
  .path_is_empty    = maildir_check_empty,
  // clang-format on
};
