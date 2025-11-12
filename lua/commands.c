/**
 * @file
 * Lua Commands
 *
 * @authors
 * Copyright (C) 2025-2026 Richard Russon <rich@flatcap.org>
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

#include "config.h"
#include <lauxlib.h>
#include <lua.h>
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "lib.h"
#include "parse/lib.h"
#include "module_data.h"
#include "muttlib.h"

bool lua_init_state(lua_State **l);

/**
 * parse_lua - Parse the 'lua' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `lua <lua-command>`
 */
enum CommandResult parse_lua(const struct Command *cmd, struct Buffer *line,
                             const struct ParseContext *pc, struct ParseError *pe)
{
  struct LuaModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_LUA);
  struct Buffer *err = pe->message;

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  // From here on, use the remainder of `line`, raw
  enum CommandResult rc = MUTT_CMD_ERROR;

  lua_State *lua_state = mod_data->lua_state;
  lua_init_state(&lua_state);
  mod_data->lua_state = lua_state;
  mutt_debug(LL_DEBUG2, "%s\n", line->dptr);

  if (luaL_dostring(lua_state, line->dptr) != LUA_OK)
  {
    mutt_debug(LL_DEBUG2, "%s -> failure\n", line->dptr);
    buf_printf(err, "%s: %s", line->dptr, lua_tostring(lua_state, -1));
    /* pop error message from the stack */
    lua_pop(lua_state, 1);
    goto done;
  }
  mutt_debug(LL_DEBUG2, "%s -> success\n", line->dptr);
  buf_reset(line); // Clear the rest of the line

  rc = MUTT_CMD_SUCCESS;

done:
  return rc;
}

/**
 * parse_lua_source - Parse the 'lua-source' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `lua-source <filename>`
 */
enum CommandResult parse_lua_source(const struct Command *cmd, struct Buffer *line,
                                    const struct ParseContext *pc, struct ParseError *pe)
{
  struct LuaModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_LUA);
  struct Buffer *err = pe->message;

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  mutt_debug(LL_DEBUG2, "enter\n");

  lua_State *lua_state = mod_data->lua_state;
  lua_init_state(&lua_state);
  mod_data->lua_state = lua_state;

  if (parse_extract_token(token, line, TOKEN_NO_FLAGS) != 0)
  {
    buf_printf(err, _("source: error at %s"), line->dptr);
    goto done;
  }
  if (MoreArgs(line))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto done;
  }

  expand_path(token, false);

  if (luaL_dofile(lua_state, buf_string(token)) != LUA_OK)
  {
    mutt_error(_("Couldn't source lua source: %s"), lua_tostring(lua_state, -1));
    lua_pop(lua_state, 1);
    goto done;
  }

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  return rc;
}

/**
 * LuaCommands - List of NeoMutt commands to register
 */
const struct Command LuaCommands[] = {
  // clang-format off
  { "lua", CMD_LUA, parse_lua,
        N_("Run a Lua expression or call a Lua function"),
        N_("lua <lua-command>"),
        "optionalfeatures.html#lua" },
  { "lua-source", CMD_LUA_SOURCE, parse_lua_source,
        N_("Execute a Lua script file"),
        N_("lua-source <filename>"),
        "optionalfeatures.html#lua" },

  { NULL, CMD_NONE, NULL, NULL, NULL, NULL, CF_NO_FLAGS },
  // clang-format on
};
