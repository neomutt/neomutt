/**
 * @file
 * Representation of a mailbox
 *
 * @authors
 * Copyright (C) 1996-2000,2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2016-2017 Kevin J. McCarthy <kevin@8t8.us>
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
#include "email/email.h"
#include "mutt.h"
#include "mailbox.h"
#include "context.h"
#include "globals.h"
#include "maildir/maildir.h"
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
 * fseek_last_message - Find the last message in the file
 * @param f File to search
 * @retval  0 Success
 * @retval -1 No message found
 */
static int fseek_last_message(FILE *f)
{
  LOFF_T pos;
  char buffer[BUFSIZ + 9] = { 0 }; /* 7 for "\n\nFrom " */
  size_t bytes_read;

  fseek(f, 0, SEEK_END);
  pos = ftello(f);

  /* Set `bytes_read' to the size of the last, probably partial, buffer;
   * 0 < `bytes_read' <= `BUFSIZ'.  */
  bytes_read = pos % BUFSIZ;
  if (bytes_read == 0)
    bytes_read = BUFSIZ;
  /* Make `pos' a multiple of `BUFSIZ' (0 if the file is short), so that all
   * reads will be on block boundaries, which might increase efficiency.  */
  while ((pos -= bytes_read) >= 0)
  {
    /* we save in the buffer at the end the first 7 chars from the last read */
    strncpy(buffer + BUFSIZ, buffer, 5 + 2); /* 2 == 2 * mutt_str_strlen(CRLF) */
    fseeko(f, pos, SEEK_SET);
    bytes_read = fread(buffer, sizeof(char), bytes_read, f);
    if (bytes_read == 0)
      return -1;
    /* 'i' is Index into `buffer' for scanning.  */
    for (int i = bytes_read; i >= 0; i--)
    {
      if (mutt_str_strncmp(buffer + i, "\n\nFrom ", mutt_str_strlen("\n\nFrom ")) == 0)
      { /* found it - go to the beginning of the From */
        fseeko(f, pos + i + 2, SEEK_SET);
        return 0;
      }
    }
    bytes_read = BUFSIZ;
  }

  /* here we are at the beginning of the file */
  if (mutt_str_strncmp("From ", buffer, 5) == 0)
  {
    fseek(f, 0, SEEK_SET);
    return 0;
  }

  return -1;
}

/**
 * test_last_status_new - Is the last message new
 * @param f File to check
 * @retval true if the last message is new
 */
static bool test_last_status_new(FILE *f)
{
  struct Header *hdr = NULL;
  struct Envelope *tmp_envelope = NULL;
  bool result = false;

  if (fseek_last_message(f) == -1)
    return false;

  hdr = mutt_header_new();
  tmp_envelope = mutt_rfc822_read_header(f, hdr, false, false);
  if (!(hdr->read || hdr->old))
    result = true;

  mutt_env_free(&tmp_envelope);
  mutt_header_free(&hdr);

  return result;
}

/**
 * test_new_folder - Test if an mbox or mmdf mailbox has new mail
 * @param path Path to the mailbox
 * @retval bool true if the folder contains new mail
 */
static bool test_new_folder(const char *path)
{
  FILE *f = NULL;
  bool rc = false;

  enum MailboxType magic = mx_path_probe(path, NULL);

  if ((magic != MUTT_MBOX) && (magic != MUTT_MMDF))
    return false;

  f = fopen(path, "rb");
  if (f)
  {
    rc = test_last_status_new(f);
    mutt_file_fclose(&f);
  }

  return rc;
}

/**
 * mailbox_new - Create a new Maibox
 * @param path Path to the mailbox
 * @retval ptr New Mailbox
 */
static struct Mailbox *mailbox_new(const char *path)
{
  char rp[PATH_MAX] = "";

  struct Mailbox *mailbox = mutt_mem_calloc(1, sizeof(struct Mailbox));
  mutt_str_strfcpy(mailbox->path, path, sizeof(mailbox->path));
  char *r = realpath(path, rp);
  mutt_str_strfcpy(mailbox->realpath, r ? rp : path, sizeof(mailbox->realpath));
  mailbox->magic = MUTT_UNKNOWN;

  return mailbox;
}

/**
 * mailbox_free - Free a Mailbox
 * @param mailbox Mailbox to free
 */
static void mailbox_free(struct Mailbox **mailbox)
{
  if (mailbox && *mailbox)
    FREE(&(*mailbox)->desc);
  FREE(mailbox);
}

