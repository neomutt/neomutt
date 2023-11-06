/**
 * @file
 * Mailbox helper functions
 *
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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
 * @page neo_mutt_mailbox Mailbox helper functions
 *
 * Mailbox helper functions
 */

#include "config.h"
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "mutt_mailbox.h"
#include "postpone/lib.h"
#include "muttlib.h"
#include "mx.h"

static time_t MailboxTime = 0; ///< last time we started checking for mail
static time_t MailboxStatsTime = 0; ///< last time we check performed mail_check_stats
static short MailboxCount = 0;  ///< how many boxes with new mail
static short MailboxNotify = 0; ///< # of unnotified new boxes

/**
 * is_same_mailbox - Compare two Mailboxes to see if they're equal
 * @param m1  First mailbox
 * @param m2  Second mailbox
 * @param st1 stat() info for first mailbox
 * @param st2 stat() info for second mailbox
 * @retval true  Mailboxes are the same
 * @retval false Mailboxes are different
 */
static bool is_same_mailbox(struct Mailbox *m1, struct Mailbox *m2,
                            struct stat *st1, struct stat *st2)
{
  if (!m1 || buf_is_empty(&m1->pathbuf) || !m2 || buf_is_empty(&m2->pathbuf) ||
      (m1->type != m2->type))
  {
    return false;
  }

  const bool uses_protocol = (m2->type == MUTT_IMAP) || (m2->type == MUTT_NNTP) ||
                             (m2->type == MUTT_NOTMUCH) || (m2->type == MUTT_POP);

  if (uses_protocol)
    return mutt_str_equal(mailbox_path(m1), mailbox_path(m2));
  else
    return ((st1->st_dev == st2->st_dev) && (st1->st_ino == st2->st_ino));
}

/**
 * mailbox_check - Check a mailbox for new mail
 * @param m_cur   Current Mailbox
 * @param m_check Mailbox to check
 * @param st_cur  stat() info for the current Mailbox
 * @param flags   Flags, e.g. #MUTT_MAILBOX_CHECK_FORCE
 */
static void mailbox_check(struct Mailbox *m_cur, struct Mailbox *m_check,
                          struct stat *st_cur, CheckStatsFlags flags)
{
  struct stat st = { 0 };

  enum MailboxType mb_type = mx_path_probe(mailbox_path(m_check));

  const bool c_mail_check_recent = cs_subset_bool(NeoMutt->sub, "mail_check_recent");
  if ((m_cur == m_check) && c_mail_check_recent)
    m_check->has_new = false;

  switch (mb_type)
  {
    case MUTT_POP:
    case MUTT_NNTP:
    case MUTT_NOTMUCH:
    case MUTT_IMAP:
      m_check->type = mb_type;
      break;
    default:
      if ((stat(mailbox_path(m_check), &st) != 0) ||
          ((m_check->type == MUTT_UNKNOWN) && S_ISREG(st.st_mode) && (st.st_size == 0)) ||
          ((m_check->type == MUTT_UNKNOWN) &&
           ((m_check->type = mx_path_probe(mailbox_path(m_check))) <= 0)))
      {
        /* if the mailbox still doesn't exist, set the newly created flag to be
         * ready for when it does. */
        m_check->newly_created = true;
        m_check->type = MUTT_UNKNOWN;
        m_check->size = 0;
        return;
      }
      break; // kept for consistency.
  }

  const bool c_check_mbox_size = cs_subset_bool(NeoMutt->sub, "check_mbox_size");

  /* check to see if the folder is the currently selected folder before polling */
  if (!is_same_mailbox(m_cur, m_check, st_cur, &st))
  {
    switch (m_check->type)
    {
      case MUTT_NOTMUCH:
        // Remove this when non-notmuch backends only check unread, flagged,
        // and total counts per 'mbox_check_stats' docs.
        if ((flags & MUTT_MAILBOX_CHECK_FORCE_STATS) == 0)
          break;
        FALLTHROUGH;

      case MUTT_IMAP:
      case MUTT_MBOX:
      case MUTT_MMDF:
      case MUTT_MAILDIR:
      case MUTT_MH:
        mx_mbox_check_stats(m_check, flags);
        break;
      default:; /* do nothing */
    }
  }
  else if (c_check_mbox_size && m_cur && buf_is_empty(&m_cur->pathbuf))
  {
    m_check->size = (off_t) st.st_size; /* update the size of current folder */
  }

  if (!m_check->has_new)
  {
    m_check->notified = false;
  }
  else
  {
    // pretend that we've already notified for the mailbox
    if (!m_check->notify_user)
      m_check->notified = true;
    else if (!m_check->notified)
      MailboxNotify++;
  }
}

/**
 * mutt_mailbox_check - Check all all Mailboxes for new mail
 * @param m_cur Current Mailbox
 * @param flags Flags, e.g. #MUTT_MAILBOX_CHECK_FORCE
 * @retval num Number of mailboxes with new mail
 *
 * Check all all Mailboxes for new mail and total/new/flagged messages
 */
