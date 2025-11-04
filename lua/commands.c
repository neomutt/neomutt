/**
 * @file
 * Lua Commands
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
 * @page lua_commands Lua Commands
 *
 * Lua Commands
 */

#ifndef LUA_COMPAT_ALL
#define LUA_COMPAT_ALL
#endif
#ifndef LUA_COMPAT_5_1
#define LUA_COMPAT_5_1
#endif

#include "config.h"
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "lib.h"
#include "parse/lib.h"
#include "muttlib.h"
#include "version.h"

extern lua_State *LuaState;

bool lua_init_state(lua_State **l);

/**
 * parse_lua - Parse the 'lua' command - Implements Command::parse() - @ingroup command_parse
 */
static enum CommandResult parse_lua(struct Buffer *buf, struct Buffer *s,
                                    intptr_t data, struct Buffer *err)
{
  lua_init_state(&LuaState);
  mutt_debug(LL_DEBUG2, "%s\n", buf->data);

  if (luaL_dostring(LuaState, s->dptr))
  {
    mutt_debug(LL_DEBUG2, "%s -> failure\n", s->dptr);
    buf_printf(err, "%s: %s", s->dptr, lua_tostring(LuaState, -1));
    /* pop error message from the stack */
    lua_pop(LuaState, 1);
    return MUTT_CMD_ERROR;
  }
  mutt_debug(LL_DEBUG2, "%s -> success\n", s->dptr);
  buf_reset(s); // Clear the rest of the line
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_lua_source - Parse the 'lua-source' command - Implements Command::parse() - @ingroup command_parse
 */
static enum CommandResult parse_lua_source(struct Buffer *buf, struct Buffer *s,
                                           intptr_t data, struct Buffer *err)
{
  mutt_debug(LL_DEBUG2, "enter\n");

  lua_init_state(&LuaState);

  if (parse_extract_token(buf, s, TOKEN_NO_FLAGS) != 0)
  {
    buf_printf(err, _("source: error at %s"), s->dptr);
    return MUTT_CMD_ERROR;
  }
  if (MoreArgs(s))
  {
    buf_printf(err, _("%s: too many arguments"), "source");
    return MUTT_CMD_WARNING;
  }

  struct Buffer *path = buf_pool_get();
  buf_copy(path, buf);
  buf_expand_path(path);

  if (luaL_dofile(LuaState, buf_string(path)))
  {
    mutt_error(_("Couldn't source lua source: %s"), lua_tostring(LuaState, -1));
    lua_pop(LuaState, 1);
    buf_pool_release(&path);
    return MUTT_CMD_ERROR;
  }

  buf_pool_release(&path);
  return MUTT_CMD_SUCCESS;
}

/**
 * LuaCommands - List of NeoMutt commands to register
 */
static const struct Command LuaCommands[] = {
  // clang-format off
  { "lua",        parse_lua,        0 },
  { "lua-source", parse_lua_source, 0 },
  { NULL, NULL, 0 },
  // clang-format on
};

/**
 * lua_init - Setup feature commands
 */
void lua_init(void)
{
  commands_register(&NeoMutt->commands, LuaCommands);
}

/**
 * lua_cleanup - Clean up Lua
 */
void lua_cleanup(void)
{
  if (LuaState)
  {
    lua_close(LuaState);
    LuaState = NULL;
  }
}
