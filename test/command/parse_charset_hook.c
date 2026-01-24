/**
 * @file
 * Test code for parse_charset_hook()
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
#include "hooks/lib.h"
#include "parse/lib.h"
#include "common.h"
#include "test_common.h"

// clang-format off
static const struct Command CharsetHook = { "charset-hook", CMD_CHARSET_HOOK, NULL };
static const struct Command IconvHook   = { "iconv-hook",   CMD_ICONV_HOOK,   NULL };
// clang-format on

static const struct CommandTest CharsetTests[] = {
  // clang-format off
  // charset-hook <alias>   <charset>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "u8 utf-8" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

static const struct CommandTest IconvTests[] = {
  // clang-format off
  // iconv-hook   <charset> <local-charset>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "ascii utf-8" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

void test_parse_hook_charset2(void)
{
  // enum CommandResult parse_charset_hook(const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe)

  struct Buffer *line = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();
  enum CommandResult rc;

  for (int i = 0; CharsetTests[i].line; i++)
  {
    TEST_CASE(CharsetTests[i].line);
    parse_error_reset(pe);
    buf_strcpy(line, CharsetTests[i].line);
    buf_seek(line, 0);
    rc = parse_charset_hook(&CharsetHook, line, pc, pe);
    TEST_CHECK_NUM_EQ(rc, CharsetTests[i].rc);
  }

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&line);
}

void test_parse_iconv_hook(void)
{
  // enum CommandResult parse_charset_hook(const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe)

  struct Buffer *line = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();
  enum CommandResult rc;

  for (int i = 0; IconvTests[i].line; i++)
  {
    TEST_CASE(IconvTests[i].line);
    parse_error_reset(pe);
    buf_strcpy(line, IconvTests[i].line);
    buf_seek(line, 0);
    rc = parse_charset_hook(&IconvHook, line, pc, pe);
    TEST_CHECK_NUM_EQ(rc, IconvTests[i].rc);
  }

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&line);
}

void test_parse_hook_charset(void)
{
  test_parse_hook_charset2();
  test_parse_iconv_hook();
}
