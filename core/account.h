/**
 * @file
 * A group of associated Mailboxes
 *
 * @authors
 * Copyright (C) 1996-2000,2010,2013 Michael R. Elkins <me@mutt.org>
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
  struct Notify *notify;          ///< Notifications handler
  void *adata;                    ///< Private data (for Mailbox backends)
  void (*adata_free)(void **ptr); ///< Callback function to free private data
  TAILQ_ENTRY(Account) entries;   ///< Linked list of Accounts
};
TAILQ_HEAD(AccountList, Account);

/**
 * enum NotifyAccount - Types of Account Event
 *
 * Observers of #NT_ACCOUNT will be passed an #EventAccount.
 */
enum NotifyAccount
{
  NT_ACCOUNT_ADD = 1, ///< A new Account has been created
  NT_ACCOUNT_REMOVE,  ///< An Account is about to be destroyed
};

/**
 * struct EventAccount - An Event that happened to an Account
 */
struct EventAccount
{
  struct Account *account; ///< The Account this Event relates to
};

struct Account *account_find          (const char *name);
void            account_free          (struct Account **ptr);
bool            account_mailbox_add   (struct Account *a, struct Mailbox *m);
bool            account_mailbox_remove(struct Account *a, struct Mailbox *m);
struct Account *account_new           (const char *name, struct ConfigSubset *sub);

#endif /* MUTT_CORE_ACCOUNT_H */
