/**
 * @file
 * Array of Alias Views
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
 * @page alias_array Array of Alias Views
 *
 * Array of Alias Views
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "gui.h"

struct Alias;

/**
 * alias_array_alias_add - Add an Alias to the AliasViewArray
 * @param ava Array of Aliases
 * @param alias Alias to add
 * @retval num Size of array
 * @retval -1  Error
 *
 * @note The Alias is wrapped in an AliasView
 * @note Call alias_array_sort() to sort and reindex the AliasViewArray
 */
int alias_array_alias_add(struct AliasViewArray *ava, struct Alias *alias)
{
  if (!ava || !alias)
    return -1;

  struct AliasView av = {
    .num = 0,
    .orig_seq = ARRAY_SIZE(ava),
    .is_tagged = false,
    .is_deleted = false,
    .is_visible = true,
    .alias = alias,
  };
  ARRAY_ADD(ava, av);
  return ARRAY_SIZE(ava);
}

/**
 * alias_array_alias_delete - Delete an Alias from the AliasViewArray
 * @param ava    Array of Aliases
 * @param alias Alias to remove
 * @retval num Size of array
 * @retval -1  Error
 *
 * @note Call alias_array_sort() to sort and reindex the AliasViewArray
 */
int alias_array_alias_delete(struct AliasViewArray *ava, const struct Alias *alias)
{
  if (!ava || !alias)
    return -1;

  struct AliasView *avp = NULL;
  ARRAY_FOREACH(avp, ava)
  {
    if (avp->alias != alias)
      continue;

    ARRAY_REMOVE(ava, avp);
    break;
  }

  return ARRAY_SIZE(ava);
}

/**
 * alias_array_count_visible - Count number of visible Aliases
 * @param ava Array of Aliases
 */
int alias_array_count_visible(struct AliasViewArray *ava)
{
  int count = 0;

  struct AliasView *avp = NULL;
  ARRAY_FOREACH(avp, ava)
  {
    if (avp->is_visible)
      count++;
  }

  return count;
}
