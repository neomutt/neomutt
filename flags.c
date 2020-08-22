/**
 * @file
 * Manipulate the flags in an email header
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
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
 * @page flags Manipulate the flags in an email header
 *
 * Manipulate the flags in an email header
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "context.h"
#include "index.h"
#include "keymap.h"
#include "mutt_globals.h"
#include "mutt_menu.h"
#include "protos.h"
#include "sort.h"

/**
 * mutt_set_flag_update - Set a flag on an email
 * @param m        Mailbox
 * @param e        Email
 * @param flag     Flag to set, e.g. #MUTT_DELETE
 * @param bf       true: set the flag; false: clear the flag
 * @param upd_mbox true: update the Mailbox
 */
void mutt_set_flag_update(struct Mailbox *m, struct Email *e, int flag, bool bf, bool upd_mbox)
{
  if (!m || !e)
    return;

  bool changed = e->changed;
  int deleted = m->msg_deleted;
  int tagged = m->msg_tagged;
  int flagged = m->msg_flagged;
  int update = false;

  if (m->readonly && (flag != MUTT_TAG))
    return; /* don't modify anything if we are read-only */

  switch (flag)
  {
    case MUTT_DELETE:

      if (!(m->rights & MUTT_ACL_DELETE))
        return;

      if (bf)
      {
        if (!e->deleted && !m->readonly && (!e->flagged || !C_FlagSafe))
        {
          e->deleted = true;
          update = true;
          if (upd_mbox)
            m->msg_deleted++;
#ifdef USE_IMAP
          /* deleted messages aren't treated as changed elsewhere so that the
           * purge-on-sync option works correctly. This isn't applicable here */
          if (m->type == MUTT_IMAP)
          {
            e->changed = true;
            if (upd_mbox)
              m->changed = true;
          }
#endif
        }
      }
      else if (e->deleted)
      {
        e->deleted = false;
        update = true;
        if (upd_mbox)
          m->msg_deleted--;
#ifdef USE_IMAP
        /* see my comment above */
        if (m->type == MUTT_IMAP)
        {
          e->changed = true;
          if (upd_mbox)
            m->changed = true;
        }
#endif
        /* If the user undeletes a message which is marked as
         * "trash" in the maildir folder on disk, the folder has
         * been changed, and is marked accordingly.  However, we do
         * _not_ mark the message itself changed, because trashing
         * is checked in specific code in the maildir folder
         * driver.  */
        if ((m->type == MUTT_MAILDIR) && upd_mbox && e->trash)
          m->changed = true;
      }
      break;

    case MUTT_PURGE:

      if (!(m->rights & MUTT_ACL_DELETE))
        return;

      if (bf)
      {
        if (!e->purge && !m->readonly)
          e->purge = true;
      }
      else if (e->purge)
        e->purge = false;
      break;

    case MUTT_NEW:

      if (!(m->rights & MUTT_ACL_SEEN))
        return;

      if (bf)
      {
        if (e->read || e->old)
        {
          update = true;
          e->old = false;
          if (upd_mbox)
            m->msg_new++;
          if (e->read)
          {
            e->read = false;
            if (upd_mbox)
              m->msg_unread++;
          }
          e->changed = true;
          if (upd_mbox)
            m->changed = true;
        }
      }
      else if (!e->read)
      {
        update = true;
        if (!e->old)
          if (upd_mbox)
            m->msg_new--;
        e->read = true;
        if (upd_mbox)
          m->msg_unread--;
        e->changed = true;
        if (upd_mbox)
          m->changed = true;
      }
      break;

    case MUTT_OLD:

      if (!(m->rights & MUTT_ACL_SEEN))
        return;

      if (bf)
      {
        if (!e->old)
        {
          update = true;
          e->old = true;
          if (!e->read)
            if (upd_mbox)
              m->msg_new--;
          e->changed = true;
          if (upd_mbox)
            m->changed = true;
        }
      }
      else if (e->old)
      {
        update = true;
        e->old = false;
        if (!e->read)
          if (upd_mbox)
            m->msg_new++;
        e->changed = true;
        if (upd_mbox)
          m->changed = true;
      }
      break;

    case MUTT_READ:

      if (!(m->rights & MUTT_ACL_SEEN))
        return;

      if (bf)
      {
        if (!e->read)
        {
          update = true;
          e->read = true;
          if (upd_mbox)
            m->msg_unread--;
          if (!e->old)
            if (upd_mbox)
              m->msg_new--;
          e->changed = true;
          if (upd_mbox)
            m->changed = true;
        }
      }
      else if (e->read)
      {
        update = true;
        e->read = false;
        if (upd_mbox)
          m->msg_unread++;
        if (!e->old)
          if (upd_mbox)
            m->msg_new++;
        e->changed = true;
        if (upd_mbox)
          m->changed = true;
      }
      break;

    case MUTT_REPLIED:

      if (!(m->rights & MUTT_ACL_WRITE))
        return;

      if (bf)
      {
        if (!e->replied)
        {
          update = true;
          e->replied = true;
          if (!e->read)
          {
            e->read = true;
            if (upd_mbox)
              m->msg_unread--;
            if (!e->old)
              if (upd_mbox)
                m->msg_new--;
          }
          e->changed = true;
          if (upd_mbox)
            m->changed = true;
        }
      }
      else if (e->replied)
      {
        update = true;
        e->replied = false;
        e->changed = true;
        if (upd_mbox)
          m->changed = true;
      }
      break;

    case MUTT_FLAG:

      if (!(m->rights & MUTT_ACL_WRITE))
        return;

      if (bf)
      {
        if (!e->flagged)
        {
          update = true;
          e->flagged = bf;
          if (upd_mbox)
            m->msg_flagged++;
          e->changed = true;
          if (upd_mbox)
            m->changed = true;
        }
      }
      else if (e->flagged)
      {
        update = true;
        e->flagged = false;
        if (upd_mbox)
          m->msg_flagged--;
        e->changed = true;
        if (upd_mbox)
          m->changed = true;
      }
      break;

    case MUTT_TAG:
      if (bf)
      {
        if (!e->tagged)
        {
          update = true;
          e->tagged = true;
          if (upd_mbox)
            m->msg_tagged++;
        }
      }
      else if (e->tagged)
      {
        update = true;
        e->tagged = false;
        if (upd_mbox)
          m->msg_tagged--;
      }
      break;
  }

  if (update)
  {
    mutt_set_header_color(m, e);
  }

  /* if the message status has changed, we need to invalidate the cached
   * search results so that any future search will match the current status
   * of this message and not what it was at the time it was last searched.  */
  if (e->searched && ((changed != e->changed) || (deleted != m->msg_deleted) ||
                      (tagged != m->msg_tagged) || (flagged != m->msg_flagged)))
  {
    e->searched = false;
  }
}

