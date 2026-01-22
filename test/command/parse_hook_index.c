/**
 * @file
 * Test code for parse_hook_index()
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
#include "hooks/lib.h"
#include "parse/lib.h"
#include "common.h"
#include "test_common.h"

static const struct Command IndexFormatHook = { "index-format-hook",
                                                CMD_INDEX_FORMAT_HOOK, NULL };

static struct ConfigDef Vars[] = {
  // clang-format off
  // index-format-hook <name> [!]<pattern> <format-string>
  { "default_hook", DT_STRING, IP "~f %s !~P | (~P ~C %s)", 0, NULL, },
  { NULL },
  // clang-format on
};

static const struct CommandTest Tests[] = {
  // clang-format off
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "date '~d<1d' '%[%H:%M]'" },
  { MUTT_CMD_SUCCESS, "date '~d<1m' '%[%a %d]'" },
  { MUTT_CMD_SUCCESS, "date '~d<1y' '%[%b %d]'" },
  { MUTT_CMD_SUCCESS, "date '~A'    '%[%m/%y]'" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

void test_parse_hook_index(void)
{
  // enum CommandResult parse_hook_index(const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe)

  TEST_CHECK(cs_register_variables(NeoMutt->sub->cs, Vars));

  struct Buffer *line = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();
  enum CommandResult rc;

  for (int i = 0; Tests[i].line; i++)
  {
    TEST_CASE(Tests[i].line);
    parse_error_reset(pe);
    buf_strcpy(line, Tests[i].line);
    buf_seek(line, 0);
    rc = parse_hook_index(&IndexFormatHook, line, pc, pe);
    TEST_CHECK_NUM_EQ(rc, Tests[i].rc);
  }

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&line);
}
