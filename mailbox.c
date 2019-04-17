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

/**
 * @page mailbox Representation of a mailbox
 *
 * Representation of a mailbox
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
#include "maildir/lib.h"
#include "mbox/mbox.h"
#include "mutt_commands.h"
#include "mutt_menu.h"
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
short C_MailCheck; ///< Config: Number of seconds before NeoMutt checks for new mail
bool C_MailCheckStats;          ///< Config: Periodically check for new mail
short C_MailCheckStatsInterval; ///< Config: How often to check for new mail
bool C_MaildirCheckCur; ///< Config: Check both 'new' and 'cur' directories for new mail

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
  struct Mailbox *m = mutt_mem_calloc(1, sizeof(struct Mailbox));

  m->pathbuf = mutt_buffer_new();

  return m;
}

/**
 * mailbox_free - Free a Mailbox
 * @param[out] ptr Mailbox to free
 */
void mailbox_free(struct Mailbox **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Mailbox *m = *ptr;
  mutt_mailbox_changed(m, MBN_CLOSED);

  mutt_buffer_free(&m->pathbuf);
  FREE(&m->desc);
  if (m->mdata && m->free_mdata)
    m->free_mdata(&m->mdata);
  FREE(&m->realpath);
  FREE(ptr);
}

/**
 * mailbox_check - Check a mailbox for new mail
 * @param m_cur       Current Mailbox
 * @param m_check     Mailbox to check
 * @param ctx_sb      stat() info for the current Mailbox
 * @param check_stats If true, also count the total, new and flagged messages
 */
static void mailbox_check(struct Mailbox *m_cur, struct Mailbox *m_check,
                          struct stat *ctx_sb, bool check_stats)
{
  struct stat sb = { 0 };

#ifdef USE_SIDEBAR
  short orig_new = m_check->has_new;
  int orig_count = m_check->msg_count;
  int orig_unread = m_check->msg_unread;
  int orig_flagged = m_check->msg_flagged;
#endif

  enum MailboxType mb_magic = mx_path_probe(mutt_b2s(m_check->pathbuf), NULL);

  switch (mb_magic)
  {
    case MUTT_POP:
    case MUTT_NNTP:
    case MUTT_NOTMUCH:
    case MUTT_IMAP:
      if (mb_magic != MUTT_IMAP)
        m_check->has_new = false;
      m_check->magic = mb_magic;
      break;
    default:
      m_check->has_new = false;

      if ((stat(mutt_b2s(m_check->pathbuf), &sb) != 0) ||
          (S_ISREG(sb.st_mode) && (sb.st_size == 0)) ||
          ((m_check->magic == MUTT_UNKNOWN) &&
           ((m_check->magic = mx_path_probe(mutt_b2s(m_check->pathbuf), NULL)) <= 0)))
      {
        /* if the mailbox still doesn't exist, set the newly created flag to be
         * ready for when it does. */
        m_check->newly_created = true;
        m_check->magic = MUTT_UNKNOWN;
        m_check->size = 0;
        return;
      }
      break; // kept for consistency.
  }

  /* check to see if the folder is the currently selected folder before polling */
  if (!m_cur || mutt_buffer_is_empty(m_cur->pathbuf) ||
      (((m_check->magic == MUTT_IMAP) || (m_check->magic == MUTT_NNTP) ||
        (m_check->magic == MUTT_NOTMUCH) || (m_check->magic == MUTT_POP)) ?
           (mutt_str_strcmp(mutt_b2s(m_check->pathbuf), mutt_b2s(m_cur->pathbuf)) != 0) :
           ((sb.st_dev != ctx_sb->st_dev) || (sb.st_ino != ctx_sb->st_ino))))
  {
    switch (m_check->magic)
    {
      case MUTT_IMAP:
      case MUTT_MBOX:
      case MUTT_MMDF:
      case MUTT_MAILDIR:
      case MUTT_MH:
      case MUTT_NOTMUCH:
        if (mx_mbox_check_stats(m_check, 0) == 0)
          MailboxCount++;
        break;
      default:; /* do nothing */
    }
  }
  else if (C_CheckMboxSize && m_cur && mutt_buffer_is_empty(m_cur->pathbuf))
    m_check->size = (off_t) sb.st_size; /* update the size of current folder */

#ifdef USE_SIDEBAR
  if ((orig_new != m_check->has_new) || (orig_count != m_check->msg_count) ||
      (orig_unread != m_check->msg_unread) || (orig_flagged != m_check->msg_flagged))
  {
    mutt_menu_set_current_redraw(REDRAW_SIDEBAR);
  }
#endif

