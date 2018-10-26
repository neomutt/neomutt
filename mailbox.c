/**
 * @file
 * Representation of a mailbox
 *
 * @authors
 * Copyright (C) 1996-2000,2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2016-2017 Kevin J. McCarthy <kevin@8t8.us>
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

#include "config.h"
#include <dirent.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <utime.h>
#include "mutt/mutt.h"
#include "config/lib.h"
#include "email/lib.h"
#include "mutt.h"
#include "mailbox.h"
#include "account.h"
#include "context.h"
#include "globals.h"
#include "maildir/maildir.h"
#include "mbox/mbox.h"
#include "menu.h"
#include "mutt_window.h"
#include "muttlib.h"
#include "mx.h"
#include "protos.h"
#ifdef USE_SIDEBAR
#include "sidebar.h"
#endif
#ifdef USE_IMAP
#include "imap/imap.h"
#endif
#ifdef USE_NOTMUCH
#include "notmuch/mutt_notmuch.h"
#endif
#ifdef USE_NNTP
#include "nntp/nntp.h"
#endif
#ifdef USE_POP
#include "pop/pop.h"
#endif
#ifdef USE_INOTIFY
#include "monitor.h"
#endif

/* These Config Variables are only used in mailbox.c */
short MailCheck; ///< Config: Number of seconds before NeoMutt checks for new mail
bool MailCheckStats;          ///< Config: Periodically check for new mail
short MailCheckStatsInterval; ///< Config: How often to check for new mail
bool MaildirCheckCur; ///< Config: Check both 'new' and 'cur' directories for new mail

static time_t MailboxTime = 0; /**< last time we started checking for mail */
static time_t MailboxStatsTime = 0; /**< last time we check performed mail_check_stats */
static short MailboxCount = 0;  /**< how many boxes with new mail */
static short MailboxNotify = 0; /**< # of unnotified new boxes */

struct MailboxList AllMailboxes = STAILQ_HEAD_INITIALIZER(AllMailboxes);

/**
 * mailbox_new - Create a new Mailbox
 * @retval ptr New Mailbox
 */
struct Mailbox *mailbox_new(void)
{
  // char rp[PATH_MAX] = "";

  struct Mailbox *m = mutt_mem_calloc(1, sizeof(struct Mailbox));
  // mutt_str_strfcpy(m->path, path, sizeof(m->path));
  // char *r = realpath(path, rp);
  // mutt_str_strfcpy(m->realpath, r ? rp : path, sizeof(m->realpath));
  // m->magic = MUTT_UNKNOWN;
  // m->desc = get_mailbox_description(m->path);

  return m;
}

/**
 * mailbox_free - Free a Mailbox
 * @param m Mailbox to free
 */
void mailbox_free(struct Mailbox **m)
{
  if (!m || !*m)
    return;

  FREE(&(*m)->desc);
  if ((*m)->mdata && (*m)->free_mdata)
    (*m)->free_mdata(&(*m)->mdata);
  FREE(m);
}

/**
 * mailbox_maildir_check_dir - Check for new mail / mail counts
 * @param m           Mailbox to check
 * @param dir_name    Path to Mailbox
 * @param check_new   if true, check for new mail
 * @param check_stats if true, count total, new, and flagged messages
 * @retval 1 if the dir has new mail
 *
 * Checks the specified maildir subdir (cur or new) for new mail or mail counts.
 */