/**
 * mailbox_maildir_check_dir - Check for new mail / mail counts
 * @param mailbox     Mailbox to check
 * @param dir_name    Path to mailbox
 * @param check_new   if true, check for new mail
 * @param check_stats if true, count total, new, and flagged messages
 * @retval 1 if the dir has new mail
 *
 * Checks the specified maildir subdir (cur or new) for new mail or mail counts.
 */
static int mailbox_maildir_check_dir(struct Mailbox *mailbox, const char *dir_name,
                                     bool check_new, bool check_stats)
{
  char path[PATH_MAX];
  char msgpath[PATH_MAX];
  DIR *dirp = NULL;
  struct dirent *de = NULL;
  char *p = NULL;
  int rc = 0;
  struct stat sb;

  snprintf(path, sizeof(path), "%s/%s", mailbox->path, dir_name);

  /* when $mail_check_recent is set, if the new/ directory hasn't been modified since
   * the user last exited the mailbox, then we know there is no recent mail.
   */
  if (check_new && MailCheckRecent)
  {
    if (stat(path, &sb) == 0 && sb.st_mtime < mailbox->last_visited)
    {
      rc = 0;
      check_new = false;
    }
  }

  if (!(check_new || check_stats))
    return rc;

  dirp = opendir(path);
  if (!dirp)
  {
    mailbox->magic = MUTT_UNKNOWN;
    return 0;
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
      mailbox->msg_count++;
      if (p && strchr(p + 3, 'F'))
        mailbox->msg_flagged++;
    }
    if (!p || !strchr(p + 3, 'S'))
    {
      if (check_stats)
        mailbox->msg_unread++;
      if (check_new)
      {
        if (MailCheckRecent)
        {
          snprintf(msgpath, sizeof(msgpath), "%s/%s", path, de->d_name);
          /* ensure this message was received since leaving this mailbox */
          if (stat(msgpath, &sb) == 0 && (sb.st_ctime <= mailbox->last_visited))
            continue;
        }
        mailbox->new = true;
        rc = 1;
        check_new = false;
        if (!check_stats)
          break;
      }
    }
  }

  closedir(dirp);

  return rc;
}

/**
 * mailbox_maildir_check - Check for new mail in a maildir mailbox
 * @param mailbox     Mailbox to check
 * @param check_stats if true, also count total, new, and flagged messages
 * @retval 1 if the mailbox has new mail
 */
static int mailbox_maildir_check(struct Mailbox *mailbox, bool check_stats)
{
  int rc = 1;
  bool check_new = true;

  if (check_stats)
  {
    mailbox->msg_count = 0;
    mailbox->msg_unread = 0;
    mailbox->msg_flagged = 0;
  }

  rc = mailbox_maildir_check_dir(mailbox, "new", check_new, check_stats);

  check_new = !rc && MaildirCheckCur;
  if (check_new || check_stats)
    if (mailbox_maildir_check_dir(mailbox, "cur", check_new, check_stats))
      rc = 1;

  return rc;
}

/**
 * mailbox_mbox_check - Check for new mail for an mbox mailbox
 * @param mailbox     Mailbox to check
 * @param sb          stat(2) information about the mailbox
 * @param check_stats if true, also count total, new, and flagged messages
 * @retval 1 if the mailbox has new mail
 */
static int mailbox_mbox_check(struct Mailbox *mailbox, struct stat *sb, bool check_stats)
{
  int rc = 0;
  bool new_or_changed;

  if (CheckMboxSize)
    new_or_changed = (sb->st_size > mailbox->size);
  else
  {
    new_or_changed = (sb->st_mtime > sb->st_atime) ||
                     (mailbox->newly_created && (sb->st_ctime == sb->st_mtime) &&
                      (sb->st_ctime == sb->st_atime));
  }

  if (new_or_changed)
  {
    if (!MailCheckRecent || sb->st_mtime > mailbox->last_visited)
    {
      rc = 1;
      mailbox->new = true;
    }
  }
  else if (CheckMboxSize)
  {
    /* some other program has deleted mail from the folder */
    mailbox->size = (off_t) sb->st_size;
  }

  if (mailbox->newly_created && (sb->st_ctime != sb->st_mtime || sb->st_ctime != sb->st_atime))
    mailbox->newly_created = false;

  if (check_stats && (mailbox->stats_last_checked < sb->st_mtime))
  {
    struct Context *ctx =
        mx_mbox_open(mailbox->path, MUTT_READONLY | MUTT_QUIET | MUTT_NOSORT | MUTT_PEEK);
    if (ctx)
    {
      mailbox->msg_count = ctx->msgcount;
      mailbox->msg_unread = ctx->unread;
      mailbox->msg_flagged = ctx->flagged;
      mailbox->stats_last_checked = ctx->mtime;
      mx_mbox_close(&ctx, NULL);
    }
  }

  return rc;
}

