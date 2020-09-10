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
 * alias_sort_name - Compare two Aliases by their short names - Implements ::sort_t
 *
 * @note Non-visible Aliases are sorted to the end
 */
int alias_sort_name(const void *a, const void *b)
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
int alias_sort_address(const void *a, const void *b)
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
int alias_sort_unsort(const void *a, const void *b)
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

  alias_array_sort(&mdata->ava);

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
 * alias_array_alias_add - Add an Alias to the AliasViewArray
 * @param ava Array of Aliases
 * @param alias Alias to add
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
 *
 * @note Call alias_array_sort() to sort and reindex the AliasViewArray
 */
int alias_array_alias_delete(struct AliasViewArray *ava, struct Alias *alias)
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
 * alias_array_sort - Sort and reindex an AliasViewArray
 * @param ava Array of Aliases
 */
void alias_array_sort(struct AliasViewArray *ava)
{
  if (!ava || ARRAY_EMPTY(ava))
    return;

  ARRAY_SORT(ava, alias_get_sort_function(C_SortAlias));

  struct AliasView *avp = NULL;
  int i = 0;
  ARRAY_FOREACH(avp, ava)
  {
    avp->num = i++;
  }
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

/**
 * menu_create_alias_title - Create a title string for the Menu
 * @param menu_name Menu name
 * @param limit     Limit being applied
 *
 * @note Caller must free the returned string
 */
char *menu_create_alias_title(char *menu_name, char *limit)
{
  if (limit)
  {
    char *tmp_str = NULL;
    char *new_title = NULL;

    mutt_str_asprintf(&tmp_str, _("Limit: %s"), limit);
    mutt_str_asprintf(&new_title, "%s - %s", menu_name, tmp_str);

    FREE(&tmp_str);

    return new_title;
  }
  else
  {
    return strdup(menu_name);
  }
}