static int mailbox_maildir_check_dir(struct Mailbox *m, const char *dir_name,
                                     bool check_new, bool check_stats)
{
  DIR *dirp = NULL;
  struct dirent *de = NULL;
  char *p = NULL;
  int rc = 0;
  struct stat sb;

  struct Buffer *path = mutt_buffer_pool_get();
  struct Buffer *msgpath = mutt_buffer_pool_get();
  mutt_buffer_printf(path, "%s/%s", m->path, dir_name);

  /* when $mail_check_recent is set, if the new/ directory hasn't been modified since
   * the user last exited the m, then we know there is no recent mail.
   */
  if (check_new && MailCheckRecent)
  {
    if (stat(mutt_b2s(path), &sb) == 0 &&
        mutt_stat_timespec_compare(&sb, MUTT_STAT_MTIME, &m->last_visited) < 0)
    {
      rc = 0;
      check_new = false;
    }
  }

  if (!(check_new || check_stats))
    goto cleanup;

  dirp = opendir(mutt_b2s(path));
  if (!dirp)
  {
    m->magic = MUTT_UNKNOWN;
    rc = 0;
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
        if (MailCheckRecent)
        {
          mutt_buffer_printf(msgpath, "%s/%s", mutt_b2s(path), de->d_name);
          /* ensure this message was received since leaving this m */
          if (stat(mutt_b2s(msgpath), &sb) == 0 &&
              (mutt_stat_timespec_compare(&sb, MUTT_STAT_CTIME, &m->last_visited) <= 0))
          {
            continue;
          }
        }
        m->has_new = true;
        rc = 1;
        check_new = false;
        if (!check_stats)
          break;
      }
    }
  }

  closedir(dirp);

cleanup:
  mutt_buffer_pool_release(&path);
  mutt_buffer_pool_release(&msgpath);

  return rc;
}

/**
 * mailbox_maildir_check - Check for new mail in a maildir mailbox
 * @param m           Mailbox to check
 * @param check_stats if true, also count total, new, and flagged messages
 * @retval 1 if the mailbox has new mail
 */
static int mailbox_maildir_check(struct Mailbox *m, bool check_stats)
{
  int rc = 1;
  bool check_new = true;

  if (check_stats)
  {
    m->msg_count = 0;
    m->msg_unread = 0;
    m->msg_flagged = 0;
  }

  rc = mailbox_maildir_check_dir(m, "new", check_new, check_stats);

  check_new = !rc && MaildirCheckCur;
  if (check_new || check_stats)
    if (mailbox_maildir_check_dir(m, "cur", check_new, check_stats))
      rc = 1;

  return rc;
}

/**
 * mailbox_mbox_check - Check for new mail for an mbox mailbox
 * @param m           Mailbox to check
 * @param sb          stat(2) information about the mailbox
 * @param check_stats if true, also count total, new, and flagged messages
 * @retval 1 if the mailbox has new mail
 */
static int mailbox_mbox_check(struct Mailbox *m, struct stat *sb, bool check_stats)
{
  int rc = 0;
  bool new_or_changed;

  if (CheckMboxSize)
    new_or_changed = (sb->st_size > m->size);
  else
  {
    new_or_changed =
        (mutt_stat_compare(sb, MUTT_STAT_MTIME, sb, MUTT_STAT_ATIME) > 0) ||
        (m->newly_created &&
         (mutt_stat_compare(sb, MUTT_STAT_CTIME, sb, MUTT_STAT_MTIME) == 0) &&
         (mutt_stat_compare(sb, MUTT_STAT_CTIME, sb, MUTT_STAT_ATIME) == 0));
  }

  if (new_or_changed)
  {
    if (!MailCheckRecent ||
        (mutt_stat_timespec_compare(sb, MUTT_STAT_MTIME, &m->last_visited) > 0))
    {
      rc = 1;
      m->has_new = true;
    }
  }
  else if (CheckMboxSize)
  {
    /* some other program has deleted mail from the folder */
    m->size = (off_t) sb->st_size;
  }

  if (m->newly_created && (sb->st_ctime != sb->st_mtime || sb->st_ctime != sb->st_atime))
    m->newly_created = false;

  if (check_stats &&
      (mutt_stat_timespec_compare(sb, MUTT_STAT_MTIME, &m->stats_last_checked) > 0))
  {
    struct Context *ctx =
        mx_mbox_open(NULL, m->path, MUTT_READONLY | MUTT_QUIET | MUTT_NOSORT | MUTT_PEEK);
    if (ctx)
    {
      m->msg_count = ctx->mailbox->msg_count;
      m->msg_unread = ctx->mailbox->msg_unread;
      m->msg_flagged = ctx->mailbox->msg_flagged;
      m->stats_last_checked = ctx->mailbox->mtime;
      mx_mbox_close(&ctx, NULL);
    }
  }

  return rc;
}

/**
 * mailbox_check - Check a mailbox for new mail
 * @param m           Mailbox to check
 * @param ctx_sb      stat() info for the current mailbox (Context)
 * @param check_stats If true, also count the total, new and flagged messages
 */
