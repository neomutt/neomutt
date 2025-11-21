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

/**
 * @page lua_helpers Lua Helpers
 *
 * Lua Helpers
 */

#include "config.h"
#include <lauxlib.h>
#include <lua.h>
#include "core/lib.h"

/**
 * lua_type_name - Turn a Lua Type into a String
 * @param type e.g. LUA_TNUMBER
 * @retval str Type description
 */
const char *lua_type_name(int type)
{
  switch (type)
  {
    case LUA_TBOOLEAN:
      return "Boolean";
    case LUA_TFUNCTION:
      return "Function";
    case LUA_TLIGHTUSERDATA:
      return "LightUserData";
    case LUA_TNIL:
      return "Nil";
    case LUA_TNUMBER:
      return "Number";
    case LUA_TSTRING:
      return "String";
    case LUA_TTABLE:
      return "Table";
    case LUA_TTHREAD:
      return "Thread";
    case LUA_TUSERDATA:
      return "UserData";
    default:
      return "UNKNOWN";
  }
}

/**
 * lua_dump_stack - Dump the Lua Stack
 * @param l Lua State
 *
 * Dump the contents of the Lua Stack.
 * Use `tostring()` to serialise the objects.
 */
void lua_dump_stack(lua_State *l)
{
  int top = lua_gettop(l);
  mutt_debug(LL_DEBUG1, "Stack: %d\n", top);
  for (int i = 1; i <= top; i++)
  {
    // mutt_debug(LL_DEBUG1, "lua_index_cb_index: stack: %s(%d)\n", name_lua_type(lua_type(l, i)), lua_type(l, i));
    if (lua_type(l, i) == LUA_TUSERDATA)
    {
      void *ptr = lua_touserdata(l, 1);

      // mutt_debug(LL_DEBUG1, "STACK1: %d\n", lua_gettop(l));
      int rc = luaL_getmetafield(l, 1, "__name");
      // mutt_debug(LL_DEBUG1, "STACK2: %d\n", lua_gettop(l));
      if (rc == LUA_TNIL)
      {
        mutt_debug(LL_DEBUG1, "        userdata: %p\n", ptr);
      }
      else
      {
        mutt_debug(LL_DEBUG1, "        userdata: %p - %s\n", ptr, lua_tostring(l, -1));
      }
      lua_pop(l, 1);
      // mutt_debug(LL_DEBUG1, "STACK3: %d\n", lua_gettop(l));
    }
    else if (lua_type(l, i) == LUA_TSTRING)
    {
      mutt_debug(LL_DEBUG1, "        string: %s\n", lua_tostring(l, i));
    }
    else if (lua_type(l, i) == LUA_TNUMBER)
    {
      mutt_debug(LL_DEBUG1, "        number: %lld\n", lua_tointeger(l, i));
    }
    else
    {
      mutt_debug(LL_DEBUG1, "        %s\n", lua_type_name(lua_type(l, i)));
    }
  }
}
