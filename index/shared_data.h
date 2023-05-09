/**
 * @file
 * Data shared between Index, Pager and Sidebar
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_INDEX_SHARED_DATA_H
#define MUTT_INDEX_SHARED_DATA_H

#include <stdbool.h>
#include <stdio.h>

struct MailboxView;
struct Email;
struct MuttWindow;

/**
 * struct IndexSharedData - Data shared between Index, Pager and Sidebar
 */
struct IndexSharedData
{
  struct ConfigSubset *sub;         ///< Config set to use
  struct MailboxView *mailbox_view; ///< Current Mailbox view
  struct Account *account;          ///< Current Account
  struct Mailbox *mailbox;          ///< Current Mailbox
  struct Email *email;              ///< Currently selected Email
  size_t email_seq;                 ///< Sequence number of the current email
  struct Notify *notify;            ///< Notifications: #NotifyIndex, #IndexSharedData
};

void                    index_shared_data_free(struct MuttWindow *win, void **ptr);
struct IndexSharedData *index_shared_data_new (void);

bool index_shared_data_is_cur_email(const struct IndexSharedData *shared, const struct Email *e);
void index_shared_data_set_mview (struct IndexSharedData *shared, struct MailboxView *mv);
void index_shared_data_set_email   (struct IndexSharedData *shared, struct Email *e);

#endif /* MUTT_INDEX_SHARED_DATA_H */
