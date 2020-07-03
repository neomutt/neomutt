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
 */

#include "config.h"
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
#include <unistd.h>
#include "private.h"
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "monitor.h"
#include "mutt_globals.h"
#include "muttlib.h"
#include "mx.h"
#include "maildir/lib.h"
#ifdef USE_HCACHE
#include "hcache/lib.h"
#endif

// Flags for maildir_mbox_check()
#define MMC_NO_DIRS 0        ///< No directories changed
#define MMC_NEW_DIR (1 << 0) ///< 'new' directory changed
#define MMC_CUR_DIR (1 << 1) ///< 'cur' directory changed

/* These Config Variables are only used in maildir/maildir.c */
bool C_MaildirCheckCur; ///< Config: Check both 'new' and 'cur' directories for new mail

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
  DIR *dirp = NULL;
  struct dirent *de = NULL;
  char *p = NULL;
  struct stat sb;

  struct Buffer *path = mutt_buffer_pool_get();
  struct Buffer *msgpath = mutt_buffer_pool_get();
  mutt_buffer_printf(path, "%s/%s", mailbox_path(m), dir_name);

  /* when $mail_check_recent is set, if the new/ directory hasn't been modified since
   * the user last exited the m, then we know there is no recent mail.  */
  if (check_new && C_MailCheckRecent)
  {
    if ((stat(mutt_b2s(path), &sb) == 0) &&
        (mutt_file_stat_timespec_compare(&sb, MUTT_STAT_MTIME, &m->last_visited) < 0))
    {
      check_new = false;
    }
  }

  if (!(check_new || check_stats))
    goto cleanup;

  dirp = opendir(mutt_b2s(path));
  if (!dirp)
  {
    m->type = MUTT_UNKNOWN;
    goto cleanup;
  }

  while ((de = readdir(dirp)))
  {
    if (*de->d_name == '.')
      continue;

    p = strstr(de->d_name, ":2,");
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
        if (C_MailCheckRecent)
        {
          mutt_buffer_printf(msgpath, "%s/%s", mutt_b2s(path), de->d_name);
          /* ensure this message was received since leaving this m */
          if ((stat(mutt_b2s(msgpath), &sb) == 0) &&
              (mutt_file_stat_timespec_compare(&sb, MUTT_STAT_CTIME, &m->last_visited) <= 0))
          {
            continue;
          }
        }
        m->has_new = true;
        check_new = false;
        m->msg_new++;
        if (!check_stats)
          break;
      }
    }
  }

  closedir(dirp);

cleanup:
  mutt_buffer_pool_release(&path);
  mutt_buffer_pool_release(&msgpath);
}

/**
 * ch_compare - qsort callback to sort characters
 * @param a First  character to compare
 * @param b Second character to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int ch_compare(const void *a, const void *b)
{
  return (int) (*((const char *) a) - *((const char *) b));
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

  /* The maildir specification requires that all files in the cur
   * subdirectory have the :unique string appended, regardless of whether
   * or not there are any flags.  If .old is set, we know that this message
   * will end up in the cur directory, so we include it in the following
   * test even though there is no associated flag.  */

  if (e && (e->flagged || e->replied || e->read || e->deleted || e->old || e->maildir_flags))
  {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s%s%s%s%s", e->flagged ? "F" : "", e->replied ? "R" : "",
             e->read ? "S" : "", e->deleted ? "T" : "", NONULL(e->maildir_flags));
    if (e->maildir_flags)
      qsort(tmp, strlen(tmp), 1, ch_compare);
    snprintf(dest, destlen, ":2,%s", tmp);
  }
}

/**
 * maildir_sync_message - Sync an email to a Maildir folder
 * @param m     Mailbox
 * @param msgno Index number
 * @retval  0 Success
 * @retval -1 Error
 */
