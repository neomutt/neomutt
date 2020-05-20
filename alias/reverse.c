/**
 * @file
 * Manage alias reverse lookups
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page alias_reverse Manage alias reverse lookups
 *
 * Manage alias reverse lookups
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "alias.h"

struct Hash *ReverseAliases; ///< Hash table of aliases (email address -> alias)

/**
 * mutt_alias_add_reverse - Add an email address lookup for an Alias
 * @param t Alias to use
 */
void mutt_alias_add_reverse(struct Alias *t)
{
  if (!t)
    return;

  /* Note that the address mailbox should be converted to intl form
   * before using as a key in the hash.  This is currently done
   * by all callers, but added here mostly as documentation. */
  mutt_addrlist_to_intl(&t->addr, NULL);

  struct Address *a = NULL;
  TAILQ_FOREACH(a, &t->addr, entries)
  {
    if (!a->group && a->mailbox)
      mutt_hash_insert(ReverseAliases, a->mailbox, a);
  }
}

/**
 * mutt_alias_delete_reverse - Remove an email address lookup for an Alias
 * @param t Alias to use
 */
void mutt_alias_delete_reverse(struct Alias *t)
{
  if (!t)
    return;

  /* If the alias addresses were converted to local form, they won't
   * match the hash entries. */
  mutt_addrlist_to_intl(&t->addr, NULL);

  struct Address *a = NULL;
  TAILQ_FOREACH(a, &t->addr, entries)
  {
    if (!a->group && a->mailbox)
      mutt_hash_delete(ReverseAliases, a->mailbox, a);
  }
}

/**
 * mutt_alias_reverse_lookup - Does the user have an alias for the given address
 * @param a Address to lookup
 * @retval ptr Matching Address
 */
struct Address *mutt_alias_reverse_lookup(const struct Address *a)
{
  if (!a || !a->mailbox)
    return NULL;

  return mutt_hash_find(ReverseAliases, a->mailbox);
}
