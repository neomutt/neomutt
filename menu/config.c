/**
 * @file
 * Config used by libmenu
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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
 * @page menu_config Config used by libmenu
 *
 * Config used by libmenu
 */

#include "config.h"
#include <stddef.h>
#include <config/lib.h>
#include <stdbool.h>

/**
 * MenuVars - Config definitions for the Menu library
 */
static struct ConfigDef MenuVars[] = {
  // clang-format off
  { "menu_context", DT_NUMBER|DT_NOT_NEGATIVE, 0, 0, NULL,
    "Number of lines of overlap when changing pages in the index"
  },
  { "menu_move_off", DT_BOOL, true, 0, NULL,
    "Allow the last menu item to move off the bottom of the screen"
  },
  { "menu_scroll", DT_BOOL, false, 0, NULL,
    "Scroll the menu/index by one line, rather than a page"
  },
  { NULL },
  // clang-format on
};

/**
 * config_init_menu - Register menu config variables - Implements ::module_init_config_t - @ingroup cfg_module_api
 */
bool config_init_menu(struct ConfigSet *cs)
{
  return cs_register_variables(cs, MenuVars, 0);
}
