/**
 * @file
 * Lua Account Wrapper
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
 * @page lua_account Lua Account Wrapper
 *
 * Lua Account Wrapper
 */

#include "config.h"
#include <lauxlib.h>
#include <lua.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "account.h"
#include "helpers.h"
#include "logging.h"

/**
 * lua_account_cb_tostring - Turn an Account into a string - Implements ::lua_callback_t
 */
static int lua_account_cb_tostring(lua_State *l)
{
  struct Account *a = *(struct Account **) luaL_checkudata(l, 1, "Account");
  if (!a)
    return 0;

  struct Buffer *buf = buf_pool_get();
  buf_printf(buf, "A:%lX", (long) a);
  lua_pushstring(l, buf_string(buf));
  buf_pool_release(&buf);

  return 1;
}

/**
 * lua_account_cb_index - Callback for Lua Metamethod Index::__index() - Implements ::lua_callback_t
 */
static int lua_account_cb_index(lua_State *l)
{
  lua_debug(LL_DEBUG1, "lua_account_cb_index\n");
  bool found = lua_index_lookup(l, "Account");
  if (found)
    return 1;

  return 0;
}

/**
 * lua_account_cb_mailboxes_iter - Iterator for the Mailboxes Array
 */
static int lua_account_cb_mailboxes_iter(lua_State *l)
{
  // lua_debug(LL_DEBUG1, "lua_account_cb_mailboxes_iter: stack: %d\n", lua_gettop(l));
  // lua_dump_stack(l);
  struct Account *a = lua_touserdata(l, lua_upvalueindex(1));

  int i = lua_tointeger(l, lua_upvalueindex(2));

  struct Mailbox **mp = ARRAY_GET(&a->mailboxes, i);
  if (!mp)
    return 0;

  lua_pushinteger(l, i + 1);
  lua_replace(l, lua_upvalueindex(2));
  LUA_PUSH_OBJECT(l, Mailbox, *mp);

  return 1;
}

/**
 * lua_account_cb_mailboxes - Get the Mailboxes in the Account
 */
static int lua_account_cb_mailboxes(lua_State *l)
{
  struct Account *a = *(struct Account **) luaL_checkudata(l, 1, "Account");
  lua_pushlightuserdata(l, a);
  lua_pushinteger(l, 0);
  lua_pushcclosure(l, lua_account_cb_mailboxes_iter, 2);

  return 1;
}

/**
 * lua_account_cb_num_mailboxes - Count the number of Mailboxes
 */
static int lua_account_cb_num_mailboxes(lua_State *l)
{
  struct Account *a = *(struct Account **) luaL_checkudata(l, 1, "Account");

  lua_pushinteger(l, ARRAY_SIZE(&a->mailboxes));

  return 1;
}

/**
 * AccountMethods - Account Class Methods
 */
static const struct luaL_Reg AccountMethods[] = {
  // clang-format off
  { "__index",       lua_account_cb_index         },
  { "__tostring",    lua_account_cb_tostring      },
  { "mailboxes",     lua_account_cb_mailboxes     },
  { "num_mailboxes", lua_account_cb_num_mailboxes },
  { NULL, NULL },
  // clang-format on
};

/**
 * lua_account_class - Declare the Account Class
 */
void lua_account_class(lua_State *l)
{
  luaL_newmetatable(l, "Account");
  luaL_setfuncs(l, AccountMethods, 0);
  lua_pop(l, 1);
}
