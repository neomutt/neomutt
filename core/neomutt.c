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
 * @page core_neomutt Container for Accounts, Notifications
 *
 * Container for Accounts, Notifications
 */

#include "config.h"
#include <stddef.h>
#include "mutt/mutt.h"
#include "config/lib.h"
#include "neomutt.h"
#include "account.h"
#include "mailbox.h"

struct NeoMutt *NeoMutt; ///< Global NeoMutt object

/**
 * neomutt_new - Create the master NeoMutt object
 * @param cs Config Set
 * @retval ptr New NeoMutt
 */
struct NeoMutt *neomutt_new(struct ConfigSet *cs)
{
  if (!cs)
    return NULL;

  struct NeoMutt *n = mutt_mem_calloc(1, sizeof(*NeoMutt));

  TAILQ_INIT(&n->accounts);
  n->notify = notify_new();
  n->sub = cs_subset_new(NULL, NULL, n->notify);
  n->sub->cs = cs;
  n->sub->scope = SET_SCOPE_NEOMUTT;

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
  cs_subset_free(&n->sub);
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
  notify_send(n->notify, NT_ACCOUNT, NT_ACCOUNT_ADD, &ev_a);
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
      notify_send(n->notify, NT_ACCOUNT, NT_ACCOUNT_REMOVE, &ev_a);
      TAILQ_REMOVE(&n->accounts, np, entries);
      account_free(&np);
      result = true;
      if (a)
        break;
    }
  }
  return result;
}

/**
 * neomutt_mailboxlist_clear - Free a Mailbox List
 * @param ml Mailbox List to free
 *
 * @note The Mailboxes aren't freed
 */
void neomutt_mailboxlist_clear(struct MailboxList *ml)
{
  if (!ml)
    return;

  struct MailboxNode *mn = NULL;
  struct MailboxNode *tmp = NULL;
  STAILQ_FOREACH_SAFE(mn, ml, entries, tmp)
  {
    STAILQ_REMOVE(ml, mn, MailboxNode, entries);
    FREE(&mn);
  }
}

/**
 * neomutt_mailboxlist_get_all - Get a List of all Mailboxes
 * @param n     NeoMutt
 * @param magic Type of Account to match, see #MailboxType
 * @retval obj List of Mailboxes
 *
 * @note If magic is #MUTT_MAILBOX_ANY then all Mailbox types will be matched
 */
struct MailboxList neomutt_mailboxlist_get_all(struct NeoMutt *n, enum MailboxType magic)
{
  struct MailboxList ml = STAILQ_HEAD_INITIALIZER(ml);
  if (!n)
    return ml;

  struct Account *a = NULL;
  struct MailboxNode *mn = NULL;

  TAILQ_FOREACH(a, &n->accounts, entries)
  {
    if ((magic > MUTT_UNKNOWN) && (a->magic != magic))
      continue;

    STAILQ_FOREACH(mn, &a->mailboxes, entries)
    {
      struct MailboxNode *mn2 = mutt_mem_calloc(1, sizeof(*mn2));
      mn2->mailbox = mn->mailbox;
      STAILQ_INSERT_TAIL(&ml, mn2, entries);
    }
  }

  return ml;
}
