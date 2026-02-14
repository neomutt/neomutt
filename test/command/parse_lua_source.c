/**
 * @file
 * Test code for parse_lua_source()
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

#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "lua/lib.h"
#include "parse/lib.h"
#include "common.h"
#include "test_common.h"

#ifdef USE_LUA
static const struct Command LuaSource = { "lua-source", CMD_LUA_SOURCE, NULL };

static const struct CommandTest Tests[] = {
  // clang-format off
  // lua-source <filename>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "%s/lua/test.lua" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};
#endif

void test_parse_lua_source(void)
{
#ifdef USE_LUA
  // enum CommandResult parse_lua_source(const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe)

  MuttLogger = log_disp_null;

  struct Buffer *line = buf_pool_get();
  struct Buffer *file = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();
  enum CommandResult rc = MUTT_CMD_SUCCESS;

  for (int i = 0; Tests[i].line; i++)
  {
    TEST_CASE(Tests[i].line);
    parse_error_reset(pe);
    buf_strcpy(line, Tests[i].line);
    test_gen_path(file, buf_string(line));
    buf_seek(file, 0);
    rc = parse_lua_source(&LuaSource, file, pc, pe);
    TEST_CHECK_NUM_EQ(rc, Tests[i].rc);
  }

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&file);
  buf_pool_release(&line);
  MuttLogger = log_disp_terminal;
#endif
}
