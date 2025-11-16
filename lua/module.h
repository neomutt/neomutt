/**
 * @file
 * Lua Module
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

#ifndef MUTT_LUA_MODULE_H
#define MUTT_LUA_MODULE_H

#include <lua.h>
#include "core/lib.h"

/**
 * struct LuaModule - Wrapper for the Lua Variables
 */
struct LuaModule
{
  lua_State             *lua_state;      ///< Lua State
  struct LuaLogFile     *log_file;       ///< Log File
};

lua_State *lua_init_state(void);

/**
 * lua_get_state - XXX
 */
static inline lua_State *lua_get_state(void)
{
  if (NeoMutt && NeoMutt->lua_module)
    return NeoMutt->lua_module->lua_state;

  return NULL;
}

/**
 * lua_get_log_file - XXX
 */
static inline struct LuaLogFile *lua_get_log_file(void)
{
  if (NeoMutt && NeoMutt->lua_module)
    return NeoMutt->lua_module->log_file;

  return NULL;
}

#endif /* MUTT_LUA_MODULE_H */
