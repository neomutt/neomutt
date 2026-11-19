/**
 * @file
 * Lua Module
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

#ifndef MUTT_LUA_MODULE_H
#define MUTT_LUA_MODULE_H

#include <lua.h>
#include <stddef.h>
#include "core/lib.h"
#include "module_data.h"

struct LuaConsoleInfo;

/**
 * lua_get_state - Get the Lua State
 * @retval ptr  Lua State
 * @retval NULL No state available
 */
static inline lua_State *lua_get_state(void)
{
  struct LuaModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_LUA);
  return mod_data ? mod_data->lua_state : NULL;
}

/**
 * lua_get_log_file - Get the Lua Log File
 * @retval ptr  Lua Log File
 * @retval NULL No log file available
 */
static inline struct LuaLogFile *lua_get_log_file(void)
{
  struct LuaModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_LUA);
  return mod_data ? mod_data->log_file : NULL;
}

/**
 * lua_get_console - Get the Lua Console
 * @retval ptr  Lua Console Info
 * @retval NULL No console available
 */
static inline struct LuaConsoleInfo *lua_get_console(void)
{
  struct LuaModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_LUA);
  return mod_data ? mod_data->console : NULL;
}

/**
 * lua_set_console - Set the Lua Console
 * @param console Lua Console Info
 */
static inline void lua_set_console(struct LuaConsoleInfo *console)
{
  struct LuaModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_LUA);
  if (mod_data)
    mod_data->console = console;
}

#endif /* MUTT_LUA_MODULE_H */
