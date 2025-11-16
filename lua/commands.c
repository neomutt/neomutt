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

#include "config.h"
#include <lauxlib.h>
#include <lua.h>
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "lib.h"
#include "parse/lib.h"
#include "module.h"
#include "muttlib.h"

/**
 * parse_lua - Parse the 'lua' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `lua "<lua-commands>"`
 */
enum CommandResult parse_lua(const struct Command *cmd, struct Buffer *line, struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  parse_extract_token(token, line, TOKEN_NO_FLAGS);

  lua_State *l = lua_init_state();
  if (!l)
    return MUTT_CMD_ERROR;

  mutt_debug(LL_DEBUG2, "%s\n", buf_string(token));

  if (luaL_dostring(l, buf_string(token)) != LUA_OK)
  {
    mutt_debug(LL_DEBUG2, "%s -> failure\n", buf_string(token));
    buf_printf(err, "%s: %s", buf_string(token), lua_tostring(l, -1));
    /* pop error message from the stack */
    lua_pop(l, 1);
    goto done;
  }
  mutt_debug(LL_DEBUG2, "%s -> success\n", buf_string(token));
  buf_reset(line); // Clear the rest of the line

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  return rc;
}

/**
 * parse_lua_source - Parse the 'lua-source' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `lua-source <file>`
 */
enum CommandResult parse_lua_source(const struct Command *cmd,
                                    struct Buffer *line, struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  lua_State *l = lua_init_state();
  if (!l)
    return MUTT_CMD_ERROR;

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

  buf_expand_path(token);

  if (luaL_dofile(l, buf_string(token)) != LUA_OK)
  {
    mutt_error(_("Couldn't source lua source: %s"), lua_tostring(l, -1));
    lua_pop(l, 1);
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
static const struct Command LuaCommands[] = {
  // clang-format off
  { "lua", CMD_LUA, parse_lua, CMD_NO_DATA,
        N_("Run a Lua expression or call a Lua function"),
        N_("lua '<lua-commands>'"),
        "optionalfeatures.html#lua-commands" },
  { "lua-source", CMD_LUA_SOURCE, parse_lua_source, CMD_NO_DATA,
        N_("Execute a Lua script file"),
        N_("lua-source <file>"),
        "optionalfeatures.html#lua-commands" },

  { NULL, CMD_NONE, NULL, CMD_NO_DATA, NULL, NULL, NULL, CF_NO_FLAGS },
  // clang-format on
};

/**
 * lua_commands_init - Setup Lua commands
 */
void lua_commands_init(void)
{
  commands_register(&NeoMutt->commands, LuaCommands);
}
