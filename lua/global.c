/**
 * @file
 * Lua Global Functions and Variables
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
 * @page lua_global Lua Global Functions and Variables
 *
 * Lua Global Functions and Variables
 */

#include "config.h"
#include <lauxlib.h>
#include <lua.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "parse/lib.h"
#include "logging.h"
#include "version.h"

/**
 * lua_handle_error - Handle an error in the Lua interpreter
 * @param l Lua State
 * @retval -1 Always
 */
static int lua_handle_error(lua_State *l)
{
  lua_debug(LL_DEBUG1, "lua runtime error: %s\n", lua_tostring(l, -1));
  lua_error("Lua runtime error: %s", lua_tostring(l, -1));
  lua_pop(l, 1);
  return -1;
}

/**
 * lua_global_cb_call - Call a NeoMutt command by name
 * @param l Lua State
 * @retval >=0 Success
 * @retval -1 Error
 *
 * XXX: Usage: expects ONE or more params (concatenated)
 * XXX: Retval: enum CommandResult
 * XXX: Error: retval, msg?
 */
static int lua_global_cb_call(lua_State *l)
{
  lua_debug(LL_DEBUG2, "enter\n");
  struct Buffer *err = buf_pool_get();
  struct Buffer *line = buf_pool_get();
  const struct Command *cmd = NULL;
  int rc = 0;

  if (lua_gettop(l) == 0)
  {
    luaL_error(l, "Error command argument required");
    return -1;
  }

  cmd = commands_get(&NeoMutt->commands, lua_tostring(l, 1));
  if (!cmd)
  {
    luaL_error(l, "Error command %s not found", lua_tostring(l, 1));
    return -1;
  }

  for (int i = 2; i <= lua_gettop(l); i++)
  {
    buf_addstr(line, lua_tostring(l, i));
    buf_addch(line, ' ');
  }
  buf_seek(line, 0);

  if (cmd->parse(cmd, line, err))
  {
    luaL_error(l, "NeoMutt error: %s", buf_string(err));
    rc = -1;
  }
  else
  {
    if (!lua_pushstring(l, buf_string(err)))
      lua_handle_error(l);
    else
      rc++;
  }

  buf_pool_release(&line);
  buf_pool_release(&err);
  return rc;
}

/**
 * lua_global_cb_enter - Execute NeoMutt config from Lua
 * @param l Lua State
 * @retval >=0 Success
 * @retval -1  Error
 *
 * XXX: Usage: expects ONE string param
 * XXX: Retval: enum CommandResult?
 * XXX: Error: ABORT
 */
static int lua_global_cb_enter(lua_State *l)
{
  lua_debug(LL_DEBUG2, "enter\n");
  struct Buffer *err = buf_pool_get();
  struct Buffer *line = buf_pool_get();
  buf_strcpy(line, lua_tostring(l, -1));
  int rc = 0;

  if (parse_rc_line(line, err) != MUTT_CMD_SUCCESS)
  {
    luaL_error(l, "NeoMutt error: %s", buf_string(err));
    rc = -1;
  }
  else
  {
    if (!lua_pushstring(l, buf_string(err)))
      lua_handle_error(l);
    else
      rc++;
  }

  buf_pool_release(&line);
  buf_pool_release(&err);

  return rc;
}

/**
 * lua_global_cb_refresh - Callback for Global::refresh() - Implements ::lua_callback_t
 *
 * XXX: redraw() - reflow/recalc/repaint everything?
 * XXX: Usage:
 * XXX: Retval:
 * XXX: Error:
 */
static int lua_global_cb_refresh(lua_State *l)
{
  return 0;
}

/**
 * lua_global_init - Initialise the Lua global functions and variables
 */
void lua_global_init(lua_State *l)
{
  lua_pushstring(l, mutt_make_version());
  lua_setglobal(l, "VERSION");

  lua_register(l, "call", lua_global_cb_call);
  lua_register(l, "enter", lua_global_cb_enter);
  lua_register(l, "refresh", lua_global_cb_refresh);
}
