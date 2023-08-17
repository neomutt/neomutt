/**
 * @file
 * Container for Accounts, Notifications
 *
 * @authors
 * Copyright (C) 2019-2023 Richard Russon <rich@flatcap.org>
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

#include "config.h"
#include <stddef.h>
#include <locale.h>
#include <stdbool.h>
#include "account.h"
#include "mailbox.h"
#ifdef __APPLE__
#include <xlocale.h>
#endif

struct ConfigSet;

/**
 * struct NeoMutt - Container for Accounts, Notifications
 */
struct NeoMutt
{
  struct Notify *notify;         ///< Notifications handler
  struct Notify *notify_resize;  ///< Window resize notifications handler
  struct Notify *notify_timeout; ///< Timeout notifications handler
  struct ConfigSubset *sub;      ///< Inherited config items
  struct AccountList accounts;   ///< List of all Accounts
  locale_t time_c_locale;        ///< Current locale but LC_TIME=C
#ifdef USE_MONITOR
  struct Monitor *mon;           ///< Filesystem monitor
#endif
};

extern struct NeoMutt *NeoMutt;

/**
 * enum NotifyGlobal - Events not associated with an object
 *
 * Observers of #NT_GLOBAL will not be passed any Event data.
 */
enum NotifyGlobal
{
  NT_GLOBAL_STARTUP = 1, ///< NeoMutt is initialised
  NT_GLOBAL_SHUTDOWN,    ///< NeoMutt is about to close
  NT_GLOBAL_COMMAND,     ///< A NeoMutt command
};

bool            neomutt_account_add   (struct NeoMutt *n, struct Account *a);
bool            neomutt_account_remove(struct NeoMutt *n, const struct Account *a);
void            neomutt_free          (struct NeoMutt **ptr);
struct NeoMutt *neomutt_new           (struct ConfigSet *cs);

void   neomutt_mailboxlist_clear  (struct MailboxList *ml);
size_t neomutt_mailboxlist_get_all(struct MailboxList *head, struct NeoMutt *n, enum MailboxType type);

#endif /* MUTT_CORE_NEOMUTT_H */