/**
 * mailbox_check - Check a mailbox for new mail
 * @param tmp         Mailbox to check
 * @param contex_sb   stat() info for the current mailbox (Context)
 * @param check_stats If true, also count the total, new and flagged messages
 */
static void mailbox_check(struct Mailbox *tmp, struct stat *contex_sb, bool check_stats)
{
  struct stat sb;
#ifdef USE_SIDEBAR
  short orig_new;
  int orig_count, orig_unread, orig_flagged;
#endif

  memset(&sb, 0, sizeof(sb));

#ifdef USE_SIDEBAR
  orig_new = tmp->new;
  orig_count = tmp->msg_count;
  orig_unread = tmp->msg_unread;
  orig_flagged = tmp->msg_flagged;
#endif

  if (tmp->magic != MUTT_IMAP)
  {
    tmp->new = false;
#ifdef USE_POP
    if (pop_path_probe(tmp->path, NULL) == MUTT_POP)
      tmp->magic = MUTT_POP;
    else
#endif
#ifdef USE_NNTP
        if ((tmp->magic == MUTT_NNTP) || (nntp_path_probe(tmp->path, NULL) == MUTT_NNTP))
      tmp->magic = MUTT_NNTP;
#endif
#ifdef USE_NOTMUCH
    if (nm_path_probe(tmp->path, NULL) == MUTT_NOTMUCH)
      tmp->magic = MUTT_NOTMUCH;
    else
#endif
        if (stat(tmp->path, &sb) != 0 || (S_ISREG(sb.st_mode) && sb.st_size == 0) ||
            ((tmp->magic == MUTT_UNKNOWN) &&
             (tmp->magic = mx_path_probe(tmp->path, NULL)) <= 0))
    {
      /* if the mailbox still doesn't exist, set the newly created flag to be
       * ready for when it does. */
      tmp->newly_created = true;
      tmp->magic = MUTT_UNKNOWN;
      tmp->size = 0;
      return;
    }
  }

  /* check to see if the folder is the currently selected folder before polling */
  if (!Context || !Context->path ||
      ((tmp->magic == MUTT_IMAP ||
#ifdef USE_NNTP
        tmp->magic == MUTT_NNTP ||
#endif
#ifdef USE_NOTMUCH
        tmp->magic == MUTT_NOTMUCH ||
#endif
        tmp->magic == MUTT_POP) ?
           (mutt_str_strcmp(tmp->path, Context->path) != 0) :
           (sb.st_dev != contex_sb->st_dev || sb.st_ino != contex_sb->st_ino)))
  {
    switch (tmp->magic)
    {
      case MUTT_MBOX:
      case MUTT_MMDF:
        if (mailbox_mbox_check(tmp, &sb, check_stats) > 0)
          MailboxCount++;
        break;

      case MUTT_MAILDIR:
        if (mailbox_maildir_check(tmp, check_stats) > 0)
          MailboxCount++;
        break;

      case MUTT_MH:
        if (mh_mailbox(tmp, check_stats))
          MailboxCount++;
        break;
#ifdef USE_NOTMUCH
      case MUTT_NOTMUCH:
        tmp->msg_count = 0;
        tmp->msg_unread = 0;
        tmp->msg_flagged = 0;
        nm_nonctx_get_count(tmp->path, &tmp->msg_count, &tmp->msg_unread);
        if (tmp->msg_unread > 0)
        {
          MailboxCount++;
          tmp->new = true;
        }
        break;
#endif
      default:; /* do nothing */
    }
  }
  else if (CheckMboxSize && Context && Context->path)
    tmp->size = (off_t) sb.st_size; /* update the size of current folder */

#ifdef USE_SIDEBAR
  if ((orig_new != tmp->new) || (orig_count != tmp->msg_count) ||
      (orig_unread != tmp->msg_unread) || (orig_flagged != tmp->msg_flagged))
  {
    mutt_menu_set_current_redraw(REDRAW_SIDEBAR);
  }
#endif

  if (!tmp->new)
    tmp->notified = false;
  else if (!tmp->notified)
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
  mutt_expand_path(epath, mutt_str_strlen(epath));

  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &AllMailboxes, entries)
  {
    /* must be done late because e.g. IMAP delimiter may change */
    mutt_expand_path(np->b->path, sizeof(np->b->path));
    if (mutt_str_strcmp(np->b->path, path) == 0)
    {
      FREE(&epath);
      return np->b;
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
  struct utimbuf ut;

  if (CheckMboxSize)
  {
    struct Mailbox *b = mutt_find_mailbox(path);
    if (b && !b->new)
      mutt_update_mailbox(b);
  }
  else
  {
    /* fix up the times so mailbox won't get confused */
    if (st->st_mtime > st->st_atime)
    {
      ut.actime = st->st_atime;
      ut.modtime = time(NULL);
      utime(path, &ut);
    }
    else
      utime(path, NULL);
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
    if ((stat(np->b->path, &tmp_sb) == 0) && (sb.st_dev == tmp_sb.st_dev) &&
        (sb.st_ino == tmp_sb.st_ino))
    {
      return np->b;
    }
  }

  return NULL;
}

/**
 * mutt_update_mailbox - Get the mailbox's current size
 * @param b Mailbox to check
 */
void mutt_update_mailbox(struct Mailbox *b)
{
  struct stat sb;

  if (!b)
    return;

  if (stat(b->path, &sb) == 0)
    b->size = (off_t) sb.st_size;
  else
    b->size = 0;
}

/**
 * mutt_parse_mailboxes - Parse the 'mailboxes' command
 * @param path Temporary Buffer space
 * @param s    Buffer containing string to be parsed
 * @param data Flags associated with the command
 * @param err  Buffer for error messages
 * @retval  0 Success
 * @retval -1 Error
 *
 * This is also used by 'virtual-mailboxes'.
 */
int mutt_parse_mailboxes(struct Buffer *path, struct Buffer *s,
                         unsigned long data, struct Buffer *err)
{
  char buf[PATH_MAX];
  struct stat sb;
  char f1[PATH_MAX];
  char *p = NULL;

  while (MoreArgs(s))
  {
    char *desc = NULL;

    if (data & MUTT_NAMED)
    {
      mutt_extract_token(path, s, 0);
      if (path->data && *path->data)
        desc = mutt_str_strdup(path->data);
      else
        continue;
    }

    mutt_extract_token(path, s, 0);
#ifdef USE_NOTMUCH
    if (nm_path_probe(path->data, NULL) == MUTT_NOTMUCH)
      nm_normalize_uri(path->data, buf, sizeof(buf));
    else
#endif
      mutt_str_strfcpy(buf, path->data, sizeof(buf));

    mutt_expand_path(buf, sizeof(buf));

    /* Skip empty tokens. */
    if (!*buf)
    {
      FREE(&desc);
      continue;
    }

    /* avoid duplicates */
    p = realpath(buf, f1);
    struct MailboxNode *np = NULL;
    STAILQ_FOREACH(np, &AllMailboxes, entries)
    {
      if (mutt_str_strcmp(p ? p : buf, np->b->realpath) == 0)
      {
        mutt_debug(3, "mailbox '%s' already registered as '%s'\n", buf, np->b->path);
        break;
      }
    }

    if (np)
    {
      FREE(&desc);
      continue;
    }

    struct Mailbox *b = mailbox_new(buf);

    b->new = false;
    b->notified = true;
    b->newly_created = false;
    b->desc = desc;
#ifdef USE_NOTMUCH
    if (nm_path_probe(b->path, NULL) == MUTT_NOTMUCH)
    {
      b->magic = MUTT_NOTMUCH;
      b->size = 0;
    }
    else
#endif
    {
      /* for check_mbox_size, it is important that if the folder is new (tested by
       * reading it), the size is set to 0 so that later when we check we see
       * that it increased. without check_mbox_size we probably don't care.
       */
      if (CheckMboxSize && stat(b->path, &sb) == 0 && !test_new_folder(b->path))
      {
        /* some systems out there don't have an off_t type */
        b->size = (off_t) sb.st_size;
      }
      else
        b->size = 0;
    }

    struct MailboxNode *mn = mutt_mem_calloc(1, sizeof(*mn));
    mn->b = b;
    STAILQ_INSERT_TAIL(&AllMailboxes, mn, entries);

#ifdef USE_SIDEBAR
    mutt_sb_notify_mailbox(b, true);
#endif
#ifdef USE_INOTIFY
    b->magic = mx_path_probe(b->path, NULL);
    mutt_monitor_add(b);
#endif
  }
  return 0;
}

/**
 * mutt_parse_unmailboxes - Parse the 'unmailboxes' command
 * @param path Temporary Buffer space
 * @param s    Buffer containing string to be parsed
 * @param data Flags associated with the command
 * @param err  Buffer for error messages
 * @retval  0 Success
 * @retval -1 Error
 *
 * This is also used by 'unvirtual-mailboxes'
 */
int mutt_parse_unmailboxes(struct Buffer *path, struct Buffer *s,
                           unsigned long data, struct Buffer *err)
{
  char buf[PATH_MAX];
  bool clear_all = false;

  while (!clear_all && MoreArgs(s))
  {
    mutt_extract_token(path, s, 0);

    if (mutt_str_strcmp(path->data, "*") == 0)
    {
      clear_all = true;
    }
    else
    {
#ifdef USE_NOTMUCH
      if (nm_path_probe(path->data, NULL) == MUTT_NOTMUCH)
      {
        nm_normalize_uri(path->data, buf, sizeof(buf));
      }
      else
#endif
      {
        mutt_str_strfcpy(buf, path->data, sizeof(buf));
        mutt_expand_path(buf, sizeof(buf));
      }
    }

    struct MailboxNode *np = NULL;
    struct MailboxNode *tmp = NULL;
    STAILQ_FOREACH_SAFE(np, &AllMailboxes, entries, tmp)
    {
      /* Decide whether to delete all normal mailboxes or all virtual */
      bool virt = ((np->b->magic == MUTT_NOTMUCH) && (data & MUTT_VIRTUAL));
      bool norm = ((np->b->magic != MUTT_NOTMUCH) && !(data & MUTT_VIRTUAL));
      bool clear_this = clear_all && (virt || norm);

      if (clear_this || (mutt_str_strcasecmp(buf, np->b->path) == 0) ||
          (mutt_str_strcasecmp(buf, np->b->desc) == 0))
      {
#ifdef USE_SIDEBAR
        mutt_sb_notify_mailbox(np->b, false);
#endif
#ifdef USE_INOTIFY
        mutt_monitor_remove(np->b);
#endif
        STAILQ_REMOVE(&AllMailboxes, np, MailboxNode, entries);
        mailbox_free(&np->b);
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
  if (!Context || Context->magic == MUTT_IMAP || Context->magic == MUTT_POP
#ifdef USE_NNTP
      || Context->magic == MUTT_NNTP
#endif
      || stat(Context->path, &contex_sb) != 0)
  {
    contex_sb.st_dev = 0;
    contex_sb.st_ino = 0;
  }

  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &AllMailboxes, entries)
  {
    mailbox_check(np->b, &contex_sb, check_stats);
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
    if (!np->b->new || (have_unnotified && np->b->notified))
      continue;

    mutt_str_strfcpy(path, np->b->path, sizeof(path));
    mutt_pretty_mailbox(path, sizeof(path));

    if (!first && (MuttMessageWindow->cols >= 7) &&
        (pos + strlen(path) >= (size_t) MuttMessageWindow->cols - 7))
    {
      break;
    }

    if (!first)
      pos += strlen(strncat(mailboxlist + pos, ", ", sizeof(mailboxlist) - 1 - pos));

    /* Prepend an asterisk to mailboxes not already notified */
    if (!np->b->notified)
    {
      /* pos += strlen (strncat(mailboxlist + pos, "*", sizeof(mailboxlist)-1-pos)); */
      np->b->notified = true;
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
  struct Mailbox *mailbox = mailbox_get(path);
  if (!mailbox)
    return;

  mailbox->notified = true;
  time(&mailbox->last_visited);
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
        if (np->b->magic == MUTT_NOTMUCH) /* only match real mailboxes */
          continue;
        mutt_expand_path(np->b->path, sizeof(np->b->path));
        if ((found || pass) && np->b->new)
        {
          mutt_str_strfcpy(s, np->b->path, slen);
          mutt_pretty_mailbox(s, slen);
          return;
        }
        if (mutt_str_strcmp(s, np->b->path) == 0)
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
        if (np->b->magic != MUTT_NOTMUCH)
          continue;
        if ((found || pass) && np->b->new)
        {
          mutt_str_strfcpy(buf, np->b->desc, buflen);
          return;
        }
        if (mutt_str_strcmp(buf, np->b->path) == 0)
          found = true;
      }
    }

    mutt_mailbox_check(MUTT_MAILBOX_CHECK_FORCE); /* Mailbox was wrong - resync things */
  }

  /* no folders with new mail */
  *buf = '\0';
}
#endif
