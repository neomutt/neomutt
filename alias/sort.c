/**
 * @file
 * Address book sorting functions
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
 * @page alias_sort Address book sorting functions
 *
 * Address book sorting functions
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "alias/lib.h"
#include "sort.h"
#include "lib.h"
#include "alias.h"
#include "gui.h"

#define RSORT(num) ((sort_alias & SORT_REVERSE) ? -num : num)

static short sort_alias = 0;

/**
 * alias_sort_name - Compare two Aliases by their short names - Implements ::sort_t
 *
 * @note Non-visible Aliases are sorted to the end
 */
static int alias_sort_name(const void *a, const void *b)
{
  const struct AliasView *av_a = a;
  const struct AliasView *av_b = b;

  if (av_a->is_visible != av_b->is_visible)
    return av_a->is_visible ? -1 : 1;

  if (!av_a->is_visible)
    return 0;

  int r = mutt_str_coll(av_a->alias->name, av_b->alias->name);

  return RSORT(r);
}

/**
 * alias_sort_address - Compare two Aliases by their Addresses - Implements ::sort_t
 *
 * @note Non-visible Aliases are sorted to the end
 */
static int alias_sort_address(const void *a, const void *b)
{
  const struct AliasView *av_a = a;
  const struct AliasView *av_b = b;

  const struct AddressList *al_a = &av_a->alias->addr;
  const struct AddressList *al_b = &av_b->alias->addr;

  if (av_a->is_visible != av_b->is_visible)
    return av_a->is_visible ? -1 : 1;

  if (!av_a->is_visible)
    return 0;

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
    if (addr_a && addr_a->personal)
    {
      if (addr_b && addr_b->personal)
        r = mutt_str_coll(addr_a->personal, addr_b->personal);
      else
        r = 1;
    }
    else if (addr_b && addr_b->personal)
      r = -1;
    else if (addr_a && addr_b)
      r = mutt_str_coll(addr_a->mailbox, addr_b->mailbox);
    else
      r = 0;
  }

  return RSORT(r);
}

/**
 * alias_sort_unsort - Compare two Aliases by their original configuration position - Implements ::sort_t
 *
 * @note Non-visible Aliases are sorted to the end
 */
static int alias_sort_unsort(const void *a, const void *b)
{
  const struct AliasView *av_a = a;
  const struct AliasView *av_b = b;

  if (av_a->is_visible != av_b->is_visible)
    return av_a->is_visible ? -1 : 1;

  if (!av_a->is_visible)
    return 0;

  int r = (av_a->orig_seq - av_b->orig_seq);

  return RSORT(r);
}

/**
 * alias_get_sort_function - Sorting function decision logic
 * @param sort Sort method, e.g. #SORT_ALIAS
 */
static sort_t alias_get_sort_function(short sort)
{
  switch ((sort & SORT_MASK))
  {
    case SORT_ALIAS:
      return alias_sort_name;
    case SORT_ADDRESS:
      return alias_sort_address;
    case SORT_ORDER:
      return alias_sort_unsort;
    default:
      return alias_sort_name;
  }
}

/**
 * alias_array_sort - Sort and reindex an AliasViewArray
 * @param ava Array of Aliases
 * @param sub Config items
 */
void alias_array_sort(struct AliasViewArray *ava, const struct ConfigSubset *sub)
{
  if (!ava || ARRAY_EMPTY(ava))
    return;

  sort_alias = cs_subset_sort(sub, "sort_alias");
  ARRAY_SORT(ava, alias_get_sort_function(sort_alias));

  struct AliasView *avp = NULL;
  ARRAY_FOREACH(avp, ava)
  {
    avp->num = ARRAY_FOREACH_IDX;
  }
}
