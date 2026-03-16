/**
 * @file
 * Definition of the Lua Module
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
 * @page lua_module Definition of the Lua Module
 *
 * Definition of the Lua Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "module_data.h"

extern const struct Command LuaCommands[];

/**
 * lua_init - Initialise a Module - Implements Module::init()
 */
static bool lua_init(struct NeoMutt *n)
{
  // struct LuaModuleData *md = MUTT_MEM_CALLOC(1, struct LuaModuleData);
  // neomutt_set_module_data(n, MODULE_ID_LUA, md);

  return true;
}

/**
 * lua_commands_register - Register NeoMutt Commands - Implements Module::commands_register()
 */
static bool lua_commands_register(struct NeoMutt *n, struct CommandArray *ca)
{
  return commands_register(ca, LuaCommands);
}

/**
 * lua_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool lua_cleanup(struct NeoMutt *n)
{
  // struct LuaModuleData *md = neomutt_get_module_data(n, MODULE_ID_LUA);
  // ASSERT(md);

  // FREE(&md);
  return true;
}

/**
 * ModuleLua - Module for the Lua library
 */
const struct Module ModuleLua = {
  MODULE_ID_LUA,
  "lua",
  lua_init,
  NULL, // config_define_types
  NULL, // config_define_variables
  lua_commands_register,
  NULL, // gui_init
  NULL, // gui_cleanup
  lua_cleanup,
};
