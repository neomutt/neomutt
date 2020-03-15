/**
 * @file
 * Container for Accounts, Notifications
 *
 * @authors
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

#ifndef MUTT_CORE_NEOMUTT_H
#define MUTT_CORE_NEOMUTT_H

#include <stdbool.h>
#include "account.h"
#include "mailbox.h"

struct ConfigSet;

/**
 * struct NeoMutt - Container for Accounts, Notifications
 */
struct NeoMutt
{
  struct Notify *notify;       ///< Notifications handler
  struct ConfigSubset *sub;    ///< Inherited config items
  struct AccountList accounts; ///< List of all Accounts
};

extern struct NeoMutt *NeoMutt;

/**
 * NotifyGlobal - Events not associated with an object
 */
enum NotifyGlobal
{
  NT_GLOBAL_STARTUP = 1, ///< NeoMutt is initialised
  NT_GLOBAL_SHUTDOWN,    ///< NeoMutt is about to close
  NT_GLOBAL_TIMEOUT,     ///< A timer has elapsed
};

bool            neomutt_account_add   (struct NeoMutt *n, struct Account *a);
bool            neomutt_account_remove(struct NeoMutt *n, struct Account *a);
void            neomutt_free          (struct NeoMutt **ptr);
struct NeoMutt *neomutt_new           (struct ConfigSet *cs);

void               neomutt_mailboxlist_clear  (struct MailboxList *ml);
struct MailboxList neomutt_mailboxlist_get_all(struct NeoMutt *n, enum MailboxType type);

#endif /* MUTT_CORE_NEOMUTT_H */
