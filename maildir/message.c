/**
 * @file
 * Maildir Message
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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
 * @page maildir_message Maildir Message
 *
 * Maildir Message
 */

#include "config.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "message.h"
#include "copy.h"
#include "edata.h"
#include "globals.h"
#include "hcache.h"
#include "mx.h"
#include "shared.h"
#include "sort.h"
#ifdef USE_HCACHE
#include "hcache/lib.h"
#else
struct HeaderCache;
#endif

int nm_update_filename(struct Mailbox *m, const char *old_file,
                       const char *new_file, struct Email *e);

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
    flags = edata->custom_flags;

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

  buf_concat_path(dirname, folder, subfolder);

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
      fp = mutt_file_fopen(buf_string(fname), "r");
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
      hcache_delete_email(hc, key, keylen);
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
    hcache_store_email(hc, key, keylen, e, 0);
  }
#endif

  return true;
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
int maildir_rewrite_message(struct Mailbox *m, struct Email *e)
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

// Mailbox API -----------------------------------------------------------------

/**
 * maildir_msg_open - Open an email message in a Mailbox - Implements MxOps::msg_open() - @ingroup mx_msg_open
 */
bool maildir_msg_open(struct Mailbox *m, struct Message *msg, struct Email *e)
{
  char path[PATH_MAX] = { 0 };

  snprintf(path, sizeof(path), "%s/%s", mailbox_path(m), e->path);

  msg->fp = mutt_file_fopen(path, "r");
  if (!msg->fp && (errno == ENOENT))
    msg->fp = maildir_open_find_message(mailbox_path(m), e->path, NULL);

  if (!msg->fp)
  {
    mutt_perror("%s", path);
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
int maildir_msg_commit(struct Mailbox *m, struct Message *msg)
{
  return maildir_commit_message(m, msg, NULL);
}

/**
 * maildir_msg_close - Close an email - Implements MxOps::msg_close() - @ingroup mx_msg_close
 *
 * @note May also return EOF Failure, see errno
 */
int maildir_msg_close(struct Mailbox *m, struct Message *msg)
{
  return mutt_file_fclose(&msg->fp);
}

/**
 * maildir_msg_save_hcache - Save message to the header cache - Implements MxOps::msg_save_hcache() - @ingroup mx_msg_save_hcache
 */
int maildir_msg_save_hcache(struct Mailbox *m, struct Email *e)
{
  int rc = 0;
#ifdef USE_HCACHE
  const char *const c_header_cache = cs_subset_path(NeoMutt->sub, "header_cache");
  struct HeaderCache *hc = hcache_open(c_header_cache, mailbox_path(m), NULL);
  const char *key = maildir_hcache_key(e);
  int keylen = maildir_hcache_keylen(key);
  rc = hcache_store_email(hc, key, keylen, e, 0);
  hcache_close(&hc);
#endif
  return rc;
}
