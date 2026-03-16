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
#include "module_data.h"

extern struct ConfigDef SidebarVars[];

extern const struct Command SbCommands[];

/**
 * sidebar_init - Initialise a Module - Implements Module::init()
 */
static bool sidebar_init(struct NeoMutt *n)
{
  // struct SidebarModuleData *md = MUTT_MEM_CALLOC(1, struct SidebarModuleData);
  // neomutt_set_module_data(n, MODULE_ID_SIDEBAR, md);

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
static bool sidebar_cleanup(struct NeoMutt *n)
{
  // struct SidebarModuleData *md = neomutt_get_module_data(n, MODULE_ID_SIDEBAR);
  // ASSERT(md);

  // FREE(&md);
  return true;
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
  NULL, // gui_init
  NULL, // gui_cleanup
  sidebar_cleanup,
};
