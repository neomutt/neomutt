/**
 * @file
 * Lua Helpers
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

#ifndef MUTT_LUA_HELPER_H
#define MUTT_LUA_HELPER_H

#include <stdbool.h>
#include <lua.h>

void        lua_dump_stack  (lua_State *l);
const char *lua_type_name   (int type);

#define LUA_PUSH_OBJECT(L, TYPENAME, OBJ)                                      \
  do                                                                           \
  {                                                                            \
    struct TYPENAME **ud = lua_newuserdata(L, sizeof(struct TYPENAME *));      \
    *ud = OBJ;                                                                 \
    luaL_getmetatable(L, #TYPENAME);                                           \
    lua_setmetatable(L, -2);                                                   \
  } while (0)

/**
 * @defgroup lua_callback_api Lua Callback Function
 *
 * lua_callback_t - Lua Callback Function
 * @param[in] l Lua State
 * @retval num Number of objects put on the Lua Stack
 *
 * A C function called from Lua
 */
typedef int (*lua_callback_t)(lua_State *l);

#endif /* MUTT_LUA_HELPER_H */