static void mailbox_check(struct Mailbox *m, struct stat *ctx_sb, bool check_stats)
{
  struct stat sb = { 0 };

#ifdef USE_SIDEBAR
  short orig_new = m->has_new;
  int orig_count = m->msg_count;
  int orig_unread = m->msg_unread;
  int orig_flagged = m->msg_flagged;
#endif

  if (m->magic != MUTT_IMAP)
  {
    m->has_new = false;
#ifdef USE_POP
    if (pop_path_probe(m->path, NULL) == MUTT_POP)
    {
      m->magic = MUTT_POP;
    }
    else
#endif
#ifdef USE_NNTP
        if ((m->magic == MUTT_NNTP) || (nntp_path_probe(m->path, NULL) == MUTT_NNTP))
    {
      m->magic = MUTT_NNTP;
    }
    else
#endif
#ifdef USE_NOTMUCH
        if (nm_path_probe(m->path, NULL) == MUTT_NOTMUCH)
    {
      m->magic = MUTT_NOTMUCH;
    }
    else
#endif
        if (stat(m->path, &sb) != 0 || (S_ISREG(sb.st_mode) && sb.st_size == 0) ||
            ((m->magic == MUTT_UNKNOWN) && (m->magic = mx_path_probe(m->path, NULL)) <= 0))
    {
      /* if the mailbox still doesn't exist, set the newly created flag to be
       * ready for when it does. */
      m->newly_created = true;
      m->magic = MUTT_UNKNOWN;
      m->size = 0;
      return;
    }
  }

  /* check to see if the folder is the currently selected folder before polling */
  if (!Context || (Context->mailbox->path[0] == '\0') ||
      ((m->magic == MUTT_IMAP ||
#ifdef USE_NNTP
        m->magic == MUTT_NNTP ||
#endif
#ifdef USE_NOTMUCH
        m->magic == MUTT_NOTMUCH ||
#endif
        m->magic == MUTT_POP) ?
           (mutt_str_strcmp(m->path, Context->mailbox->path) != 0) :
           (sb.st_dev != ctx_sb->st_dev || sb.st_ino != ctx_sb->st_ino)))
  {
    switch (m->magic)
    {
      case MUTT_MBOX:
      case MUTT_MMDF:
        if (mailbox_mbox_check(m, &sb, check_stats) > 0)
          MailboxCount++;
        break;

      case MUTT_MAILDIR:
        if (mailbox_maildir_check(m, check_stats) > 0)
          MailboxCount++;
        break;

      case MUTT_MH:
        if (mh_mailbox(m, check_stats))
          MailboxCount++;
        break;
#ifdef USE_NOTMUCH
      case MUTT_NOTMUCH:
        m->msg_count = 0;
        m->msg_unread = 0;
        m->msg_flagged = 0;
        nm_nonctx_get_count(m->path, &m->msg_count, &m->msg_unread);
        if (m->msg_unread > 0)
        {
          MailboxCount++;
          m->has_new = true;
        }
        break;
#endif
      default:; /* do nothing */
    }
  }
  else if (CheckMboxSize && Context && (Context->mailbox->path[0] != '\0'))
    m->size = (off_t) sb.st_size; /* update the size of current folder */

#ifdef USE_SIDEBAR
  if ((orig_new != m->has_new) || (orig_count != m->msg_count) ||
      (orig_unread != m->msg_unread) || (orig_flagged != m->msg_flagged))
  {
    mutt_menu_set_current_redraw(REDRAW_SIDEBAR);
  }
#endif

  if (!m->has_new)
    m->notified = false;
  else if (!m->notified)
    MailboxNotify++;
}

/**
 * mailbox_get - Fetch mailbox object for given path, if present
 * @param path Path to the mailbox
 * @retval ptr Mailbox for the path
 */
static struct Mailbox *mailbox_get(const char *path)
{
  if (!path)
    return NULL;

  char *epath = mutt_str_strdup(path);
  if (!epath)
    return NULL;

