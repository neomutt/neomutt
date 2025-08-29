/**
 * @file
 * A group of associated Mailboxes
 *
 * @authors
 * Copyright (C) 2018-2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_CORE_ACCOUNT_H
#define MUTT_CORE_ACCOUNT_H

#include <stdbool.h>
#include "mutt/lib.h"
#include "mailbox.h"

struct ConfigSubset;

/**
 * struct Account - A group of associated Mailboxes
 */
struct Account
{
  enum MailboxType type;          ///< Type of Mailboxes this Account contains
  char *name;                     ///< Name of Account
  struct ConfigSubset *sub;       ///< Inherited config items
  struct MailboxList mailboxes;   ///< List of Mailboxes
  struct Notify *notify;          ///< Notifications: #NotifyAccount, #EventAccount
  void *adata;                    ///< Private data (for Mailbox backends)

  /**
   * @defgroup account_adata_free Account Private Data API
   *
   * adata_free - Free the private data attached to the Account
   * @param ptr Private data to be freed
   *
   * @pre ptr  is not NULL
   * @pre *ptr is not NULL
   */
  void (*adata_free)(void **ptr);

  TAILQ_ENTRY(Account) entries;   ///< Linked list
};
TAILQ_HEAD(AccountList, Account);

/**
 * enum NotifyAccount - Types of Account Event
 *
 * Observers of #NT_ACCOUNT will be passed an #EventAccount.
 *
 * @note Delete notifications are sent **before** the object is deleted.
 * @note Other notifications are sent **after** the event.
 */
enum NotifyAccount
{
  NT_ACCOUNT_ADD = 1,    ///< Account has been added
  NT_ACCOUNT_DELETE,     ///< Account is about to be deleted
  NT_ACCOUNT_DELETE_ALL, ///< All Accounts are about to be deleted
  NT_ACCOUNT_CHANGE,     ///< Account has been changed
};

/**
 * struct EventAccount - An Event that happened to an Account
 */
struct EventAccount
{
  struct Account *account; ///< The Account this Event relates to
};

void            account_free          (struct Account **ptr);
bool            account_mailbox_add   (struct Account *a, struct Mailbox *m);
bool            account_mailbox_remove(struct Account *a, struct Mailbox *m);
struct Account *account_new           (const char *name, struct ConfigSubset *sub);

#endif /* MUTT_CORE_ACCOUNT_H */