/**
 * mutt_emails_set_flag - Set flag on messages
 * @param m    Mailbox
 * @param el   List of Emails to flag
 * @param flag Flag to set, e.g. #MUTT_DELETE
 * @param bf   true: set the flag; false: clear the flag
 */
void mutt_emails_set_flag(struct Mailbox *m, struct EmailList *el, int flag, bool bf)
{
  if (!m || !el || STAILQ_EMPTY(el))
    return;

  struct EmailNode *en = NULL;
  STAILQ_FOREACH(en, el, entries)
  {
    mutt_set_flag(m, en->email, flag, bf);
  }
}

/**
 * mutt_thread_set_flag - Set a flag on an entire thread
 * @param e         Email
 * @param flag      Flag to set, e.g. #MUTT_DELETE
 * @param bf        true: set the flag; false: clear the flag
 * @param subthread If true apply to all of the thread
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_thread_set_flag(struct Email *e, int flag, bool bf, bool subthread)
{
  struct MuttThread *start = NULL;
  struct MuttThread *cur = e->thread;

  if ((C_Sort & SORT_MASK) != SORT_THREADS)
  {
    mutt_error(_("Threading is not enabled"));
    return -1;
  }

  if (!subthread)
    while (cur->parent)
      cur = cur->parent;
  start = cur;

  if (cur->message && (cur != e->thread))
    mutt_set_flag(Context->mailbox, cur->message, flag, bf);

  cur = cur->child;
  if (!cur)
    goto done;

  while (true)
  {
    if (cur->message && (cur != e->thread))
      mutt_set_flag(Context->mailbox, cur->message, flag, bf);

    if (cur->child)
      cur = cur->child;
    else if (cur->next)
      cur = cur->next;
    else
    {
      while (!cur->next)
      {
        cur = cur->parent;
        if (cur == start)
          goto done;
      }
      cur = cur->next;
    }
  }
done:
  cur = e->thread;
  if (cur->message)
    mutt_set_flag(Context->mailbox, cur->message, flag, bf);
  return 0;
}

/**
 * mutt_change_flag - Change the flag on a Message
 * @param m  Mailbox
 * @param el List of Emails to change
 * @param bf true: set the flag; false: clear the flag
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_change_flag(struct Mailbox *m, struct EmailList *el, bool bf)
{
  if (!m || !el || STAILQ_EMPTY(el))
    return -1;

  int flag;
  struct KeyEvent event;

  mutt_window_mvprintw(MessageWindow, 0, 0,
                       "%s? (D/N/O/r/*/!): ", bf ? _("Set flag") : _("Clear flag"));
  mutt_window_clrtoeol(MessageWindow);
  mutt_refresh();

  do
  {
    event = mutt_getch();
  } while (event.ch == -2);
  int i = event.ch;
  if (i < 0)
  {
    mutt_window_clearline(MessageWindow, 0);
    return -1;
  }

  mutt_window_clearline(MessageWindow, 0);

  switch (i)
  {
    case 'd':
    case 'D':
      if (!bf)
        mutt_emails_set_flag(m, el, MUTT_PURGE, bf);
      flag = MUTT_DELETE;
      break;

    case 'N':
    case 'n':
      flag = MUTT_NEW;
      break;

    case 'o':
    case 'O':
      mutt_emails_set_flag(m, el, MUTT_READ, !bf);
      flag = MUTT_OLD;
      break;

    case 'r':
    case 'R':
      flag = MUTT_REPLIED;
      break;

    case '*':
      flag = MUTT_TAG;
      break;

    case '!':
      flag = MUTT_FLAG;
      break;

    default:
      mutt_beep(false);
      return -1;
  }

  mutt_emails_set_flag(m, el, flag, bf);
  return 0;
}
