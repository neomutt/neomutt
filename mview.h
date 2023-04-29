/**
 * @file
 * The "currently-open" mailbox
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_MVIEW_H
#define MUTT_MVIEW_H

#include <stdbool.h>
#include <sys/types.h>

struct Email;
struct EmailList;
struct Mailbox;
struct NotifyCallback;

/**
 * struct MailboxView - The "current" mailbox
 */
struct MailboxView
{
  off_t vsize;                       ///< Size (in bytes) of the messages shown
  char *pattern;                     ///< Limit pattern string
  struct PatternList *limit_pattern; ///< Compiled limit pattern
  struct ThreadsContext *threads;    ///< Threads context
  int msg_in_pager;                  ///< Message currently shown in the pager

  struct Menu *menu;                 ///< Needed for pattern compilation

  bool collapsed : 1;                ///< Are all threads collapsed?

  struct Mailbox *mailbox;           ///< Current Mailbox
  struct Notify *notify;             ///< Notifications: #NotifyMview, #EventMview
};

/**
 * enum NotifyMview - Types of MailboxView Event
 *
 * Observers of #NT_MVIEW will be passed an #EventMview.
 */
enum NotifyMview
{
  NT_MVIEW_ADD = 1, ///< The Mview has been opened
  NT_MVIEW_DELETE,  ///< The Mview is about to be destroyed
  NT_MVIEW_CHANGE,  ///< The Mview has changed
};

/**
 * struct EventMview - An Event that happened to an MailboxView
 */
struct EventMview
{
  struct MailboxView *mv; ///< The MailboxView this Event relates to
};

void                mview_free            (struct MailboxView **ptr);
int                 mview_mailbox_observer(struct NotifyCallback *nc);
struct MailboxView *mview_new             (struct Mailbox *m);
void                mview_update          (struct MailboxView *mv);
bool                mview_has_limit       (const struct MailboxView *mv);
struct Mailbox*     mview_mailbox         (struct MailboxView *mv);

bool message_is_tagged(struct Email *e);
struct Email *mutt_get_virt_email(struct Mailbox *m, int vnum);

int  el_add_tagged(struct EmailList *el, struct MailboxView *mv, struct Email *e, bool use_tagged);

bool mutt_limit_current_thread(struct MailboxView *mv, struct Email *e);

#endif /* MUTT_MVIEW_H */
