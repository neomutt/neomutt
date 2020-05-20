/**
 * @file
 * Shared code for the Alias and Query Dialogs
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
 * @page alias_gui Shared code for the Alias and Query Dialogs
 *
 * Shared code for the Alias and Query Dialogs
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "gui.h"
#include "lib.h"
#include "alias.h"

#define RSORT(num) ((C_SortAlias & SORT_REVERSE) ? -num : num)

/**
 * alias_sort_name - Compare two Aliases by their short names
 * @param a First  Alias to compare
 * @param b Second Alias to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int alias_sort_name(const void *a, const void *b)
{
  const struct Alias *pa = *(struct Alias const *const *) a;
  const struct Alias *pb = *(struct Alias const *const *) b;
  int r = mutt_str_strcasecmp(pa->name, pb->name);

  return RSORT(r);
}

/**
 * alias_sort_address - Compare two Aliases by their Addresses
 * @param a First  Alias to compare
 * @param b Second Alias to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int alias_sort_address(const void *a, const void *b)
{
  const struct AddressList *al_a = &(*(struct Alias const *const *) a)->addr;
  const struct AddressList *al_b = &(*(struct Alias const *const *) b)->addr;
  int r;
  if (al_a == al_b)
    r = 0;
  else if (!al_a)
    r = -1;
  else if (!al_b)
    r = 1;
  else
  {
    const struct Address *addr_a = TAILQ_FIRST(al_a);
    const struct Address *addr_b = TAILQ_FIRST(al_b);
    if (addr_a->personal)
    {
      if (addr_b->personal)
        r = mutt_str_strcasecmp(addr_a->personal, addr_b->personal);
      else
        r = 1;
    }
    else if (addr_b->personal)
      r = -1;
    else
      r = mutt_str_strcasecmp(addr_a->mailbox, addr_b->mailbox);
  }
  return RSORT(r);
}