int mutt_mailbox_check(struct Mailbox *m_cur, CheckStatsFlags flags)
{
  if (TAILQ_EMPTY(&NeoMutt->accounts)) // fast return if there are no mailboxes
    return 0;

#ifdef USE_IMAP
  if (flags & MUTT_MAILBOX_CHECK_FORCE)
    mutt_update_num_postponed();
#endif

  const short c_mail_check = cs_subset_number(NeoMutt->sub, "mail_check");
  const bool c_mail_check_stats = cs_subset_bool(NeoMutt->sub, "mail_check_stats");
  const short c_mail_check_stats_interval = cs_subset_number(NeoMutt->sub, "mail_check_stats_interval");

  time_t t = mutt_date_now();
  if ((flags == MUTT_MAILBOX_CHECK_NO_FLAGS) && ((t - MailboxTime) < c_mail_check))
    return MailboxCount;

  if ((flags & MUTT_MAILBOX_CHECK_FORCE_STATS) ||
      (c_mail_check_stats && ((t - MailboxStatsTime) >= c_mail_check_stats_interval)))
  {
    flags |= MUTT_MAILBOX_CHECK_FORCE_STATS;
    MailboxStatsTime = t;
  }

  MailboxTime = t;
  MailboxCount = 0;
  MailboxNotify = 0;

  /* check device ID and serial number instead of comparing paths */
  struct stat st_cur = { 0 };
  if (!m_cur || (m_cur->type == MUTT_IMAP) || (m_cur->type == MUTT_POP)
#ifdef USE_NNTP
      || (m_cur->type == MUTT_NNTP)
#endif
      || stat(mailbox_path(m_cur), &st_cur) != 0)
  {
    st_cur.st_dev = 0;
    st_cur.st_ino = 0;
  }

  struct MailboxList ml = STAILQ_HEAD_INITIALIZER(ml);
  neomutt_mailboxlist_get_all(&ml, NeoMutt, MUTT_MAILBOX_ANY);
  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &ml, entries)
  {
    struct Mailbox *m = np->mailbox;

    if (!m->visible || !m->poll_new_mail)
      continue;

    CheckStatsFlags m_flags = flags;
    if (!m->first_check_stats_done && c_mail_check_stats)
    {
      m_flags |= MUTT_MAILBOX_CHECK_FORCE_STATS;
    }
    mailbox_check(m_cur, m, &st_cur, m_flags);
    if (m->has_new)
      MailboxCount++;
    m->first_check_stats_done = true;
  }
  neomutt_mailboxlist_clear(&ml);

  return MailboxCount;
}

/**
 * mutt_mailbox_notify - Notify the user if there's new mail
 * @param m_cur Current Mailbox
 * @retval true There is new mail
 */
bool mutt_mailbox_notify(struct Mailbox *m_cur)
{
  if ((mutt_mailbox_check(m_cur, MUTT_MAILBOX_CHECK_NO_FLAGS) > 0) && (MailboxNotify != 0))
  {
    return mutt_mailbox_list();
  }
  return false;
}

/**
 * mutt_mailbox_list - Show a message with the list of mailboxes with new mail
 * @retval true There is new mail
 */
bool mutt_mailbox_list(void)
{
  char mailboxlist[512] = { 0 };
  size_t pos = 0;
  int first = 1;

  int have_unnotified = MailboxNotify;

  struct Buffer *path = buf_pool_get();

  mailboxlist[0] = '\0';
  pos += strlen(strncat(mailboxlist, _("New mail in "), sizeof(mailboxlist) - 1 - pos));
  struct MailboxList ml = STAILQ_HEAD_INITIALIZER(ml);
  neomutt_mailboxlist_get_all(&ml, NeoMutt, MUTT_MAILBOX_ANY);
  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &ml, entries)
  {
    /* Is there new mail in this mailbox? */
    if (!np->mailbox->has_new || (have_unnotified && np->mailbox->notified))
      continue;

    buf_strcpy(path, mailbox_path(np->mailbox));
    buf_pretty_mailbox(path);

    if (!first)
      pos += strlen(strncat(mailboxlist + pos, ", ", sizeof(mailboxlist) - 1 - pos));

    if (!np->mailbox->notified)
    {
      np->mailbox->notified = true;
      MailboxNotify--;
    }
    pos += strlen(strncat(mailboxlist + pos, buf_string(path), sizeof(mailboxlist) - 1 - pos));
    first = 0;
  }
  neomutt_mailboxlist_clear(&ml);

  buf_pool_release(&path);

  if (!first)
  {
    mutt_message("%s", mailboxlist);
    return true;
  }

  /* there were no mailboxes needing to be notified, so clean up since
    * MailboxNotify has somehow gotten out of sync */
  MailboxNotify = 0;
  return false;
}

