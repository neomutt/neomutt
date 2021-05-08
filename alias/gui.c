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
#include "config/lib.h"
#include "gui.h"
#include "lib.h"
#include "mutt_menu.h"

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

  alias_array_sort(&mdata->ava, mdata->sub);

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
  menu_queue_redraw(menu, REDRAW_FULL);

  return 0;
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
