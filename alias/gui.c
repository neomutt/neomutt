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
 * alias_sort_alias - Compare two Aliases
 * @param a First  Alias to compare
 * @param b Second Alias to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int alias_sort_alias(const void *a, const void *b)
{
  const struct Alias *pa = *(struct Alias const *const *) a;
  const struct Alias *pb = *(struct Alias const *const *) b;
  int r = mutt_str_strcasecmp(pa->name, pb->name);

  return RSORT(r);
}

/**
 * alias_sort_address - Compare two Addresses
 * @param a First  Address to compare
 * @param b Second Address to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int alias_sort_address(const void *a, const void *b)
{
  const struct AddressList *pal = &(*(struct Alias const *const *) a)->addr;
  const struct AddressList *pbl = &(*(struct Alias const *const *) b)->addr;
  int r;

  if (pal == pbl)
    r = 0;
  else if (!pal)
    r = -1;
  else if (!pbl)
    r = 1;
  else
  {
    const struct Address *pa = TAILQ_FIRST(pal);
    const struct Address *pb = TAILQ_FIRST(pbl);
    if (pa->personal)
    {
      if (pb->personal)
        r = mutt_str_strcasecmp(pa->personal, pb->personal);
      else
        r = 1;
    }
    else if (pb->personal)
      r = -1;
    else
      r = mutt_str_strcasecmp(pa->mailbox, pb->mailbox);
  }
  return RSORT(r);
}
