/**
 * @file
 * Test code for parse_lua_source()
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

#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "lua/lib.h"
#include "common.h"
#include "mutt_logging.h"
#include "test_common.h"

#ifdef USE_LUA
static const struct Command LuaSource = { "lua-source", CMD_LUA_SOURCE, NULL, CMD_NO_DATA };

// clang-format off
static const struct CommandTest Tests[] = {
  // lua-source <file>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "%s/lua/test.lua" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on
#endif

static struct ConfigDef LuaVars[] = {
  // clang-format off
  { "lua_debug_file",  DT_PATH|D_PATH_FILE, 0, 0, NULL,                  },
  { "lua_debug_level", DT_NUMBER,           0, 1, debug_level_validator, },
  { NULL },
  // clang-format on
};

void test_parse_lua_source(void)
{
#ifdef USE_LUA
  // enum CommandResult parse_lua_source(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  MuttLogger = log_disp_null;
  TEST_CHECK(cs_register_variables(NeoMutt->sub->cs, LuaVars));
  NeoMutt->lua_module = lua_init();

  struct Buffer *line = buf_pool_get();
  struct Buffer *file = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_SUCCESS;

  for (int i = 0; Tests[i].line; i++)
  {
    TEST_CASE(Tests[i].line);
    buf_reset(err);
    buf_strcpy(line, Tests[i].line);
    test_gen_path(file, buf_string(line));
    buf_seek(file, 0);
    rc = parse_lua_source(&LuaSource, file, err);
    TEST_CHECK_NUM_EQ(rc, Tests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&file);
  buf_pool_release(&line);
  lua_cleanup(&NeoMutt->lua_module);
  commands_clear(&NeoMutt->commands);
  MuttLogger = log_disp_terminal;
#endif
}
