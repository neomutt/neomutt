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
#include "mutt_menu.h"
#include "sort.h"

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
  const struct AliasView *av_a = a;
  const struct AliasView *av_b = b;

  int r = mutt_str_coll(av_a->alias->name, av_b->alias->name);

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
  const struct AddressList *al_a = &((struct AliasView const *) a)->alias->addr;
  const struct AddressList *al_b = &((struct AliasView const *) b)->alias->addr;

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
 */
int alias_sort_unsort(const void *a, const void *b)
{
  const struct AliasView *av_a = a;
  const struct AliasView *av_b = b;

  int r = (av_a->orig_seq - av_b->orig_seq);

  return RSORT(r);
}

/**
 * alias_config_observer - Listen for `sort_alias` configuration changes and reorders menu items accordingly
 */
int alias_config_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;

  struct EventConfig *ec = nc->event_data;

  if (!mutt_str_equal(ec->name, "sort_alias"))
    return 0;

  struct AliasMenuData *mdata = nc->global_data;

  menu_data_sort(mdata);

  return 0;
}

/**
 * alias_color_observer - Listen for color configuration changes and refreshes the menu
 */
int alias_color_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_COLOR) || !nc->event_data || !nc->global_data)
    return -1;

  struct Menu *menu = nc->global_data;
  menu->redraw = REDRAW_FULL;

  return 0;
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

  FREE(ptr);
}

/**
 * alias_get_sort_function - Sorting function decision logic
 * @param sort Sort method, e.g. #SORT_ALIAS
 */
sort_t alias_get_sort_function(short sort)
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
 * menu_data_clear - Empty an AliasMenuData
 * @param mdata  Menu data holding Aliases
 *
 * Free the AliasViews but not the Aliases.
 */
void menu_data_clear(struct AliasMenuData *mdata)
{
  if (!mdata)
    return;

  struct AliasView *avp = NULL;
  ARRAY_FOREACH(avp, mdata)
  {
    alias_view_free(&avp);
  }
  ARRAY_SHRINK(mdata, ARRAY_SIZE(mdata));
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
  menu_data_clear(mdata);
  ARRAY_FREE(mdata);
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

  struct AliasView av = {
    .num = 0,
    .orig_seq = ARRAY_SIZE(mdata),
    .is_tagged = false,
    .is_deleted = false,
    .alias = alias,
  };
  ARRAY_ADD(mdata, av);
  return ARRAY_SIZE(mdata);
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

  struct AliasView *avp = NULL;
  ARRAY_FOREACH(avp, mdata)
  {
    if (avp->alias != alias)
      continue;

    ARRAY_REMOVE(mdata, avp);
    break;
  }

  return ARRAY_SIZE(mdata);
}

/**
 * menu_data_sort - Sort and reindex an AliasMenuData
 * @param mdata Menu data holding Aliases
 */
void menu_data_sort(struct AliasMenuData *mdata)
{
  ARRAY_SORT(mdata, alias_get_sort_function(C_SortAlias));

  struct AliasView *avp = NULL;
  int i = 0;
  ARRAY_FOREACH(avp, mdata)
  {
    avp->num = i++;
  }
}