  if (!m_check->has_new)
    m_check->notified = false;
  else if (!m_check->notified)
    MailboxNotify++;
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

  if (C_CheckMboxSize)
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
    if ((stat(mutt_b2s(np->mailbox->pathbuf), &tmp_sb) == 0) &&
        (sb.st_dev == tmp_sb.st_dev) && (sb.st_ino == tmp_sb.st_ino))
    {
      return np->mailbox;
    }
  }

  return NULL;
}

/**
 * mutt_find_mailbox_desc - Find the mailbox with a given description
 * @param desc Description to match
 * @retval ptr Matching Mailbox
 * @retval NULL No matching mailbox found
 */
struct Mailbox *mutt_find_mailbox_desc(const char *desc)
{
  if (!desc)
    return NULL;

  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &AllMailboxes, entries)
  {
    if (np->mailbox->desc && (mutt_str_strcmp(np->mailbox->desc, desc) == 0))
      return np->mailbox;
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

  if (stat(mutt_b2s(m->pathbuf), &sb) == 0)
    m->size = (off_t) sb.st_size;
  else
    m->size = 0;
}

/**
 * mutt_mailbox_check - Check all AllMailboxes for new mail
 * @param m_cur Current Mailbox
 * @param force Force flags, see below
 * @retval num Number of mailboxes with new mail
 *
 * The force argument may be any combination of the following values:
 * - MUTT_MAILBOX_CHECK_FORCE        ignore MailboxTime and check for new mail
 * - MUTT_MAILBOX_CHECK_FORCE_STATS  ignore MailboxTime and calculate statistics
 *
 * Check all AllMailboxes for new mail and total/new/flagged messages
 */
int mutt_mailbox_check(struct Mailbox *m_cur, int force)
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
  if (!force && (t - MailboxTime < C_MailCheck))
    return MailboxCount;

  if ((force & MUTT_MAILBOX_CHECK_FORCE_STATS) ||
      (C_MailCheckStats && ((t - MailboxStatsTime) >= C_MailCheckStatsInterval)))
  {
    check_stats = true;
    MailboxStatsTime = t;
  }

  MailboxTime = t;
  MailboxCount = 0;
  MailboxNotify = 0;

  /* check device ID and serial number instead of comparing paths */
  if (!m_cur || (m_cur->magic == MUTT_IMAP) || (m_cur->magic == MUTT_POP)
#ifdef USE_NNTP
      || (m_cur->magic == MUTT_NNTP)
#endif
      || stat(mutt_b2s(m_cur->pathbuf), &contex_sb) != 0)
  {
    contex_sb.st_dev = 0;
    contex_sb.st_ino = 0;
  }

  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &AllMailboxes, entries)
  {
    mailbox_check(m_cur, np->mailbox, &contex_sb,
                  check_stats || (!np->mailbox->first_check_stats_done && C_MailCheckStats));
    np->mailbox->first_check_stats_done = true;
  }

  return MailboxCount;
}

/**
 * mutt_mailbox_list - List the mailboxes with new mail
 * @retval true If there is new mail
 */
bool mutt_mailbox_list(void)
{
  char mailboxlist[512];
  size_t pos = 0;
  int first = 1;

  int have_unnotified = MailboxNotify;

  struct Buffer *path = mutt_buffer_pool_get();

  mailboxlist[0] = '\0';
  pos += strlen(strncat(mailboxlist, _("New mail in "), sizeof(mailboxlist) - 1 - pos));
  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &AllMailboxes, entries)
  {
    /* Is there new mail in this mailbox? */
    if (!np->mailbox->has_new || (have_unnotified && np->mailbox->notified))
      continue;

    mutt_buffer_strcpy(path, mutt_b2s(np->mailbox->pathbuf));
    mutt_buffer_pretty_mailbox(path);

    if (!first && (MuttMessageWindow->cols >= 7) &&
        ((pos + mutt_buffer_len(path)) >= ((size_t) MuttMessageWindow->cols - 7)))
    {
      break;
    }

    if (!first)
      pos += strlen(strncat(mailboxlist + pos, ", ", sizeof(mailboxlist) - 1 - pos));

    /* Prepend an asterisk to mailboxes not already notified */
    if (!np->mailbox->notified)
    {
      /* pos += strlen (strncat(mailboxlist + pos, "*", sizeof(mailboxlist)-1-pos)); */
      np->mailbox->notified = true;
      MailboxNotify--;
    }
    pos += strlen(strncat(mailboxlist + pos, mutt_b2s(path), sizeof(mailboxlist) - 1 - pos));
    first = 0;
  }

  if (!first && np)
  {
    strncat(mailboxlist + pos, ", ...", sizeof(mailboxlist) - 1 - pos);
  }

  mutt_buffer_pool_release(&path);

  if (!first)
  {
    mutt_message("%s", mailboxlist);
    return true;
  }
  else
  {
    /* there were no mailboxes needing to be notified, so clean up since
     * MailboxNotify has somehow gotten out of sync */
    MailboxNotify = 0;
    return false;
  }
}