/**
 * mutt_mailbox_set_notified - Note when the user was last notified of new mail
 * @param m Mailbox
 */
void mutt_mailbox_set_notified(struct Mailbox *m)
{
  if (!m)
    return;

  m->notified = true;
#ifdef HAVE_CLOCK_GETTIME
  clock_gettime(CLOCK_REALTIME, &m->last_visited);
#else
  m->last_visited.tv_sec = mutt_date_now();
  m->last_visited.tv_nsec = 0;
#endif
}

/**
 * find_next_mailbox - Find the next mailbox with new or unread mail.
 * @param s         Buffer containing name of current mailbox
 * @param find_new  Boolean controlling new or unread check
 * @retval ptr Mailbox
 *
 * Given a folder name, find the next incoming folder with new or unread mail.
 * The Mailbox will be returned and a pretty version of the path put into s.
 */
static struct Mailbox *find_next_mailbox(struct Buffer *s, bool find_new)
{
  bool found = false;
  for (int pass = 0; pass < 2; pass++)
  {
    struct MailboxList ml = STAILQ_HEAD_INITIALIZER(ml);
    neomutt_mailboxlist_get_all(&ml, NeoMutt, MUTT_MAILBOX_ANY);
    struct MailboxNode *np = NULL;
    STAILQ_FOREACH(np, &ml, entries)
    {
      // Match only real mailboxes if looking for new mail.
      if (find_new && np->mailbox->type == MUTT_NOTMUCH)
        continue;

      buf_expand_path(&np->mailbox->pathbuf);
      struct Mailbox *m_cur = np->mailbox;

      if ((found || (pass > 0)) && (find_new ? m_cur->has_new : m_cur->msg_unread > 0))
      {
        buf_strcpy(s, mailbox_path(np->mailbox));
        buf_pretty_mailbox(s);
        struct Mailbox *m_result = np->mailbox;
        neomutt_mailboxlist_clear(&ml);
        return m_result;
      }
      if (mutt_str_equal(buf_string(s), mailbox_path(np->mailbox)))
        found = true;
    }
    neomutt_mailboxlist_clear(&ml);
  }

  return NULL;
}

/**
 * mutt_mailbox_next - Incoming folders completion routine
 * @param m_cur Current Mailbox
 * @param s     Buffer containing name of current mailbox
 * @retval ptr Mailbox
 *
 * Given a folder name, find the next incoming folder with new mail.
 * The Mailbox will be returned and a pretty version of the path put into s.
 */
struct Mailbox *mutt_mailbox_next(struct Mailbox *m_cur, struct Buffer *s)
{
  buf_expand_path(s);

  if (mutt_mailbox_check(m_cur, MUTT_MAILBOX_CHECK_NO_FLAGS) > 0)
  {
    struct Mailbox *m_res = find_next_mailbox(s, true);
    if (m_res)
      return m_res;

    mutt_mailbox_check(m_cur, MUTT_MAILBOX_CHECK_FORCE); /* mailbox was wrong - resync things */
  }

  buf_reset(s); // no folders with new mail
  return NULL;
}

/**
 * mutt_mailbox_next_unread - Find next mailbox with unread mail
 * @param m_cur Current Mailbox
 * @param s     Buffer containing name of current mailbox
 * @retval ptr Mailbox
 *
 * Given a folder name, find the next mailbox with unread mail.
 * The Mailbox will be returned and a pretty version of the path put into s.
 */
struct Mailbox *mutt_mailbox_next_unread(struct Mailbox *m_cur, struct Buffer *s)
{
  buf_expand_path(s);

  struct Mailbox *m_res = find_next_mailbox(s, false);
  if (m_res)
    return m_res;

  buf_reset(s); // no folders with new mail
  return NULL;
}

/**
 * mailbox_restore_timestamp - Restore the timestamp of a mailbox
 * @param path Path to the mailbox
 * @param st   Timestamp info from stat()
 *
 * Fix up the atime and mtime after mbox/mmdf mailbox was modified according to
 * stat() info taken before a modification.
 */
void mailbox_restore_timestamp(const char *path, struct stat *st)
{
#ifdef HAVE_UTIMENSAT
  struct timespec ts[2] = { { 0 }, { 0 } };
#else
  struct utimbuf ut = { 0 };
#endif

  const bool c_check_mbox_size = cs_subset_bool(NeoMutt->sub, "check_mbox_size");
  if (c_check_mbox_size)
  {
    struct Mailbox *m = mailbox_find(path);
    if (m && !m->has_new)
      mailbox_update(m);
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
      utimensat(AT_FDCWD, buf, ts, 0);
#else
      ut.actime = st->st_atime;
      ut.modtime = mutt_date_now();
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
      utimensat(AT_FDCWD, buf, ts, 0);
#else
      utime(path, NULL);
#endif
    }
  }
}
