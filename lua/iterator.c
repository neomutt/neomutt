/**
 * @file
 * Lua EmailArray
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
 * @page lua_email_array Lua EmailArray
 *
 * Lua EmailArray
 */

#include "config.h"
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "iterator.h"
#include "config2.h"
#include "global.h"
#include "helpers.h"
#include "logging.h"
#include "module.h"

/**
 * emailarray_new - XXX
 */
struct EmailArray *emailarray_new(lua_State *l)
{
  struct EmailArray *ea = lua_newuserdata(l, sizeof(struct EmailArray));
  ARRAY_INIT(ea);
  luaL_getmetatable(l, "EmailArray");
  lua_setmetatable(l, -2);

  return ea;
}

/**
 * lua_emailarray_cb_gc - XXX
 */
static int lua_emailarray_cb_gc(lua_State *l)
{
  lua_debug(LL_DEBUG1, "lua_emailarray_cb_gc: garbage collection: %d\n", lua_gettop(l));

  struct EmailArray *ea = luaL_checkudata(l, 1, "EmailArray");

  ARRAY_FREE(ea);
  lua_debug(LL_DEBUG1, "EmailArray %p\n", ea);

  return 1;
}

/**
 * lua_emailarray_cb_next - XXX
 */
static int lua_emailarray_cb_next(lua_State *l)
{
  lua_debug(LL_DEBUG1, "lua_emailarray_cb_next\n");
  // lua_debug(LL_DEBUG1, "stack: %d\n", lua_gettop(l));
  // lua_dump_stack(l);

  struct EmailArray *ea = luaL_checkudata(l, 1, "EmailArray");
  // lua_debug(LL_DEBUG1, "array %d\n", ARRAY_SIZE(ea));

  int index = lua_tointeger(l, lua_upvalueindex(2));
  lua_debug(LL_DEBUG1, "index: %d\n", index);

  // lua_pop(l, 3);

  if (index >= ARRAY_SIZE(ea))
  {
    lua_pushinteger(l, 0);
    lua_replace(l, lua_upvalueindex(2));
    return 0; // nil
  }

  struct Email **ud = lua_newuserdata(l, sizeof(struct Email *));
  *ud = *ARRAY_GET(ea, index);
  luaL_getmetatable(l, "Email");
  lua_setmetatable(l, -2);

  index++;
  lua_pushinteger(l, index);
  lua_replace(l, lua_upvalueindex(2));

  return 1;
}

/**
 * lua_emailarray_cb_call - XXX
 */
static int lua_emailarray_cb_call(lua_State *l)
{
  lua_debug(LL_DEBUG1, "lua_emailarray_cb_call\n");
  return lua_emailarray_cb_next(l);
}

/**
 * lua_emailarray_cb_pairs_next - XXX
 */
static int lua_emailarray_cb_pairs_next(lua_State *l)
{
  lua_debug(LL_DEBUG1, "lua_emailarray_cb_pairs_next\n");
  // lua_dump_stack(l);
  /* state = emailarray userdata */
  struct EmailArray *ea = luaL_checkudata(l, 1, "EmailArray");

  int index = lua_tointeger(l, lua_upvalueindex(2));
  lua_debug(LL_DEBUG1, "index: %d\n", index);

  // lua_pop(l, 3);

  if (index >= ARRAY_SIZE(ea))
  {
    lua_pushinteger(l, 0);
    lua_replace(l, lua_upvalueindex(2));
    return 0; // nil
  }

  lua_pushinteger(l, index + 1);

  struct Email **ud = lua_newuserdata(l, sizeof(struct Email *));
  *ud = *ARRAY_GET(ea, index);
  luaL_getmetatable(l, "Email");
  lua_setmetatable(l, -2);

  index++;
  lua_pushinteger(l, index);
  lua_replace(l, lua_upvalueindex(2));

  return 2;
}

/**
 * lua_emailarray_cb_pairs - XXX
 */
static int lua_emailarray_cb_pairs(lua_State *l)
{
  lua_debug(LL_DEBUG1, "lua_emailarray_cb_pairs\n");
  /* arg #1 = emailarray userdata */
  luaL_checkudata(l, 1, "EmailArray");

  lua_pushcfunction(l, lua_emailarray_cb_pairs_next); /* iteration function */
  lua_pushvalue(l, 1);                                /* state */
  lua_pushnil(l);                                     /* initial key */

  return 3;
}

/**
 * lua_emailarray_cb_tostring - XXX - Implements ::lua_callback_t
 */
static int lua_emailarray_cb_tostring(lua_State *l)
{
  // lua_debug(LL_DEBUG1, "lua_emailarray_cb_tostring\n");
  struct EmailArray *ea = *(struct EmailArray **) luaL_checkudata(l, 1, "EmailArray");
  if (!ea)
    return 0;

  struct Buffer *buf = buf_pool_get();
  buf_printf(buf, "EA:%lX", (long) ea);
  lua_pushstring(l, buf_string(buf));
  buf_pool_release(&buf);

  return 1;
}

/**
 * lua_emailarray_cb_index - Callback for Lua Metamethod Index::__index() - Implements ::lua_callback_t
 */
static int lua_emailarray_cb_index(lua_State *l)
{
  lua_debug(LL_DEBUG1, "lua_emailarray_cb_index\n");
  bool found = lua_index_lookup(l, "EmailArray");
  if (found)
    return 1;

  // lua_dump_stack(l);

  struct EmailArray *ea = luaL_checkudata(l, 1, "EmailArray");

  int index = lua_tointeger(l, 2);
  lua_debug(LL_DEBUG1, "index: %d\n", index);

  if ((index < 1) || (index > ARRAY_SIZE(ea)))
    return 0; // nil

  struct Email **ud = lua_newuserdata(l, sizeof(struct Email *));
  *ud = *ARRAY_GET(ea, index - 1);
  luaL_getmetatable(l, "Email");
  lua_setmetatable(l, -2);

  return 1;
}

/**
 * EmailArrayMethods - EmailArray Class Methods
 */
static const struct luaL_Reg EmailArrayMethods[] = {
  // clang-format off
  { "__call",     lua_emailarray_cb_call     },
  { "__gc",       lua_emailarray_cb_gc       },
  { "__index",    lua_emailarray_cb_index    },
  { "__pairs",    lua_emailarray_cb_pairs    },
  { "__tostring", lua_emailarray_cb_tostring },
  { "next",       lua_emailarray_cb_next     },
  { NULL, NULL },
  // clang-format on
};

/**
 * lua_emailarray_class - Declare the EmailArray Class
 */
void lua_emailarray_class(lua_State *l)
{
  luaL_newmetatable(l, "EmailArray");
  luaL_setfuncs(l, EmailArrayMethods, 0);
  lua_pop(l, 1);
}
