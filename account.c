/**
 * @file
 * Representation of an account
 *
 * @authors
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

#include "config.h"
#include "mutt/mutt.h"
#include "account.h"

struct AccountList AllAccounts = TAILQ_HEAD_INITIALIZER(AllAccounts);

/**
 * account_create - Create a new Account
 * @retval ptr New Account
 */
struct Account *account_create(void)
{
  struct Account *a = mutt_mem_calloc(1, sizeof(struct Account));
  STAILQ_INIT(&a->mailboxes);

  return a;
}

/**
 * account_remove_mailbox - Remove a Mailbox from an Account
 * @param a Account
 * @param m Mailbox to remove
 */
void account_remove_mailbox(struct Account *a, struct Mailbox *m)
{
  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &a->mailboxes, entries)
  {
    if (np->m == m)
    {
      STAILQ_REMOVE(&a->mailboxes, np, MailboxNode, entries);
      break;
    }
  }

  if (STAILQ_EMPTY(&a->mailboxes))
  {
    TAILQ_REMOVE(&AllAccounts, a, entries);
    if (a->adata)
      a->free_adata(&a->adata);
    FREE(&a);
  }
}
