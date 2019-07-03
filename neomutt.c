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

/**
 * @page neomutt Container for Accounts, Notifications
 *
 * Container for Accounts, Notifications
 */

#include "config.h"
#include "mutt/mutt.h"
#include "neomutt.h"
#include "account.h"

struct NeoMutt *NeoMutt; ///< Global NeoMutt object

/**
 * neomutt_new - Create the master NeoMutt object
 * @retval ptr New NeoMutt
 */
struct NeoMutt *neomutt_new(void)
{
  struct NeoMutt *n = mutt_mem_calloc(1, sizeof(*NeoMutt));

  TAILQ_INIT(&n->accounts);
  n->notify = notify_new(n, NT_NEOMUTT);

  return n;
}

/**
 * neomutt_free - Free a NeoMutt
 * @param[out] ptr NeoMutt to free
 */
void neomutt_free(struct NeoMutt **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct NeoMutt *n = *ptr;

  neomutt_account_remove(n, NULL);
  notify_free(&n->notify);

  FREE(ptr);
}

/**
 * neomutt_account_add - Add an Account to the global list
 * @param n NeoMutt
 * @param a Account to add
 * @retval true If Account was added
 */
bool neomutt_account_add(struct NeoMutt *n, struct Account *a)
{
  if (!n || !a)
    return false;

  TAILQ_INSERT_TAIL(&n->accounts, a, entries);
  notify_set_parent(a->notify, n->notify);

  struct EventAccount ev_a = { a };
  notify_send(n->notify, NT_ACCOUNT, NT_ACCOUNT_ADD, IP & ev_a);
  return true;
}

/**
 * neomutt_account_remove - Remove an Account from the global list
 * @param n NeoMutt
 * @param a Account to remove
 * @retval true If Account was removed
 *
 * @note If a is NULL, all the Accounts will be removed
 */
bool neomutt_account_remove(struct NeoMutt *n, struct Account *a)
{
  if (!n)
    return false;

  bool result = false;
  struct Account *np = NULL;
  struct Account *tmp = NULL;
  TAILQ_FOREACH_SAFE(np, &n->accounts, entries, tmp)
  {
    if (!a || (np == a))
    {
      struct EventAccount ev_a = { np };
      notify_send(n->notify, NT_ACCOUNT, NT_ACCOUNT_REMOVE, IP & ev_a);
      TAILQ_REMOVE(&n->accounts, np, entries);
      account_free(&np);
      result = true;
      if (a)
        break;
    }
  }
  return result;
}
