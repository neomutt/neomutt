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

/**
 * @page lua_module Lua Module
 *
 * Lua Module
 */

#include "config.h"
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "module.h"
#include "config2.h"
#include "global.h"
#include "gui.h"
#include "logging.h"
#include "neomutt.h"

void lua_commands_init(void);

#define LUA_CLASS(NAME)                                                        \
  void lua_##NAME##_class(lua_State *l);                                       \
  lua_##NAME##_class(l);

/**
 * lua_init - XXX
 */
struct LuaModule *lua_init(void)
{
  lua_commands_init();

  struct LuaModule *lm = MUTT_MEM_CALLOC(1, struct LuaModule);

  // Lua State and Log file will be initialised lazily
  // on a lua-* command from the user

  return lm;
}

/**
 * lua_cleanup - Clean up Lua
 */
void lua_cleanup(struct LuaModule **pptr)
{
  if (!pptr || !*pptr)
    return;

  struct LuaModule *lm = *pptr;

  if (lm->lua_state)
  {
    lua_close(lm->lua_state);
    lm->lua_state = NULL;
  }

  lua_log_close(&lm->log_file);

  FREE(pptr);
}

/**
 * lua_handle_panic - Handle a panic in the Lua interpreter
 * @param l Lua State
 * @retval -1 Always
 */
static int lua_handle_panic(lua_State *l)
{
  lua_debug(LL_DEBUG1, "lua runtime panic: %s\n", lua_tostring(l, -1));
  lua_error("Lua runtime panic: %s", lua_tostring(l, -1));
  lua_pop(l, 1);
  return -1;
}

/**
 * lua_classes - Set up all the Lua Classes
 */
static void lua_classes(lua_State *l)
{
  LUA_CLASS(account);
  LUA_CLASS(config);
  LUA_CLASS(email);
  LUA_CLASS(emailarray);
  LUA_CLASS(index);
  LUA_CLASS(mailbox);
  LUA_CLASS(neomutt);
}
/**
 * lua_init_state - Initialise a Lua State
 * @param[out] l Lua State
 * @retval true Successful
 * XXX
 */
lua_State *lua_init_state(void)
{
  if (!NeoMutt || !NeoMutt->lua_module)
    return NULL;

  struct LuaModule *lm = NeoMutt->lua_module;
  if (lm->lua_state)
    return lm->lua_state;

  if (!lm->log_file)
    lm->log_file = lua_log_open();

  lm->lua_state = luaL_newstate();
  if (!lm->lua_state)
  {
    lua_error(_("Error: Couldn't load the lua interpreter"));
    lua_log_close(&lm->log_file);
    return NULL;
  }

  lua_State *l = lm->lua_state;

  lua_atpanic(l, lua_handle_panic);

  // Load Standard Libraries - https://www.lua.org/manual/5.4/manual.html#6
  luaL_openlibs(l);

  lua_classes(l);

  lua_log_init(l);
  lua_global_init(l);
  lua_config_init(l);
  lua_neomutt_init(l);
  lua_gui_init(l);

  lua_debug(LL_DEBUG1, "init: stack %d\n", lua_gettop(l));
  return l;
}
