/**
 * @file
 * Lua Config Wrapper
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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
 * @page lua_config Lua Config Wrapper
 *
 * Lua Config Wrapper
 */

#include "config.h"
#include <lauxlib.h>
#include <lua.h>
#include <stdbool.h>
#include <stddef.h>
#include "config/lib.h"

/**
 * LuaVars - Config definitions for the lua library
 */
static struct ConfigDef LuaVars[] = {
  // clang-format off
  { "lua_log_file", DT_PATH|D_PATH_FILE, 0, 0, NULL,
    "Save new lua logs to this file"
  },
  { NULL },
  // clang-format on
};

/**
 * config_init_lua - Register lua config variables - Implements ::module_init_config_t - @ingroup cfg_module_api
 */
bool config_init_lua(struct ConfigSet *cs)
{
  return cs_register_variables(cs, LuaVars);
}

/**
 * register_config - XXX
 */
void register_config(lua_State *l)
{
  luaL_newmetatable(l, "Config");
  lua_newtable(l);
  int table = lua_gettop(l);

  lua_pushstring(l, "number");
  lua_pushinteger(l, 42);
  lua_settable(l, table);

  lua_setfield(l, -2, "__index");
}
