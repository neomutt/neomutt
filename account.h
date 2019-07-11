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

#ifndef MUTT_ACCOUNT_H
#define MUTT_ACCOUNT_H

#include <stdbool.h>
#include "mutt/mutt.h"
#include "config/lib.h"
#include "mailbox.h"

struct ConnAccount;
struct Notify;

/**
 * struct Account - A group of associated Mailboxes
 */
struct Account
{
  enum MailboxType magic;       ///< Type of Mailboxes this Account contains
  struct MailboxList mailboxes; ///< List of Mailboxes
  TAILQ_ENTRY(Account) entries; ///< Linked list of Accounts
  struct Notify *notify;        ///< Notifications handler
  void *adata;                  ///< Private data (for Mailbox backends)
  void (*free_adata)(void **);  ///< Callback function to free private data

  char *name;               ///< Name of Account
  struct ConfigSubset *sub; ///< Inherited config items
};
TAILQ_HEAD(AccountList, Account);

/**
 * struct EventAccount - An Event that happened to an Account
 */
struct EventAccount
{
  struct Account *account; ///< The Account this Event relates to
};

/**
 * enum NotifyAccount - Types of Account Event
 */
enum NotifyAccount
{
  NT_ACCOUNT_ADD = 1, ///< A new Account has been created
  NT_ACCOUNT_REMOVE,  ///< An Account is about to be destroyed
};

void            account_free(struct Account **ptr);
bool            account_mailbox_add(struct Account *a, struct Mailbox *m);
bool            account_mailbox_remove(struct Account *a, struct Mailbox *m);
struct Account *account_new(const char *name, struct ConfigSubset *parent);

#endif /* MUTT_ACCOUNT_H */
