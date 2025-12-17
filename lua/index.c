/**
 * @file
 * Index Dialog Wrapper
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
 * @page lua_index Index Dialog Wrapper
 *
 * Index Dialog Wrapper
 */

#include <lauxlib.h>
#include <lua.h>
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "index/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "helpers.h"
#include "logging.h"

extern const struct MenuFuncOp OpIndex[];

/**
 * fn_match - XXX
 *
 * XXX case matters
 * XXX [-_] doesn't
 */
bool fn_match(const char *fn, const char *test)
{
  if (!fn || !*fn || !test || !*test)
    return false;

  for (int i = 0;; i++)
  {
    if (fn[i] == '-')
    {
      if ((test[i] != '-') && (test[i] != '_'))
        return false;
    }
    else if (fn[i] != test[i])
    {
      return false;
    }

    if (fn[i] == '\0')
      break;
  }

  return true;
}

/**
 * lua_index_cb_function - XXX
 */
int lua_index_cb_function(lua_State *l)
{
  lua_debug(LL_DEBUG1, "UPVALUE1: %s\n", lua_type_name(lua_type(l, lua_upvalueindex(1))));
  lua_debug(LL_DEBUG1, "        %lld\n", lua_tointeger(l, lua_upvalueindex(1)));
  lua_debug(LL_DEBUG1, "UPVALUE2: %s\n", lua_type_name(lua_type(l, lua_upvalueindex(2))));
  // luaL_getmetafield(l, lua_upvalueindex(2), "__name");
  // lua_debug(LL_DEBUG1, "        %s\n", lua_tostring(l, -1));
  // lua_pop(l, 1);

  int op = lua_tointeger(l, lua_upvalueindex(1));
  struct MuttWindow *win = lua_touserdata(l, lua_upvalueindex(2));

  int top = lua_gettop(l);
  for (int i = 1; i <= top; i++)
  {
    // lua_debug(LL_DEBUG1, "lua_index_cb_function: stack: %s(%d)\n", lua_type_name(lua_type(l, i)), lua_type(l, i));
    if (lua_type(l, i) == LUA_TUSERDATA)
    {
      void *ptr = lua_touserdata(l, 1);

      // lua_debug(LL_DEBUG1, "STACK1: %d\n", lua_gettop(l));
      int rc = luaL_getmetafield(l, 1, "__name");
      // lua_debug(LL_DEBUG1, "STACK2: %d\n", lua_gettop(l));
      if (rc == LUA_TNIL)
      {
        lua_debug(LL_DEBUG1, "        %p\n", ptr);
      }
      else
      {
        lua_debug(LL_DEBUG1, "        %p - %s\n", ptr, lua_tostring(l, -1));
      }
      lua_pop(l, 1);
      // lua_debug(LL_DEBUG1, "STACK3: %d\n", lua_gettop(l));
    }
    else if (lua_type(l, i) == LUA_TSTRING)
    {
      lua_debug(LL_DEBUG1, "        %s\n", lua_tostring(l, i));
    }
    else if (lua_type(l, i) == LUA_TNUMBER)
    {
      lua_debug(LL_DEBUG1, "        %lld\n", lua_tointeger(l, i));
    }
  }

  lua_debug(LL_DEBUG1, "ACTION %s (%d) - %p\n", opcodes_get_name(op), op, win);
  int rc = index_function_dispatcher(win, op);
  lua_debug(LL_DEBUG1, "RESULT: %d\n", rc);

  // lua_pushstring(l, opcodes_get_name(op));
  lua_pushinteger(l, rc);
  return 1;
}

/**
 * lua_index_cb_index - Callback for Lua Metamethod Index::__index() - Implements ::lua_callback_t
 */
static int lua_index_cb_index(lua_State *l)
{
  bool found = lua_index_lookup(l, "Index");
  if (found)
    return 1;

  struct MuttWindow *win = *(struct MuttWindow **) luaL_checkudata(l, 1, "Index");
  lua_debug(LL_DEBUG1, "win = %p\n", win);

  const char *param = lua_tostring(l, -1);
  lua_debug(LL_DEBUG2, "Index: %s\n", param);

  int op = OP_NULL;
  for (int i = 0; OpIndex[i].name; i++)
  {
    if (fn_match(OpIndex[i].name, param))
    {
      op = OpIndex[i].op;
      break;
    }
  }

  if (op == OP_NULL)
  {
    luaL_error(l, "unknown function");
    return 0;
  }

  lua_pop(l, 2);
  // store upvalues
  lua_pushinteger(l, op);
  lua_pushlightuserdata(l, win);
  lua_pushcclosure(l, lua_index_cb_function, 2);

  return 1;
}