  mutt_expand_path(epath, mutt_str_strlen(epath));

  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &AllMailboxes, entries)
  {
    /* must be done late because e.g. IMAP delimiter may change */
    mutt_expand_path(np->m->path, sizeof(np->m->path));
    if (mutt_str_strcmp(np->m->path, path) == 0)
    {
      FREE(&epath);
      return np->m;
    }
  }

  FREE(&epath);
  return NULL;
}

/**
 * mutt_mailbox_cleanup - Restore the timestamp of a mailbox
 * @param path Path to the mailbox
 * @param st   Timestamp info from stat()
 *
 * Fix up the atime and mtime after mbox/mmdf mailbox was modified according to
 * stat() info taken before a modification.
 */
void mutt_mailbox_cleanup(const char *path, struct stat *st)
{
#ifdef HAVE_UTIMENSAT
  struct timespec ts[2];
#else
  struct utimbuf ut;
#endif

  if (CheckMboxSize)
  {
    struct Mailbox *m = mutt_find_mailbox(path);
    if (m && !m->has_new)
      mutt_update_mailbox(m);
  }
  else
  {
    /* fix up the times so mailbox won't get confused */
    if (st->st_mtime > st->st_atime)
    {
#ifdef HAVE_UTIMENSAT
      ts[0].tv_sec = 0;
      ts[0].tv_nsec = UTIME_OMIT;
      ts[1].tv_sec = 0;
      ts[1].tv_nsec = UTIME_NOW;
      utimensat(0, buf, ts, 0);
#else
      ut.actime = st->st_atime;
      ut.modtime = time(NULL);
      utime(path, &ut);
#endif
    }
    else
    {
#ifdef HAVE_UTIMENSAT
      ts[0].tv_sec = 0;
      ts[0].tv_nsec = UTIME_NOW;
      ts[1].tv_sec = 0;
      ts[1].tv_nsec = UTIME_NOW;
      utimensat(0, buf, ts, 0);
#else
      utime(path, NULL);
#endif
    }
  }
}

/**
 * mutt_find_mailbox - Find the mailbox with a given path
 * @param path Path to match
 * @retval ptr Matching Mailbox
 */
struct Mailbox *mutt_find_mailbox(const char *path)
{
  if (!path)
    return NULL;

  struct stat sb;
  struct stat tmp_sb;

  if (stat(path, &sb) != 0)
    return NULL;

  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &AllMailboxes, entries)
  {
    if ((stat(np->m->path, &tmp_sb) == 0) && (sb.st_dev == tmp_sb.st_dev) &&
        (sb.st_ino == tmp_sb.st_ino))
    {
      return np->m;
    }
  }

  return NULL;
}

/**
 * mutt_update_mailbox - Get the mailbox's current size
 * @param m Mailbox to check
 */
void mutt_update_mailbox(struct Mailbox *m)
{
  struct stat sb;

  if (!m)
    return;

  if (stat(m->path, &sb) == 0)
    m->size = (off_t) sb.st_size;
  else
    m->size = 0;
}

/**
 * mutt_parse_mailboxes - Parse the 'mailboxes' command - Implements ::command_t
 *
 * This is also used by 'virtual-mailboxes'.
 */