/**
 * mutt_mailbox_setnotified - Note when the user was last notified of new mail
 * @param m Mailbox
 */
void mutt_mailbox_setnotified(struct Mailbox *m)
{
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
 * @param m_cur Current Mailbox
 * @retval true If there is new mail
 */
bool mutt_mailbox_notify(struct Mailbox *m_cur)
{
  if (mutt_mailbox_check(m_cur, 0) && MailboxNotify)
  {
    return mutt_mailbox_list();
  }
  return false;
}

/**
 * mutt_buffer_mailbox - incoming folders completion routine
 * @param m_cur Current Mailbox
 * @param s     Buffer containing name of current mailbox
 *
 * Given a folder name, find the next incoming folder with new mail.
 */
void mutt_buffer_mailbox(struct Mailbox *m_cur, struct Buffer *s)
{
  mutt_buffer_expand_path(s);

  if (mutt_mailbox_check(m_cur, 0))
  {
    int found = 0;
    for (int pass = 0; pass < 2; pass++)
    {
      struct MailboxNode *np = NULL;
      STAILQ_FOREACH(np, &AllMailboxes, entries)
      {
        if (np->mailbox->magic == MUTT_NOTMUCH) /* only match real mailboxes */
          continue;
        mutt_buffer_expand_path(np->mailbox->pathbuf);
        if ((found || pass) && np->mailbox->has_new)
        {
          mutt_buffer_strcpy(s, mutt_b2s(np->mailbox->pathbuf));
          mutt_buffer_pretty_mailbox(s);
          return;
        }
        if (mutt_str_strcmp(mutt_b2s(s), mutt_b2s(np->mailbox->pathbuf)) == 0)
          found = 1;
      }
    }

    mutt_mailbox_check(m_cur, MUTT_MAILBOX_CHECK_FORCE); /* mailbox was wrong - resync things */
  }

  /* no folders with new mail */
  mutt_buffer_reset(s);
}

/**
 * mutt_mailbox - incoming folders completion routine
 * @param m_cur Current Mailbox
 * @param s     Buffer containing name of current mailbox
 * @param slen  Buffer length
 *
 * Given a folder name, find the next incoming folder with new mail.
 */
void mutt_mailbox(struct Mailbox *m_cur, char *s, size_t slen)
{
  struct Buffer *s_buf = mutt_buffer_pool_get();

  mutt_buffer_addstr(s_buf, NONULL(s));
  mutt_buffer_mailbox(m_cur, s_buf);
  mutt_str_strfcpy(s, mutt_b2s(s_buf), slen);

  mutt_buffer_pool_release(&s_buf);
}

/**
 * mutt_mailbox_changed - Notify listeners of a change to a Mailbox
 * @param m      Mailbox
 * @param action Change to Mailbox
 */
void mutt_mailbox_changed(struct Mailbox *m, enum MailboxNotification action)
{
  if (!m || !m->notify)
    return;

  m->notify(m, action);
}

/**
 * mutt_mailbox_size_add - Add an email's size to the total size of a Mailbox
 * @param m Mailbox
 * @param e Email
 */
void mutt_mailbox_size_add(struct Mailbox *m, const struct Email *e)
{
  m->size += mutt_email_size(e);
}

/**
 * mutt_mailbox_size_sub - Subtract an email's size from the total size of a Mailbox
 * @param m Mailbox
 * @param e Email
 */
void mutt_mailbox_size_sub(struct Mailbox *m, const struct Email *e)
{
  m->size -= mutt_email_size(e);
}