/**
 * lua_index_cb_get_current_email - XXX - Implements ::lua_callback_t
 */
static int lua_index_cb_get_current_email(lua_State *l)
{
  struct MuttWindow *win = *(struct MuttWindow **) luaL_checkudata(l, 1, "Index");

  struct MuttWindow *dlg = dialog_find(win);
  if (!dlg || !dlg->wdata || !win->parent || !win->parent->wdata)
    return 0;

  struct IndexSharedData *shared = dlg->wdata;

  struct Email *e = shared->email;

  struct Email **pptr = lua_newuserdata(l, sizeof(struct Email *));
  *pptr = e;
  luaL_getmetatable(l, "Email");
  lua_setmetatable(l, -2);

  return 1;
}

/**
 * lua_index_cb_get_current_mailbox - XXX - Implements ::lua_callback_t
 */
static int lua_index_cb_get_current_mailbox(lua_State *l)
{
  struct MuttWindow *win = *(struct MuttWindow **) luaL_checkudata(l, 1, "Index");

  struct MuttWindow *dlg = dialog_find(win);
  if (!dlg || !dlg->wdata || !win->parent || !win->parent->wdata)
    return 0;

  struct IndexSharedData *shared = dlg->wdata;

  struct Mailbox *m = shared->mailbox;

  struct Mailbox **pptr = lua_newuserdata(l, sizeof(struct Mailbox *));
  *pptr = m;
  luaL_getmetatable(l, "Mailbox");
  lua_setmetatable(l, -2);

  return 1;
}

/**
 * lua_index_cb_get_current_mailbox_view - XXX - Implements ::lua_callback_t
 */
static int lua_index_cb_get_current_mailbox_view(lua_State *l)
{
  struct MuttWindow *win = *(struct MuttWindow **) luaL_checkudata(l, 1, "Index");

  struct MuttWindow *dlg = dialog_find(win);
  if (!dlg || !dlg->wdata || !win->parent || !win->parent->wdata)
    return 0;

  struct IndexSharedData *shared = dlg->wdata;

  struct MailboxView *mv = shared->mailbox_view;

  struct MailboxView **pptr = lua_newuserdata(l, sizeof(struct MailboxView *));
  *pptr = mv;
  luaL_getmetatable(l, "MailboxView");
  lua_setmetatable(l, -2);

  return 1;
}

/**
 * lua_index_cb_get_functions - XXX - Implements ::lua_callback_t
 */
static int lua_index_cb_get_functions(lua_State *l)
{
  // Index supports: OpIndex, OpGeneric
  // and redirects: OpSidebar, OpGlobal (fake)

  lua_debug(LL_DEBUG1, "get_functions\n");
  // struct MuttWindow *win = *(struct MuttWindow **) luaL_checkudata(l, 1, "Index");
  // struct Menu *menu = win->wdata;
  // menu_next_entry(menu);

  lua_newtable(l);

  int count = 0;
  for (int i = 0; OpIndex[i].name; i++)
  {
    if (!(OpIndex[i].flags & MFF_LUA)) //XXX also check index prereq()
      continue;

    lua_pushstring(l, OpIndex[i].name);
    lua_pushinteger(l, OpIndex[i].op);
    lua_settable(l, -3);
    count++;
  }

  lua_debug(LL_DEBUG1, "get-functions() -> %d\n", count);

  return 1;
}

/**
 * IndexMethods - Index Class Methods
 */
static const struct luaL_Reg IndexMethods[] = {
  // clang-format off
  { "__index",                  lua_index_cb_index                    },
  { "get_current_email",        lua_index_cb_get_current_email        },
  { "get_current_mailbox",      lua_index_cb_get_current_mailbox      },
  { "get_current_mailbox_view", lua_index_cb_get_current_mailbox_view },
  { "get_functions",            lua_index_cb_get_functions            },
  { NULL, NULL },
  // clang-format on
};

/**
 * lua_index_class - Declare the Index Class
 */
void lua_index_class(lua_State *l)
{
  luaL_newmetatable(l, "Index");
  luaL_setfuncs(l, IndexMethods, 0);
  lua_pop(l, 1);
}