int mutt_parse_mailboxes(struct Buffer *buf, struct Buffer *s,
                         unsigned long data, struct Buffer *err)
{
  // char canon[PATH_MAX];
  // struct stat sb = { 0 };
#if 0
  char f1[PATH_MAX];
  char *p = NULL;
#endif

  while (MoreArgs(s))
  {
    // char *desc = NULL;

    struct Mailbox *m = mailbox_new();

    if (data & MUTT_NAMED)
    {
      mutt_extract_token(buf, s, 0);
      if (buf->data && *buf->data)
      {
        m->desc = mutt_str_strdup(buf->data);
      }
      else
      {
        mailbox_free(&m);
        continue;
      }
    }

    mutt_extract_token(buf, s, 0);
    if (mutt_buffer_is_empty(buf))
    {
      /* Skip empty tokens. */
      mailbox_free(&m);
      continue;
    }

    mutt_str_strfcpy(m->path, buf->data, sizeof(m->path));
    /* int rc = */ mx_path_canon2(m, Folder);

    bool new_account = false;
    struct Account *a = mx_ac_find(m);
    if (!a)
    {
      a = account_create();
      a->type = m->magic;
      TAILQ_INSERT_TAIL(&AllAccounts, a, entries);
      new_account = true;
    }

    if (!new_account)
    {
      struct Mailbox *old_m = mx_mbox_find(a, m);
      if (old_m)
      {
        if (old_m->flags == MB_HIDDEN)
        {
          old_m->flags = MB_NORMAL;
          mutt_sb_notify_mailbox(old_m, true);
          struct MailboxNode *mn = mutt_mem_calloc(1, sizeof(*mn));
          mn->m = m;
          STAILQ_INSERT_TAIL(&AllMailboxes, mn, entries);
        }
        else
        {
          // mutt_error("mailbox exists: %s", m->path);
          mailbox_free(&m);
        }
        continue;
      }
    }

    if (mx_ac_add(a, m) < 0)
    {
      //error
      mailbox_free(&m);
      continue;
    }

    // SUCCESS

#if 0
#ifdef USE_NOTMUCH
    if (nm_path_probe(buf->data, NULL) == MUTT_NOTMUCH)
      nm_normalize_uri(buf->data, canon, sizeof(canon));
    else
#endif
      mutt_str_strfcpy(canon, buf->data, sizeof(canon));

    mutt_expand_path(canon, sizeof(canon));

    /* Skip empty tokens. */
    if (!*canon)
    {
      FREE(&desc);
      continue;
    }

    /* avoid duplicates */
    p = realpath(canon, f1);
    struct MailboxNode *np = NULL;
    STAILQ_FOREACH(np, &AllMailboxes, entries)
    {
      if (mutt_str_strcmp(p ? p : canon, np->m->realpath) == 0)
      {
        mutt_debug(3, "mailbox '%s' already registered as '%s'\n", canon, np->m->path);
        break;
      }
    }

    if (np)
    {
      FREE(&desc);
      continue;
    }

    m = mailbox_new(canon);

    m->notified = true;
    m->desc = desc;
#ifdef USE_NOTMUCH
    if (nm_path_probe(m->path, NULL) == MUTT_NOTMUCH)
    {
      m->magic = MUTT_NOTMUCH;
    }
    else
#endif
    {
      /* for check_mbox_size, it is important that if the folder is new (tested by
       * reading it), the size is set to 0 so that later when we check we see
       * that it increased. without check_mbox_size we probably don't care.
       */
      if (CheckMboxSize && (stat(m->path, &sb) == 0) && !mbox_test_new_folder(m->path))
      {
        /* some systems out there don't have an off_t type */
        m->size = (off_t) sb.st_size;
      }
    }
#endif

    struct MailboxNode *mn = mutt_mem_calloc(1, sizeof(*mn));
    mn->m = m;
    STAILQ_INSERT_TAIL(&AllMailboxes, mn, entries);

#ifdef USE_SIDEBAR
    mutt_sb_notify_mailbox(m, true);
#endif
#ifdef USE_INOTIFY
    m->magic = mx_path_probe(m->path, NULL);
    mutt_monitor_add(m);
#endif
  }
  return 0;
}

/**
 * mutt_parse_unmailboxes - Parse the 'unmailboxes' command - Implements ::command_t
 *
 * This is also used by 'unvirtual-mailboxes'
 */
int mutt_parse_unmailboxes(struct Buffer *buf, struct Buffer *s,
                           unsigned long data, struct Buffer *err)
{
  char tmp[PATH_MAX];
  bool clear_all = false;

  while (!clear_all && MoreArgs(s))
  {
    mutt_extract_token(buf, s, 0);

    if (mutt_str_strcmp(buf->data, "*") == 0)
    {
      clear_all = true;
    }
    else
    {
#ifdef USE_NOTMUCH
      if (nm_path_probe(buf->data, NULL) == MUTT_NOTMUCH)
      {
        nm_normalize_uri(buf->data, tmp, sizeof(tmp));
      }
      else
#endif
      {
        mutt_str_strfcpy(tmp, buf->data, sizeof(tmp));
        mutt_expand_path(tmp, sizeof(tmp));
      }
    }

    struct MailboxNode *np = NULL;
    struct MailboxNode *nptmp = NULL;
    STAILQ_FOREACH_SAFE(np, &AllMailboxes, entries, nptmp)
    {
      /* Decide whether to delete all normal mailboxes or all virtual */
      bool virt = ((np->m->magic == MUTT_NOTMUCH) && (data & MUTT_VIRTUAL));
      bool norm = ((np->m->magic != MUTT_NOTMUCH) && !(data & MUTT_VIRTUAL));
      bool clear_this = clear_all && (virt || norm);

      if (clear_this || (mutt_str_strcasecmp(tmp, np->m->path) == 0) ||
          (mutt_str_strcasecmp(tmp, np->m->desc) == 0))
      {
#ifdef USE_SIDEBAR
        mutt_sb_notify_mailbox(np->m, false);
#endif
#ifdef USE_INOTIFY
        mutt_monitor_remove(np->m);
#endif
        STAILQ_REMOVE(&AllMailboxes, np, MailboxNode, entries);
        mailbox_free(&np->m);
        FREE(&np);
        continue;
      }
    }
  }
  return 0;
}

