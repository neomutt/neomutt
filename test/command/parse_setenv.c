/**
 * @file
 * Test code for parse_setenv()
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
#include "core/lib.h"
#include "commands/lib.h"
#include "parse/lib.h"
#include "common.h"
#include "test_common.h"

// clang-format off
static const struct Command Setenv   = { "setenv",   CMD_SETENV,   NULL, CMD_NO_DATA };
static const struct Command UnSetenv = { "unsetenv", CMD_UNSETENV, NULL, CMD_NO_DATA };
// clang-format on

static const struct CommandTest SetenvTests[] = {
  // clang-format off
  // setenv { <variable>? | <variable>=<value> }
  { MUTT_CMD_SUCCESS, "" },
  // Old syntax: variable name followed by value (space-separated)
  { MUTT_CMD_SUCCESS, "ORGANIZATION 'The NeoMutt Development Team'" },
  { MUTT_CMD_SUCCESS, "TERM vt100" },
  // New syntax: variable=value (equals sign, no space)
  { MUTT_CMD_SUCCESS, "ORGANIZATION='The NeoMutt Development Team'" },
  { MUTT_CMD_SUCCESS, "TERM=vt100" },
  { MUTT_CMD_SUCCESS, "PATH=/usr/bin:/bin" },
  // New syntax with quotes
  { MUTT_CMD_SUCCESS, "MY_VAR=\"quoted value\"" },
  { MUTT_CMD_SUCCESS, "TEST_123='single quotes'" },
  // Mixed syntax with equals and space (should work)
  { MUTT_CMD_SUCCESS, "VAR_NAME= value" },
  // Variable names with underscores and numbers
  { MUTT_CMD_SUCCESS, "MY_VAR_123=test" },
  { MUTT_CMD_SUCCESS, "_UNDERSCORE_START=value" },
  // Lowercase and mixed case (valid)
  { MUTT_CMD_SUCCESS, "lowercase=value" },
  { MUTT_CMD_SUCCESS, "Mixed_Case=value" },
  { MUTT_CMD_SUCCESS, "myVar=value" },
  // Invalid variable names (starting with digit or special characters)
  { MUTT_CMD_WARNING, "123STARTS_WITH_NUMBER=value" },
  { MUTT_CMD_WARNING, "HAS-DASH=value" },
  { MUTT_CMD_WARNING, "HAS.DOT=value" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

static const struct CommandTest UnSetenvTests[] = {
  // clang-format off
  // unsetenv <variable>
  { MUTT_CMD_SUCCESS, "" },
  { MUTT_CMD_SUCCESS, "ORGANIZATION" },
  { MUTT_CMD_SUCCESS, "NONEXISTENT_VAR" },  // Should succeed even if doesn't exist
  // Lowercase and mixed case (valid)
  { MUTT_CMD_SUCCESS, "lowercase" },
  { MUTT_CMD_SUCCESS, "Mixed_Case" },
  // Underscore prefix (now valid)
  { MUTT_CMD_SUCCESS, "_UNDERSCORE" },
  // Invalid variable names (starting with digit or special characters)
  { MUTT_CMD_WARNING, "123NUMBER" },
  { MUTT_CMD_WARNING, "HAS-DASH" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

static void test_parse_setenv2(void)
{
  // enum CommandResult parse_setenv(const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe)

  struct Buffer *line = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();
  enum CommandResult rc;

  for (int i = 0; SetenvTests[i].line; i++)
  {
    TEST_CASE(SetenvTests[i].line);
    parse_error_reset(pe);
    buf_strcpy(line, SetenvTests[i].line);
    buf_seek(line, 0);
    rc = parse_setenv(&Setenv, line, pc, pe);
    TEST_CHECK_NUM_EQ(rc, SetenvTests[i].rc);
  }

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&line);
}

static void test_parse_unsetenv(void)
{
  // enum CommandResult parse_setenv(const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe)

  struct Buffer *line = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();
  enum CommandResult rc;

  for (int i = 0; UnSetenvTests[i].line; i++)
  {
    TEST_CASE(UnSetenvTests[i].line);
    parse_error_reset(pe);
    buf_strcpy(line, UnSetenvTests[i].line);
    buf_seek(line, 0);
    rc = parse_setenv(&UnSetenv, line, pc, pe);
    TEST_CHECK_NUM_EQ(rc, UnSetenvTests[i].rc);
  }

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&line);
}

void test_parse_setenv(void)
{
  test_parse_setenv2();
  test_parse_unsetenv();
}
