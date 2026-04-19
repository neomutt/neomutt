/**
 * @file
 * Definition of the Menu Module
 *
 * @authors
 * Copyright (C) 2025-2026 Richard Russon <rich@flatcap.org>
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
 * @page menu_module Definition of the Menu Module
 *
 * Definition of the Menu Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "lib.h"
#include "module_data.h"
#include "type.h"

extern struct ConfigDef MenuVars[];

/**
 * menu_init - Initialise a Module - Implements Module::init()
 */
static bool menu_init(struct NeoMutt *n)
{
  struct MenuModuleData *mod_data = MUTT_MEM_CALLOC(1, struct MenuModuleData);
  neomutt_set_module_data(n, MODULE_ID_MENU, mod_data);

  mod_data->notify = notify_new();
  notify_set_parent(mod_data->notify, n->notify);

  return true;
}

/**
 * menu_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool menu_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  return cs_register_variables(cs, MenuVars);
}

/**
 * menu_gui_init - Initialise the GUI - Implements Module::gui_init()
 */
static bool menu_gui_init(struct NeoMutt *n)
{
  struct MenuModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_MENU);
  menu_init2(mod_data->search_buffers);
  return true;
}

/**
 * menu_gui_cleanup - Clean up the GUI - Implements Module::gui_cleanup()
 */
static void menu_gui_cleanup(struct NeoMutt *n)
{
  struct MenuModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_MENU);
  ASSERT(mod_data);

  for (int i = 0; i < MENU_MAX; i++)
    FREE(&mod_data->search_buffers[i]);
}

/**
 * menu_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool menu_cleanup(struct NeoMutt *n)
{
  struct MenuModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_MENU);
  ASSERT(mod_data);

  notify_free(&mod_data->notify);

  FREE(&mod_data);
  return true;
}

/**
 * ModuleMenu - Module for the Menu library
 */
const struct Module ModuleMenu = {
  MODULE_ID_MENU,
  "menu",
  menu_init,
  NULL, // config_define_types
  menu_config_define_variables,
  NULL, // commands_register
  menu_gui_init,
  menu_gui_cleanup,
  menu_cleanup,
};
