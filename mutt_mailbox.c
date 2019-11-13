#include "config.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <utime.h>
#include "mutt/mutt.h"
#include "core/lib.h"
#include "mutt_mailbox.h"
#include "globals.h"
#include "mutt_menu.h"
#include "mutt_window.h"
#include "muttlib.h"
#include "mx.h"
#include "protos.h"

static time_t MailboxTime = 0; /**< last time we started checking for mail */
static time_t MailboxStatsTime = 0; /**< last time we check performed mail_check_stats */
static short MailboxCount = 0;  /**< how many boxes with new mail */
static short MailboxNotify = 0; /**< # of unnotified new boxes */

/* These Config Variables are only used in mutt_mailbox.c */
short C_MailCheck; ///< Config: Number of seconds before NeoMutt checks for new mail
bool C_MailCheckStats;          ///< Config: Periodically check for new mail
short C_MailCheckStatsInterval; ///< Config: How often to check for new mail

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

  enum MailboxType mb_magic = mx_path_probe(mailbox_path(m_check), NULL);

  if ((m_cur == m_check) && C_MailCheckRecent)
    m_check->has_new = false;

  switch (mb_magic)
  {
    case MUTT_POP:
    case MUTT_NNTP:
    case MUTT_NOTMUCH:
    case MUTT_IMAP:
      m_check->magic = mb_magic;
      break;
    default:
      if ((stat(mailbox_path(m_check), &sb) != 0) ||
          (S_ISREG(sb.st_mode) && (sb.st_size == 0)) ||
          ((m_check->magic == MUTT_UNKNOWN) &&
           ((m_check->magic = mx_path_probe(mailbox_path(m_check), NULL)) <= 0)))
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
  if (!m_cur || mutt_buffer_is_empty(&m_cur->pathbuf) ||
      (((m_check->magic == MUTT_IMAP) || (m_check->magic == MUTT_NNTP) ||
        (m_check->magic == MUTT_NOTMUCH) || (m_check->magic == MUTT_POP)) ?
           (mutt_str_strcmp(mailbox_path(m_check), mailbox_path(m_cur)) != 0) :
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
        if ((mx_mbox_check_stats(m_check, check_stats) > 0) && m_check->has_new)
          MailboxCount++;
        break;
      default:; /* do nothing */
    }
  }
  else if (C_CheckMboxSize && m_cur && mutt_buffer_is_empty(&m_cur->pathbuf))
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
 * mutt_mailbox_check - Check all all Mailboxes for new mail
 * @param m_cur Current Mailbox
 * @param force Force flags, see below
 * @retval num Number of mailboxes with new mail
 *
 * The force argument may be any combination of the following values:
 * - MUTT_MAILBOX_CHECK_FORCE        ignore MailboxTime and check for new mail
 * - MUTT_MAILBOX_CHECK_FORCE_STATS  ignore MailboxTime and calculate statistics
 *
 * Check all all Mailboxes for new mail and total/new/flagged messages
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
  if (TAILQ_EMPTY(&NeoMutt->accounts))
    return 0;

  t = mutt_date_epoch();
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
      || stat(mailbox_path(m_cur), &contex_sb) != 0)
  {
    contex_sb.st_dev = 0;
    contex_sb.st_ino = 0;
  }

  struct MailboxList ml = neomutt_mailboxlist_get_all(NeoMutt, MUTT_MAILBOX_ANY);
  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &ml, entries)
  {
    mailbox_check(m_cur, np->mailbox, &contex_sb,
                  check_stats || (!np->mailbox->first_check_stats_done && C_MailCheckStats));
    np->mailbox->first_check_stats_done = true;
  }
  neomutt_mailboxlist_clear(&ml);

  return MailboxCount;
}

/**
 * mutt_mailbox_notify - Notify the user if there's new mail
 * @param m_cur Current Mailbox
 * @retval true If there is new mail
 */
bool mutt_mailbox_notify(struct Mailbox *m_cur)
{
  if ((mutt_mailbox_check(m_cur, 0) > 0) && MailboxNotify)
  {
    return mutt_mailbox_list();
  }
  return false;
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
  struct MailboxList ml = neomutt_mailboxlist_get_all(NeoMutt, MUTT_MAILBOX_ANY);
  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &ml, entries)
  {
    /* Is there new mail in this mailbox? */
    if (!np->mailbox->has_new || (have_unnotified && np->mailbox->notified))
      continue;

    mutt_buffer_strcpy(path, mailbox_path(np->mailbox));
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
  neomutt_mailboxlist_clear(&ml);

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
  m->last_visited.tv_sec = mutt_date_epoch();
  m->last_visited.tv_nsec = 0;
#endif
}

/**
 * mutt_mailbox_next_buffer - incoming folders completion routine
 * @param m_cur Current Mailbox
 * @param s     Buffer containing name of current mailbox
 *
 * Given a folder name, find the next incoming folder with new mail.
 */
void mutt_mailbox_next_buffer(struct Mailbox *m_cur, struct Buffer *s)
{
  mutt_buffer_expand_path(s);

  if (mutt_mailbox_check(m_cur, 0) > 0)
  {
    bool found = false;
    for (int pass = 0; pass < 2; pass++)
    {
      struct MailboxList ml = neomutt_mailboxlist_get_all(NeoMutt, MUTT_MAILBOX_ANY);
      struct MailboxNode *np = NULL;
      STAILQ_FOREACH(np, &ml, entries)
      {
        if (np->mailbox->magic == MUTT_NOTMUCH) /* only match real mailboxes */
          continue;
        mutt_buffer_expand_path(&np->mailbox->pathbuf);
        if ((found || (pass > 0)) && np->mailbox->has_new)
        {
          mutt_buffer_strcpy(s, mailbox_path(np->mailbox));
          mutt_buffer_pretty_mailbox(s);
          neomutt_mailboxlist_clear(&ml);
          return;
        }
        if (mutt_str_strcmp(mutt_b2s(s), mailbox_path(np->mailbox)) == 0)
          found = true;
      }
      neomutt_mailboxlist_clear(&ml);
    }

    mutt_mailbox_check(m_cur, MUTT_MAILBOX_CHECK_FORCE); /* mailbox was wrong - resync things */
  }

  /* no folders with new mail */
  mutt_buffer_reset(s);
}

/**
 * mutt_mailbox_next - incoming folders completion routine
 * @param m_cur Current Mailbox
 * @param s     Buffer containing name of current mailbox
 * @param slen  Buffer length
 *
 * Given a folder name, find the next incoming folder with new mail.
 */
void mutt_mailbox_next(struct Mailbox *m_cur, char *s, size_t slen)
{
  struct Buffer *s_buf = mutt_buffer_pool_get();

  mutt_buffer_addstr(s_buf, NONULL(s));
  mutt_mailbox_next_buffer(m_cur, s_buf);
  mutt_str_strfcpy(s, mutt_b2s(s_buf), slen);

  mutt_buffer_pool_release(&s_buf);
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
      utimensat(0, buf, ts, 0);
#else
      ut.actime = st->st_atime;
      ut.modtime = mutt_date_epoch();
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
