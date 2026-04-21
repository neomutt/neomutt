/**
 * @file
 * Definition of the Sidebar Module
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
 * @page sidebar_module Definition of the Sidebar Module
 *
 * Definition of the Sidebar Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/module_data.h"
#include "lib.h"
#include "module_data.h"

extern struct ConfigDef SidebarVars[];

extern const struct Command SbCommands[];

/**
 * sidebar_init - Initialise a Module - Implements Module::init()
 */
static bool sidebar_init(struct NeoMutt *n)
{
  struct SidebarModuleData *mod_data = MUTT_MEM_CALLOC(1, struct SidebarModuleData);
  STAILQ_INIT(&mod_data->sidebar_pinned);
  neomutt_set_module_data(n, MODULE_ID_SIDEBAR, mod_data);

  mod_data->notify = notify_new();
  notify_set_parent(mod_data->notify, n->notify);

  return true;
}

/**
 * sidebar_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool sidebar_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  return cs_register_variables(cs, SidebarVars);
}

/**
 * sidebar_commands_register - Register NeoMutt Commands - Implements Module::commands_register()
 */
static bool sidebar_commands_register(struct NeoMutt *n, struct CommandArray *ca)
{
  return commands_register(ca, SbCommands);
}

/**
 * sidebar_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool sidebar_cleanup(struct NeoMutt *n, void *data)
{
  struct SidebarModuleData *mod_data = data;

  notify_free(&mod_data->notify);

  mutt_list_free(&mod_data->sidebar_pinned);
  FREE(&mod_data);
  return true;
}

/**
 * sidebar_gui_init - Initialise the GUI - Implements Module::gui_init()
 */
static bool sidebar_gui_init(struct NeoMutt *n)
{
  struct GuiModuleData *gui_data = neomutt_get_module_data(n, MODULE_ID_GUI);
  sb_init(gui_data ? gui_data->all_dialogs_window : NULL);
  return true;
}

/**
 * sidebar_gui_cleanup - Clean up the GUI - Implements Module::gui_cleanup()
 */
static void sidebar_gui_cleanup(struct NeoMutt *n)
{
  struct SidebarModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_SIDEBAR);
  struct GuiModuleData *gui_data = neomutt_get_module_data(n, MODULE_ID_GUI);
  sb_cleanup(&mod_data->sidebar_pinned, (gui_data ? gui_data->all_dialogs_window : NULL));
}

/**
 * ModuleSidebar - Module for the Sidebar library
 */
const struct Module ModuleSidebar = {
  MODULE_ID_SIDEBAR,
  "sidebar",
  sidebar_init,
  NULL, // config_define_types
  sidebar_config_define_variables,
  sidebar_commands_register,
  sidebar_gui_init,
  sidebar_gui_cleanup,
  sidebar_cleanup,
};
