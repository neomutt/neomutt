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

struct HashTable *ReverseAliases; ///< Hash Table of aliases (email address -> alias)

/**
 * alias_reverse_add - Add an email address lookup for an Alias
 * @param alias Alias to use
 */
void alias_reverse_add(struct Alias *alias)
{
  if (!alias)
    return;

  /* Note that the address mailbox should be converted to intl form
   * before using as a key in the hash.  This is currently done
   * by all callers, but added here mostly as documentation. */
  mutt_addrlist_to_intl(&alias->addr, NULL);

  struct Address *addr = NULL;
  TAILQ_FOREACH(addr, &alias->addr, entries)
  {
    if (!addr->group && addr->mailbox)
      mutt_hash_insert(ReverseAliases, addr->mailbox, addr);
  }
}

/**
 * alias_reverse_delete - Remove an email address lookup for an Alias
 * @param alias Alias to use
 */
void alias_reverse_delete(struct Alias *alias)
{
  if (!alias)
    return;

  /* If the alias addresses were converted to local form, they won't
   * match the hash entries. */
  mutt_addrlist_to_intl(&alias->addr, NULL);

  struct Address *addr = NULL;
  TAILQ_FOREACH(addr, &alias->addr, entries)
  {
    if (!addr->group && addr->mailbox)
      mutt_hash_delete(ReverseAliases, addr->mailbox, addr);
  }
}

/**
 * alias_reverse_lookup - Does the user have an alias for the given address
 * @param addr Address to lookup
 * @retval ptr Matching Address
 */
struct Address *alias_reverse_lookup(const struct Address *addr)
{
  if (!addr || !addr->mailbox)
    return NULL;

  return mutt_hash_find(ReverseAliases, addr->mailbox);
}
