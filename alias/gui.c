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
  const struct AliasView *av_a = *(struct AliasView const *const *) a;
  const struct AliasView *av_b = *(struct AliasView const *const *) b;

  int r = mutt_istr_cmp(av_a->alias->name, av_b->alias->name);

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
  const struct AddressList *al_a =
      &(*(struct AliasView const *const *) a)->alias->addr;
  const struct AddressList *al_b =
      &(*(struct AliasView const *const *) b)->alias->addr;

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
        r = mutt_istr_cmp(addr_a->personal, addr_b->personal);
      else
        r = 1;
    }
    else if (addr_b->personal)
      r = -1;
    else
      r = mutt_istr_cmp(addr_a->mailbox, addr_b->mailbox);
  }
  return RSORT(r);
}

/**
 * alias_view_free - Free an AliasView
 * @param[out] ptr AliasView to free
 *
 * @note The actual Alias isn't owned by the AliasView, so it isn't freed.
 */
static void alias_view_free(struct AliasView **ptr)
{
  if (!ptr || !*ptr)
    return;

  // struct AliasView *av = *ptr;

  FREE(ptr);
}

/**
 * alias_view_new - Create a new AliasView
 * @retval ptr Newly allocated AliasView
 *
 * A GUI wrapper around an Alias
 */
static struct AliasView *alias_view_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct AliasView));
}

/**
 * menu_data_clear - Empty an AliasMenuData
 * @param mdata  Menu data holding Aliases
 *
 * Free the AliasViews but not the Aliases.
 */
void menu_data_clear(struct AliasMenuData *mdata)
{
  if (!mdata)
    return;

  for (int i = 0; i < mdata->num_views; i++)
    alias_view_free(&mdata->av[i]);

  mdata->num_views = 0;
}

/**
 * menu_data_free - Free an AliasMenuData
 * @param[out] ptr AliasMenuData to free
 */
void menu_data_free(struct AliasMenuData **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct AliasMenuData *mdata = *ptr;

  for (int i = 0; i < mdata->num_views; i++)
    alias_view_free(&mdata->av[i]);

  FREE(&mdata->av);
  FREE(ptr);
}

/**
 * menu_data_new - Create a new AliasMenuData
 * @retval ptr Newly allocated AliasMenuData
 *
 * All the GUI data required to maintain the Menu.
 */
struct AliasMenuData *menu_data_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct AliasMenuData));
}

/**
 * menu_data_alias_add - Add an Alias to the AliasMenuData
 * @param mdata Menu data holding Aliases
 * @param alias Alias to add
 *
 * @note The Alias is wrapped in an AliasView
 * @note Call menu_data_sort() to sort and reindex the AliasMenuData
 */
int menu_data_alias_add(struct AliasMenuData *mdata, struct Alias *alias)
{
  if (!mdata || !alias)
    return -1;

  const int chunk_size = 256;

  if (mdata->num_views >= mdata->max_views)
  {
    mdata->max_views += chunk_size;
    mutt_mem_realloc(&mdata->av, mdata->max_views * sizeof(struct AliasView *));

    memset(&mdata->av[mdata->max_views - chunk_size], 0,
           chunk_size * sizeof(struct AliasView *));
  }

  struct AliasView *av = alias_view_new();
  av->alias = alias;

  mdata->av[mdata->num_views] = av;
  mdata->num_views++;

  return mdata->num_views;
}

/**
 * menu_data_alias_delete - Delete an Alias from the AliasMenuData
 * @param mdata Menu data holding Aliases
 * @param alias Alias to remove
 *
 * @note Call menu_data_sort() to sort and reindex the AliasMenuData
 */
int menu_data_alias_delete(struct AliasMenuData *mdata, struct Alias *alias)
{
  if (!mdata || !alias)
    return -1;

  for (int i = 0; i < mdata->num_views; i++)
  {
    if (mdata->av[i]->alias != alias)
      continue;

    alias_view_free(&mdata->av[i]);

    int move = mdata->num_views - i - 1;
    if (move == 0)
      break;

    memmove(&mdata->av[i], &mdata->av[i + 1], move * sizeof(struct AliasView *));
    mdata->av[mdata->num_views - 1] = NULL;
    break;
  }

  mdata->num_views--;
  return mdata->num_views;
}

/**
 * menu_data_sort - Sort and reindex an AliasMenuData
 * @param mdata Menu data holding Aliases
 */
void menu_data_sort(struct AliasMenuData *mdata)
{
  if ((C_SortAlias & SORT_MASK) != SORT_ORDER)
  {
    qsort(mdata->av, mdata->num_views, sizeof(struct AliasView *),
          ((C_SortAlias & SORT_MASK) == SORT_ADDRESS) ? alias_sort_address : alias_sort_name);
  }

  for (int i = 0; i < mdata->num_views; i++)
    mdata->av[i]->num = i;
}