int maildir_sync_message(struct Mailbox *m, int msgno)
{
  if (!m || !m->emails || (msgno >= m->msg_count))
    return -1;

  struct Email *e = m->emails[msgno];
  if (!e)
    return -1;

  struct Buffer *newpath = NULL;
  struct Buffer *partpath = NULL;
  struct Buffer *fullpath = NULL;
  struct Buffer *oldpath = NULL;
  char suffix[16];
  int rc = 0;

  /* TODO: why the e->env check? */
  if (e->attach_del || (e->env && e->env->changed))
  {
    /* when doing attachment deletion/rethreading, fall back to the MH case. */
    if (mh_rewrite_message(m, msgno) != 0)
      return -1;
    /* TODO: why the env check? */
    if (e->env)
      e->env->changed = 0;
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
    newpath = mutt_buffer_pool_get();
    partpath = mutt_buffer_pool_get();
    fullpath = mutt_buffer_pool_get();
    oldpath = mutt_buffer_pool_get();

    mutt_buffer_strcpy(newpath, p);

    /* kill the previous flags */
    p = strchr(newpath->data, ':');
    if (p)
    {
      *p = '\0';
      newpath->dptr = p; /* fix buffer up, just to be safe */
    }

    maildir_gen_flags(suffix, sizeof(suffix), e);

    mutt_buffer_printf(partpath, "%s/%s%s", (e->read || e->old) ? "cur" : "new",
                       mutt_b2s(newpath), suffix);
    mutt_buffer_printf(fullpath, "%s/%s", mailbox_path(m), mutt_b2s(partpath));
    mutt_buffer_printf(oldpath, "%s/%s", mailbox_path(m), e->path);

    if (mutt_str_equal(mutt_b2s(fullpath), mutt_b2s(oldpath)))
    {
      /* message hasn't really changed */
      goto cleanup;
    }

    /* record that the message is possibly marked as trashed on disk */
    e->trash = e->deleted;

    if (rename(mutt_b2s(oldpath), mutt_b2s(fullpath)) != 0)
    {
      mutt_perror("rename");
      rc = -1;
      goto cleanup;
    }
    mutt_str_replace(&e->path, mutt_b2s(partpath));
  }

cleanup:
  mutt_buffer_pool_release(&newpath);
  mutt_buffer_pool_release(&partpath);
  mutt_buffer_pool_release(&fullpath);
  mutt_buffer_pool_release(&oldpath);

  return rc;
}

/**
 * maildir_mbox_open - Open a Mailbox - Implements MxOps::mbox_open()
 */
static int maildir_mbox_open(struct Mailbox *m)
{
  /* maildir looks sort of like MH, except that there are two subdirectories
   * of the main folder path from which to read messages */
  if ((mh_read_dir(m, "new") == -1) || (mh_read_dir(m, "cur") == -1))
    return -1;

  return 0;
}

/**
 * maildir_mbox_open_append - Open a Mailbox for appending - Implements MxOps::mbox_open_append()
 */
static int maildir_mbox_open_append(struct Mailbox *m, OpenMailboxFlags flags)
{
  if (!m)
    return -1;

  if (!(flags & (MUTT_APPEND | MUTT_APPENDNEW | MUTT_NEWFOLDER)))
  {
    return 0;
  }

  errno = 0;
  if ((mutt_file_mkdir(mailbox_path(m), S_IRWXU) != 0) && (errno != EEXIST))
  {
    mutt_perror(mailbox_path(m));
    return -1;
  }

  char tmp[PATH_MAX];
  snprintf(tmp, sizeof(tmp), "%s/cur", mailbox_path(m));
  errno = 0;
  if ((mkdir(tmp, S_IRWXU) != 0) && (errno != EEXIST))
  {
    mutt_perror(tmp);
    rmdir(mailbox_path(m));
    return -1;
  }

  snprintf(tmp, sizeof(tmp), "%s/new", mailbox_path(m));
  errno = 0;
  if ((mkdir(tmp, S_IRWXU) != 0) && (errno != EEXIST))
  {
    mutt_perror(tmp);
    snprintf(tmp, sizeof(tmp), "%s/cur", mailbox_path(m));
    rmdir(tmp);
    rmdir(mailbox_path(m));
    return -1;
  }

  snprintf(tmp, sizeof(tmp), "%s/tmp", mailbox_path(m));
  errno = 0;
  if ((mkdir(tmp, S_IRWXU) != 0) && (errno != EEXIST))
  {
    mutt_perror(tmp);
    snprintf(tmp, sizeof(tmp), "%s/cur", mailbox_path(m));
    rmdir(tmp);
    snprintf(tmp, sizeof(tmp), "%s/new", mailbox_path(m));
    rmdir(tmp);
    rmdir(mailbox_path(m));
    return -1;
  }

  return 0;
}

/**
 * maildir_mbox_check - Check for new mail - Implements MxOps::mbox_check()
 *
 * This function handles arrival of new mail and reopening of maildir folders.
 * The basic idea here is we check to see if either the new or cur
 * subdirectories have changed, and if so, we scan them for the list of files.
 * We check for newly added messages, and then merge the flags messages we
 * already knew about.  We don't treat either subdirectory differently, as mail
 * could be copied directly into the cur directory from another agent.
 */
