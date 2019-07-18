/**
 * @file
 * A group of associated Mailboxes
 *
 * @authors
 * Copyright (C) 2018-2019 Richard Russon <rich@flatcap.org>
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
 * @page core_account A group of associated Mailboxes
 *
 * A group of associated Mailboxes
 */

#include "config.h"
#include <stddef.h>
#include "mutt/mutt.h"
#include "config/lib.h"
#include "account.h"
#include "mailbox.h"

/**
 * account_new - Create a new Account
 * @param name Name for the Account
 * @param sub  Parent Config Subset
 * @retval ptr New Account
 */
struct Account *account_new(const char *name, struct ConfigSubset *sub)
{
  struct Account *a = mutt_mem_calloc(1, sizeof(struct Account));

  STAILQ_INIT(&a->mailboxes);
  a->notify = notify_new(a, NT_ACCOUNT);
  a->name = mutt_str_strdup(name);
  a->sub = cs_subset_new(name, sub);

  return a;
}

/**
 * account_mailbox_add - Add a Mailbox to an Account
 * @param a Account
 * @param m Mailbox to add
 * @retval true If Mailbox was added
 */
bool account_mailbox_add(struct Account *a, struct Mailbox *m)
{
  if (!a || !m)
    return false;

  m->account = a;
  struct MailboxNode *np = mutt_mem_calloc(1, sizeof(*np));
  np->mailbox = m;
  STAILQ_INSERT_TAIL(&a->mailboxes, np, entries);
  notify_set_parent(m->notify, a->notify);

  struct EventMailbox ev_m = { m };
  notify_send(a->notify, NT_MAILBOX, NT_MAILBOX_ADD, IP & ev_m);
  return true;
}

/**
 * account_mailbox_remove - Remove a Mailbox from an Account
 * @param a Account
 * @param m Mailbox to remove
 *
 * @note If m is NULL, all the mailboxes will be removed
 */
bool account_mailbox_remove(struct Account *a, struct Mailbox *m)
{
  if (!a)
    return false;

  bool result = false;
  struct MailboxNode *np = NULL;
  struct MailboxNode *tmp = NULL;
  STAILQ_FOREACH_SAFE(np, &a->mailboxes, entries, tmp)
  {
    if (!m || (np->mailbox == m))
    {
      struct EventMailbox ev_m = { m };
      notify_send(a->notify, NT_MAILBOX, NT_MAILBOX_REMOVE, IP & ev_m);
      STAILQ_REMOVE(&a->mailboxes, np, MailboxNode, entries);
      mailbox_free(&np->mailbox);
      FREE(&np);
      result = true;
      if (m)
        break;
    }
  }

  return result;
}

/**
 * account_free - Free an Account
 * @param[out] ptr Account to free
 */
void account_free(struct Account **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Account *a = *ptr;

  if (a->free_adata)
    a->free_adata(&a->adata);

  account_mailbox_remove(a, NULL);
  cs_subset_free(&a->sub);
  FREE(&a->name);
  notify_free(&a->notify);

  FREE(ptr);
}