/**
 * mutt_mailbox_check - Check all AllMailboxes for new mail
 * @param force Force flags, see below
 * @retval num Number of mailboxes with new mail
 *
 * The force argument may be any combination of the following values:
 * - MUTT_MAILBOX_CHECK_FORCE        ignore MailboxTime and check for new mail
 * - MUTT_MAILBOX_CHECK_FORCE_STATS  ignore MailboxTime and calculate statistics
 *
 * Check all AllMailboxes for new mail and total/new/flagged messages
 */
int mutt_mailbox_check(int force)
{
  struct stat contex_sb;
  time_t t;
  bool check_stats = false;
  contex_sb.st_dev = 0;
  contex_sb.st_ino = 0;

#ifdef USE_IMAP
  /* update postponed count as well, on force */
  if (force & MUTT_MAILBOX_CHECK_FORCE)
    mutt_update_num_postponed();
#endif

  /* fastest return if there are no mailboxes */
  if (STAILQ_EMPTY(&AllMailboxes))
    return 0;

  t = time(NULL);
  if (!force && (t - MailboxTime < MailCheck))
    return MailboxCount;

  if ((force & MUTT_MAILBOX_CHECK_FORCE_STATS) ||
      (MailCheckStats && ((t - MailboxStatsTime) >= MailCheckStatsInterval)))
  {
    check_stats = true;
    MailboxStatsTime = t;
  }

  MailboxTime = t;
  MailboxCount = 0;
  MailboxNotify = 0;

#ifdef USE_IMAP
  MailboxCount += imap_mailbox_check(check_stats);
#endif

  /* check device ID and serial number instead of comparing paths */
  if (!Context || !Context->mailbox || (Context->mailbox->magic == MUTT_IMAP) ||
      (Context->mailbox->magic == MUTT_POP)
#ifdef USE_NNTP
      || (Context->mailbox->magic == MUTT_NNTP)
#endif
      || stat(Context->mailbox->path, &contex_sb) != 0)
  {
    contex_sb.st_dev = 0;
    contex_sb.st_ino = 0;
  }

  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &AllMailboxes, entries)
  {
    mailbox_check(np->m, &contex_sb, check_stats);
  }

  return MailboxCount;
}

/**
 * mutt_mailbox_list - List the mailboxes with new mail
 * @retval true If there is new mail
 */
bool mutt_mailbox_list(void)
{
  char path[PATH_MAX];
  char mailboxlist[2 * STRING];
  size_t pos = 0;
  int first = 1;

  int have_unnotified = MailboxNotify;

  mailboxlist[0] = '\0';
  pos += strlen(strncat(mailboxlist, _("New mail in "), sizeof(mailboxlist) - 1 - pos));
  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &AllMailboxes, entries)
  {
    /* Is there new mail in this mailbox? */
    if (!np->m->has_new || (have_unnotified && np->m->notified))
      continue;

    mutt_str_strfcpy(path, np->m->path, sizeof(path));
    mutt_pretty_mailbox(path, sizeof(path));

    if (!first && (MuttMessageWindow->cols >= 7) &&
        (pos + strlen(path) >= (size_t) MuttMessageWindow->cols - 7))
    {
      break;
    }

    if (!first)
      pos += strlen(strncat(mailboxlist + pos, ", ", sizeof(mailboxlist) - 1 - pos));

    /* Prepend an asterisk to mailboxes not already notified */
    if (!np->m->notified)
    {
      /* pos += strlen (strncat(mailboxlist + pos, "*", sizeof(mailboxlist)-1-pos)); */
      np->m->notified = true;
      MailboxNotify--;
    }
    pos += strlen(strncat(mailboxlist + pos, path, sizeof(mailboxlist) - 1 - pos));
    first = 0;
  }

  if (!first && np)
  {
    strncat(mailboxlist + pos, ", ...", sizeof(mailboxlist) - 1 - pos);
  }
  if (!first)
  {
    mutt_message("%s", mailboxlist);
    return true;
  }
  /* there were no mailboxes needing to be notified, so clean up since
   * MailboxNotify has somehow gotten out of sync
   */
  MailboxNotify = 0;
  return false;
}