int maildir_mbox_check(struct Mailbox *m)
{
  if (!m)
    return -1;

  struct stat st_new;         /* status of the "new" subdirectory */
  struct stat st_cur;         /* status of the "cur" subdirectory */
  int changed = MMC_NO_DIRS;  /* which subdirectories have changed */
  bool occult = false;        /* messages were removed from the mailbox */
  int num_new = 0;            /* number of new messages added to the mailbox */
  bool flags_changed = false; /* message flags were changed in the mailbox */
  struct Maildir *md = NULL;  /* list of messages in the mailbox */
  struct Maildir **last = NULL;
  struct Maildir *p = NULL;
  int count = 0;
  struct HashTable *fnames = NULL; /* hash table for quickly looking up the base filename
                                 for a maildir message */
  struct MaildirMboxData *mdata = maildir_mdata_get(m);

  /* XXX seems like this check belongs in mx_mbox_check() rather than here.  */
  if (!C_CheckNew)
    return 0;

  struct Buffer *buf = mutt_buffer_pool_get();
  mutt_buffer_printf(buf, "%s/new", mailbox_path(m));
  if (stat(mutt_b2s(buf), &st_new) == -1)
  {
    mutt_buffer_pool_release(&buf);
    return -1;
  }

  mutt_buffer_printf(buf, "%s/cur", mailbox_path(m));
  if (stat(mutt_b2s(buf), &st_cur) == -1)
  {
    mutt_buffer_pool_release(&buf);
    return -1;
  }

  /* determine which subdirectories need to be scanned */
  if (mutt_file_stat_timespec_compare(&st_new, MUTT_STAT_MTIME, &m->mtime) > 0)
    changed = MMC_NEW_DIR;
  if (mutt_file_stat_timespec_compare(&st_cur, MUTT_STAT_MTIME, &mdata->mtime_cur) > 0)
    changed |= MMC_CUR_DIR;

  if (changed == MMC_NO_DIRS)
  {
    mutt_buffer_pool_release(&buf);
    return 0; /* nothing to do */
  }

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
    mutt_file_get_stat_timespec(&m->mtime, &st_new, MUTT_STAT_MTIME);
  }

  /* do a fast scan of just the filenames in
   * the subdirectories that have changed.  */
  md = NULL;
  last = &md;
  if (changed & MMC_NEW_DIR)
    maildir_parse_dir(m, &last, "new", &count, NULL);
  if (changed & MMC_CUR_DIR)
    maildir_parse_dir(m, &last, "cur", &count, NULL);

  /* we create a hash table keyed off the canonical (sans flags) filename
   * of each message we scanned.  This is used in the loop over the
   * existing messages below to do some correlation.  */
  fnames = mutt_hash_new(count, MUTT_HASH_NO_FLAGS);

  for (p = md; p; p = p->next)
  {
    maildir_canon_filename(buf, p->email->path);
    p->canon_fname = mutt_buffer_strdup(buf);
    mutt_hash_insert(fnames, p->canon_fname, p);
  }

  /* check for modifications and adjust flags */
  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;

    e->active = false;
    maildir_canon_filename(buf, e->path);
    p = mutt_hash_find(fnames, mutt_b2s(buf));
    if (p && p->email)
    {
      /* message already exists, merge flags */
      e->active = true;

      /* check to see if the message has moved to a different
       * subdirectory.  If so, update the associated filename.  */
      if (!mutt_str_equal(e->path, p->email->path))
        mutt_str_replace(&e->path, p->email->path);

      /* if the user hasn't modified the flags on this message, update
       * the flags we just detected.  */
      if (!e->changed)
        if (maildir_update_flags(m, e, p->email))
          flags_changed = true;

      if (e->deleted == e->trash)
      {
        if (e->deleted != p->email->deleted)
        {
          e->deleted = p->email->deleted;
          flags_changed = true;
        }
      }
      e->trash = p->email->trash;

      /* this is a duplicate of an existing email, so remove it */
      email_free(&p->email);
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
      e->active = true;
    }
  }

  /* destroy the file name hash */
  mutt_hash_free(&fnames);

  /* If we didn't just get new mail, update the tables. */
  if (occult)
    mailbox_changed(m, NT_MAILBOX_RESORT);

  /* do any delayed parsing we need to do. */
  maildir_delayed_parsing(m, &md, NULL);

  /* Incorporate new messages */
  num_new = maildir_move_to_mailbox(m, &md);
  if (num_new > 0)
  {
    mailbox_changed(m, NT_MAILBOX_INVALID);
    m->changed = true;
  }

  mutt_buffer_pool_release(&buf);

  if (occult)
    return MUTT_REOPENED;
  if (num_new > 0)
    return MUTT_NEW_MAIL;
  if (flags_changed)
    return MUTT_FLAGS;
  return 0;
}

/**
 * maildir_mbox_check_stats - Check the Mailbox statistics - Implements MxOps::mbox_check_stats()
 */
