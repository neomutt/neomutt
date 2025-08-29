/**
 * @file
 * Manipulate the flags in an email header
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2016-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2017 Fabrice Bellet <fabrice@bellet.info>
 * Copyright (C) 2018 Mehdi Abaakouk <sileht@sileht.net>
 * Copyright (C) 2022 Pietro Cerutti <gahr@gahr.ch>
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
 * @page neo_flags Manipulate the flags in an email header
 *
 * Manipulate the flags in an email header
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "color/lib.h"
#include "index/lib.h"
#include "key/lib.h"
#include "mutt_thread.h"
#include "protos.h"

/**
 * mutt_set_flag - Set a flag on an email
 * @param m        Mailbox
 * @param e        Email
 * @param flag     Flag to set, e.g. #MUTT_DELETE
 * @param bf       true: set the flag; false: clear the flag
 * @param upd_mbox true: update the Mailbox
 */
void mutt_set_flag(struct Mailbox *m, struct Email *e, enum MessageType flag,
                   bool bf, bool upd_mbox)
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
    {
      if (!(m->rights & MUTT_ACL_DELETE))
        return;

      if (bf)
      {
        const bool c_flag_safe = cs_subset_bool(NeoMutt->sub, "flag_safe");
        if (!e->deleted && !m->readonly && (!e->flagged || !c_flag_safe))
        {
          e->deleted = true;
          update = true;
          if (upd_mbox)
            m->msg_deleted++;
          /* deleted messages aren't treated as changed elsewhere so that the
           * purge-on-sync option works correctly. This isn't applicable here */
          if (m->type == MUTT_IMAP)
          {
            e->changed = true;
            if (upd_mbox)
              m->changed = true;
          }
        }
      }
      else if (e->deleted)
      {
        e->deleted = false;
        update = true;
        if (upd_mbox)
          m->msg_deleted--;
        /* see my comment above */
        if (m->type == MUTT_IMAP)
        {
          e->changed = true;
          if (upd_mbox)
            m->changed = true;
        }
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
    }
    case MUTT_PURGE:
    {
      if (!(m->rights & MUTT_ACL_DELETE))
        return;

      if (bf)
      {
        if (!e->purge && !m->readonly)
          e->purge = true;
      }
      else if (e->purge)
      {
        e->purge = false;
      }
      break;
    }
    case MUTT_NEW:
    {
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
    }
    case MUTT_OLD:
    {
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
    }
    case MUTT_READ:
    {
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
    }
    case MUTT_REPLIED:
    {
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
    }
    case MUTT_FLAG:
    {
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
    }
    case MUTT_TAG:
    {
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
    default:
    {
      break;
    }
  }

  if (update)
  {
    email_set_color(m, e);
    struct EventMailbox ev_m = { m };
    notify_send(m->notify, NT_MAILBOX, NT_MAILBOX_CHANGE, &ev_m);
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
 * @param ea   Array of Emails to flag
 * @param flag Flag to set, e.g. #MUTT_DELETE
 * @param bf   true: set the flag; false: clear the flag
 */
void mutt_emails_set_flag(struct Mailbox *m, struct EmailArray *ea,
                          enum MessageType flag, bool bf)
{
  if (!m || !ea || ARRAY_EMPTY(ea))
    return;

  struct Email **ep = NULL;
  ARRAY_FOREACH(ep, ea)
  {
    struct Email *e = *ep;
    mutt_set_flag(m, e, flag, bf, true);
  }
}

/**
 * mutt_thread_set_flag - Set a flag on an entire thread
 * @param m         Mailbox
 * @param e         Email
 * @param flag      Flag to set, e.g. #MUTT_DELETE
 * @param bf        true: set the flag; false: clear the flag
 * @param subthread If true apply to all of the thread
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_thread_set_flag(struct Mailbox *m, struct Email *e,
                         enum MessageType flag, bool bf, bool subthread)
{
  struct MuttThread *start = NULL;
  struct MuttThread *cur = e->thread;

  if (!mutt_using_threads())
  {
    mutt_error(_("Threading is not enabled"));
    return -1;
  }

  if (!subthread)
    while (cur->parent)
      cur = cur->parent;

  start = cur;

  if (cur->message && (cur != e->thread))
    mutt_set_flag(m, cur->message, flag, bf, true);

  cur = cur->child;
  if (!cur)
    goto done;

  while (true)
  {
    if (cur->message && (cur != e->thread))
      mutt_set_flag(m, cur->message, flag, bf, true);

    if (cur->child)
    {
      cur = cur->child;
    }
    else if (cur->next)
    {
      cur = cur->next;
    }
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
    mutt_set_flag(m, cur->message, flag, bf, true);
  return 0;
}

/**
 * mw_change_flag - Change the flag on a Message - @ingroup gui_mw
 * @param m  Mailbox
 * @param ea Array of Emails to change
 * @param bf true: set the flag; false: clear the flag
 * @retval  0 Success
 * @retval -1 Failure
 *
 * This function uses the message window.
 *
 * Ask the user which flag they'd like to set/clear, e.g.
 * `Clear flag? (D/N/O/r/!):`
 */
int mw_change_flag(struct Mailbox *m, struct EmailArray *ea, bool bf)
{
  if (!m || !ea || ARRAY_EMPTY(ea))
    return -1;

  // blank window (0, 0)
  struct MuttWindow *win = msgwin_new(true);
  if (!win)
    return -1;

  char prompt[256] = { 0 };
  snprintf(prompt, sizeof(prompt),
           "%s? (D/N/O/r/*/!): ", bf ? _("Set flag") : _("Clear flag"));
  msgwin_set_text(win, prompt, MT_COLOR_PROMPT);

  msgcont_push_window(win);
  struct MuttWindow *old_focus = window_set_focus(win);
  window_redraw(win);

  struct KeyEvent event = { 0, OP_NULL };
  do
  {
    window_redraw(NULL);
    event = mutt_getch(GETCH_NO_FLAGS);
  } while ((event.op == OP_TIMEOUT) || (event.op == OP_REPAINT));

  win = msgcont_pop_window();
  window_set_focus(old_focus);
  mutt_window_free(&win);

  if (event.op == OP_ABORT)
    return -1;

  enum MessageType flag = MUTT_NONE;
  switch (event.ch)
  {
    case 'd':
    case 'D':
      if (!bf)
        mutt_emails_set_flag(m, ea, MUTT_PURGE, bf);
      flag = MUTT_DELETE;
      break;

    case 'N':
    case 'n':
      flag = MUTT_NEW;
      break;

    case 'o':
    case 'O':
      mutt_emails_set_flag(m, ea, MUTT_READ, !bf);
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

  mutt_emails_set_flag(m, ea, flag, bf);
  return 0;
}
