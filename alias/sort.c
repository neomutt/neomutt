/**
 * @file
 * Address book sorting functions
 *
 * @authors
 * Copyright (C) 2020 Romeu Vieira <romeu.bizz@gmail.com>
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
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
 * @page alias_sort Alias sorting
 *
 * Address book sorting functions
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "sort.h"
#include "alias.h"
#include "gui.h"

/**
 * alias_sort_alias - Compare two Aliases by their short names - Implements ::sort_t - @ingroup sort_api
 *
 * @note Non-visible Aliases are sorted to the end
 */
static int alias_sort_alias(const void *a, const void *b, void *sdata)
{
  const struct AliasView *av_a = a;
  const struct AliasView *av_b = b;
  const bool sort_reverse = *(bool *) sdata;

  if (av_a->is_visible != av_b->is_visible)
    return av_a->is_visible ? -1 : 1;

  if (!av_a->is_visible)
    return 0;

  int rc = mutt_str_coll(av_a->alias->name, av_b->alias->name);
  return sort_reverse ? -rc : rc;
}

/**
 * alias_sort_email - Compare two Aliases by their Email Addresses - Implements ::sort_t - @ingroup sort_api
 *
 * @note Non-visible Aliases are sorted to the end
 */
static int alias_sort_email(const void *a, const void *b, void *sdata)
{
  const struct AliasView *av_a = a;
  const struct AliasView *av_b = b;
  const bool sort_reverse = *(bool *) sdata;

  const struct AddressList *al_a = &av_a->alias->addr;
  const struct AddressList *al_b = &av_b->alias->addr;

  if (av_a->is_visible != av_b->is_visible)
    return av_a->is_visible ? -1 : 1;

  if (!av_a->is_visible)
    return 0;

  int rc;
  if (al_a == al_b)
  {
    rc = 0;
  }
  else if (!al_a)
  {
    rc = -1;
  }
  else if (!al_b)
  {
    rc = 1;
  }
  else
  {
    const struct Address *addr_a = TAILQ_FIRST(al_a);
    const struct Address *addr_b = TAILQ_FIRST(al_b);
    if (addr_a && addr_a->mailbox)
    {
      if (addr_b && addr_b->mailbox)
        rc = buf_coll(addr_a->mailbox, addr_b->mailbox);
      else
        rc = 1;
    }
    else if (addr_b && addr_b->mailbox)
    {
      rc = -1;
    }
    else if (addr_a && addr_b)
    {
      rc = buf_coll(addr_a->personal, addr_b->personal);
    }
    else
    {
      rc = 0;
    }
  }

  return sort_reverse ? -rc : rc;
}

/**
 * alias_sort_name - Compare two Aliases by their Names - Implements ::sort_t - @ingroup sort_api
 *
 * @note Non-visible Aliases are sorted to the end
 */
static int alias_sort_name(const void *a, const void *b, void *sdata)
{
  const struct AliasView *av_a = a;
  const struct AliasView *av_b = b;
  const bool sort_reverse = *(bool *) sdata;

  const struct AddressList *al_a = &av_a->alias->addr;
  const struct AddressList *al_b = &av_b->alias->addr;

  if (av_a->is_visible != av_b->is_visible)
    return av_a->is_visible ? -1 : 1;

  if (!av_a->is_visible)
    return 0;

  int rc;
  if (al_a == al_b)
  {
    rc = 0;
  }
  else if (!al_a)
  {
    rc = -1;
  }
  else if (!al_b)
  {
    rc = 1;
  }
  else
  {
    const struct Address *addr_a = TAILQ_FIRST(al_a);
    const struct Address *addr_b = TAILQ_FIRST(al_b);
    if (addr_a && addr_a->personal)
    {
      if (addr_b && addr_b->personal)
        rc = buf_coll(addr_a->personal, addr_b->personal);
      else
        rc = 1;
    }
    else if (addr_b && addr_b->personal)
    {
      rc = -1;
    }
    else if (addr_a && addr_b)
    {
      rc = buf_coll(addr_a->mailbox, addr_b->mailbox);
    }
    else
    {
      rc = 0;
    }
  }

  return sort_reverse ? -rc : rc;
}

/**
 * alias_sort_unsorted - Compare two Aliases by their original configuration position - Implements ::sort_t - @ingroup sort_api
 *
 * @note Non-visible Aliases are sorted to the end
 */
static int alias_sort_unsorted(const void *a, const void *b, void *sdata)
{
  const struct AliasView *av_a = a;
  const struct AliasView *av_b = b;
  const bool sort_reverse = *(bool *) sdata;

  if (av_a->is_visible != av_b->is_visible)
    return av_a->is_visible ? -1 : 1;

  if (!av_a->is_visible)
    return 0;

  int rc = mutt_numeric_cmp(av_a->orig_seq, av_b->orig_seq);
  return sort_reverse ? -rc : rc;
}

/**
 * alias_get_sort_function - Sorting function decision logic
 * @param sort Sort method, e.g. #ALIAS_SORT_ALIAS
 */
static sort_t alias_get_sort_function(short sort)
{
  switch ((sort & SORT_MASK))
  {
    case ALIAS_SORT_ALIAS:
      return alias_sort_alias;

    case ALIAS_SORT_EMAIL:
      return alias_sort_email;

    case ALIAS_SORT_NAME:
      return alias_sort_name;

    case ALIAS_SORT_UNSORTED:
      return alias_sort_unsorted;

    default:
      return alias_sort_alias;
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

  short c_sort_alias = cs_subset_sort(sub, "alias_sort");
  if (c_sort_alias == ALIAS_SORT_ALIAS)
  {
    struct AliasView *av = ARRAY_GET(ava, 0);
    struct Alias *a = av->alias;

    if (!a->name) // We've got a Query
      c_sort_alias = ALIAS_SORT_NAME;
  }

  bool sort_reverse = (c_sort_alias & SORT_REVERSE);
  ARRAY_SORT(ava, alias_get_sort_function(c_sort_alias), &sort_reverse);

  struct AliasView *avp = NULL;
  ARRAY_FOREACH(avp, ava)
  {
    avp->num = ARRAY_FOREACH_IDX_avp;
  }
}