static int maildir_mbox_check_stats(struct Mailbox *m, int flags)
{
  if (!m)
    return -1;

  bool check_stats = flags;
  bool check_new = true;

  if (check_stats)
  {
    m->msg_count = 0;
    m->msg_unread = 0;
    m->msg_flagged = 0;
    m->msg_new = 0;
  }

  maildir_check_dir(m, "new", check_new, check_stats);

  check_new = !m->has_new && C_MaildirCheckCur;
  if (check_new || check_stats)
    maildir_check_dir(m, "cur", check_new, check_stats);

  return m->msg_new;
}

/**
 * maildir_msg_open - Open an email message in a Mailbox - Implements MxOps::msg_open()
 */
static int maildir_msg_open(struct Mailbox *m, struct Message *msg, int msgno)
{
  if (!m)
    return -1;
  return maildir_mh_open_message(m, msg, msgno, true);
}

/**
 * maildir_msg_open_new - Open a new message in a Mailbox - Implements MxOps::msg_open_new()
 *
 * Open a new (temporary) message in a maildir folder.
 *
 * @note This uses _almost_ the maildir file name format,
 * but with a {cur,new} prefix.
 */
int maildir_msg_open_new(struct Mailbox *m, struct Message *msg, struct Email *e)
{
  if (!m)
    return -1;

  int fd;
  char path[PATH_MAX];
  char suffix[16];
  char subdir[16];

  if (e)
  {
    bool deleted = e->deleted;
    e->deleted = false;

    maildir_gen_flags(suffix, sizeof(suffix), e);

    e->deleted = deleted;
  }
  else
    *suffix = '\0';

  if (e && (e->read || e->old))
    mutt_str_copy(subdir, "cur", sizeof(subdir));
  else
    mutt_str_copy(subdir, "new", sizeof(subdir));

  mode_t omask = umask(mh_umask(m));
  while (true)
  {
    snprintf(path, sizeof(path), "%s/tmp/%s.%lld.R%" PRIu64 ".%s%s",
             mailbox_path(m), subdir, (long long) mutt_date_epoch(),
             mutt_rand64(), NONULL(ShortHostname), suffix);

    mutt_debug(LL_DEBUG2, "Trying %s\n", path);

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
    return -1;
  }

  return 0;
}

/**
 * maildir_msg_commit - Save changes to an email - Implements MxOps::msg_commit()
 */
static int maildir_msg_commit(struct Mailbox *m, struct Message *msg)
{
  if (!m)
    return -1;

  return md_commit_message(m, msg, NULL);
}

/**
 * maildir_msg_save_hcache - Save message to the header cache - Implements MxOps::msg_save_hcache()
 */
static int maildir_msg_save_hcache(struct Mailbox *m, struct Email *e)
{
  int rc = 0;
#ifdef USE_HCACHE
  header_cache_t *hc = mutt_hcache_open(C_HeaderCache, mailbox_path(m), NULL);
  char *key = e->path + 3;
  int keylen = maildir_hcache_keylen(key);
  rc = mutt_hcache_store(hc, key, keylen, e, 0);
  mutt_hcache_close(hc);
#endif
  return rc;
}

/**
 * maildir_path_probe - Is this a Maildir Mailbox? - Implements MxOps::path_probe()
 */
static enum MailboxType maildir_path_probe(const char *path, const struct stat *st)
{
  if (!path)
    return MUTT_UNKNOWN;

  if (!st || !S_ISDIR(st->st_mode))
    return MUTT_UNKNOWN;

  char cur[PATH_MAX];
  snprintf(cur, sizeof(cur), "%s/cur", path);

  struct stat stc;
  if ((stat(cur, &stc) == 0) && S_ISDIR(stc.st_mode))
    return MUTT_MAILDIR;

  return MUTT_UNKNOWN;
}

// clang-format off
/**
 * MxMaildirOps - Maildir Mailbox - Implements ::MxOps
 */
struct MxOps MxMaildirOps = {
  .type            = MUTT_MAILDIR,
  .name             = "maildir",
  .is_local         = true,
  .ac_find          = maildir_ac_find,
  .ac_add           = maildir_ac_add,
  .mbox_open        = maildir_mbox_open,
  .mbox_open_append = maildir_mbox_open_append,
  .mbox_check       = maildir_mbox_check,
  .mbox_check_stats = maildir_mbox_check_stats,
  .mbox_sync        = mh_mbox_sync,
  .mbox_close       = mh_mbox_close,
  .msg_open         = maildir_msg_open,
  .msg_open_new     = maildir_msg_open_new,
  .msg_commit       = maildir_msg_commit,
  .msg_close        = mh_msg_close,
  .msg_padding_size = NULL,
  .msg_save_hcache  = maildir_msg_save_hcache,
  .tags_edit        = NULL,
  .tags_commit      = NULL,
  .path_probe       = maildir_path_probe,
  .path_canon       = maildir_path_canon,
  .path_pretty      = maildir_path_pretty,
  .path_parent      = maildir_path_parent,
};
// clang-format on