/**
 * mutt_mailbox_setnotified - Note when the user was last notified of new mail
 * @param path Path to the mailbox
 */
void mutt_mailbox_setnotified(const char *path)
{
  struct Mailbox *m = mailbox_get(path);
  if (!m)
    return;

  m->notified = true;
#if HAVE_CLOCK_GETTIME
  clock_gettime(CLOCK_REALTIME, &m->last_visited);
#else
  m->last_visited.tv_nsec = 0;
  time(&m->last_visited.tv_sec);
#endif
}

/**
 * mutt_mailbox_notify - Notify the user if there's new mail
 * @retval true If there is new mail
 */
bool mutt_mailbox_notify(void)
{
  if (mutt_mailbox_check(0) && MailboxNotify)
  {
    return mutt_mailbox_list();
  }
  return false;
}

/**
 * mutt_mailbox - incoming folders completion routine
 * @param s    Buffer containing name of current mailbox
 * @param slen Buffer length
 *
 * Given a folder name, find the next incoming folder with new mail.
 */
void mutt_mailbox(char *s, size_t slen)
{
  mutt_expand_path(s, slen);

  if (mutt_mailbox_check(0))
  {
    int found = 0;
    for (int pass = 0; pass < 2; pass++)
    {
      struct MailboxNode *np = NULL;
      STAILQ_FOREACH(np, &AllMailboxes, entries)
      {
        if (np->m->magic == MUTT_NOTMUCH) /* only match real mailboxes */
          continue;
        mutt_expand_path(np->m->path, sizeof(np->m->path));
        if ((found || pass) && np->m->has_new)
        {
          mutt_str_strfcpy(s, np->m->path, slen);
          mutt_pretty_mailbox(s, slen);
          return;
        }
        if (mutt_str_strcmp(s, np->m->path) == 0)
          found = 1;
      }
    }

    mutt_mailbox_check(MUTT_MAILBOX_CHECK_FORCE); /* mailbox was wrong - resync things */
  }

  /* no folders with new mail */
  *s = '\0';
}

#ifdef USE_NOTMUCH
/**
 * mutt_mailbox_vfolder - Find the first virtual folder with new mail
 * @param buf    Buffer for the folder name
 * @param buflen Length of the buffer
 */
void mutt_mailbox_vfolder(char *buf, size_t buflen)
{
  if (mutt_mailbox_check(0))
  {
    bool found = false;
    for (int pass = 0; pass < 2; pass++)
    {
      struct MailboxNode *np = NULL;
      STAILQ_FOREACH(np, &AllMailboxes, entries)
      {
        if (np->m->magic != MUTT_NOTMUCH)
          continue;
        if ((found || pass) && np->m->has_new)
        {
          mutt_str_strfcpy(buf, np->m->desc, buflen);
          return;
        }
        if (mutt_str_strcmp(buf, np->m->path) == 0)
          found = true;
      }
    }

    mutt_mailbox_check(MUTT_MAILBOX_CHECK_FORCE); /* Mailbox was wrong - resync things */
  }

  /* no folders with new mail */
  *buf = '\0';
}
#endif

/**
 * mutt_context_free - Free a Context
 * @param ctx Context to free
 */
void mutt_context_free(struct Context **ctx)
{
  if (!ctx || !*ctx)
    return;

  mailbox_free(&(*ctx)->mailbox);
  FREE(ctx);
}
