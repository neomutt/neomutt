/**
 * @file
 * Lua NeoMutt Wrapper
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
 * @page lua_neomutt Lua NeoMutt Wrapper
 *
 * Lua NeoMutt Wrapper
 */

#include "config.h"
#include <lauxlib.h>
#include <lua.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "neomutt.h"
#include "helpers.h"
#include "logging.h"

/**
 * lua_neomutt_cb_tostring - Turn a NeoMutt into a string - Implements ::lua_callback_t
 */
static int lua_neomutt_cb_tostring(lua_State *l)
{
  struct NeoMutt *n = *(struct NeoMutt **) luaL_checkudata(l, 1, "NeoMutt");
  if (!n)
    return 0;

  struct Buffer *buf = buf_pool_get();
  buf_printf(buf, "N:%lX", (long) n);
  lua_pushstring(l, buf_string(buf));
  buf_pool_release(&buf);

  return 1;
}

/**
 * lua_neomutt_cb_index - Callback for Lua Metamethod Index::__index() - Implements ::lua_callback_t
 */
static int lua_neomutt_cb_index(lua_State *l)
{
  lua_debug(LL_DEBUG1, "lua_neomutt_cb_index\n");
  bool found = lua_index_lookup(l, "NeoMutt");
  if (found)
    return 1;

  return 0;
}

/**
 * lua_neomutt_cb_accounts_iter - Iterator for the Accounts Array
 */
static int lua_neomutt_cb_accounts_iter(lua_State *l)
{
  struct NeoMutt *n = lua_touserdata(l, lua_upvalueindex(1));

  int i = lua_tointeger(l, lua_upvalueindex(2));

  struct Account **ap = ARRAY_GET(&n->accounts, i);
  if (!ap)
    return 0;

  lua_pushinteger(l, i + 1);
  lua_replace(l, lua_upvalueindex(2));
  LUA_PUSH_OBJECT(l, Account, *ap);

  return 1;
}

/**
 * lua_neomutt_cb_accounts - Get the Accounts in the NeoMutt
 */
static int lua_neomutt_cb_accounts(lua_State *l)
{
  struct NeoMutt *n = *(struct NeoMutt **) luaL_checkudata(l, 1, "NeoMutt");
  lua_pushlightuserdata(l, n);
  lua_pushinteger(l, 0);
  lua_pushcclosure(l, lua_neomutt_cb_accounts_iter, 2);

  return 1;
}

/**
 * lua_neomutt_cb_num_accounts - Count the number of Accounts
 */
static int lua_neomutt_cb_num_accounts(lua_State *l)
{
  struct NeoMutt *n = *(struct NeoMutt **) luaL_checkudata(l, 1, "NeoMutt");
  lua_pushinteger(l, ARRAY_SIZE(&n->accounts));

  return 1;
}

/**
 * NeoMuttMethods - NeoMutt Class Methods
 */
static const struct luaL_Reg NeoMuttMethods[] = {
  // clang-format off
  { "__index",      lua_neomutt_cb_index        },
  { "__tostring",   lua_neomutt_cb_tostring     },
  { "accounts",     lua_neomutt_cb_accounts     },
  { "num_accounts", lua_neomutt_cb_num_accounts },
  { NULL, NULL },
  // clang-format on
};

/**
 * lua_neomutt_class - Declare the NeoMutt Class
 */
void lua_neomutt_class(lua_State *l)
{
  luaL_newmetatable(l, "NeoMutt");
  luaL_setfuncs(l, NeoMuttMethods, 0);
  lua_pop(l, 1);
}

/**
 * lua_neomutt_init - Initialise the Lua `neomutt` object
 */
void lua_neomutt_init(lua_State *l)
{
  struct NeoMutt **pptr = lua_newuserdata(l, sizeof(struct NeoMutt *));
  *pptr = NeoMutt;
  luaL_getmetatable(l, "NeoMutt");
  lua_setmetatable(l, -2);

  lua_setglobal(l, "neomutt");
}
