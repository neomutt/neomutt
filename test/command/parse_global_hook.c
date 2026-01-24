/**
 * @file
 * Test code for parse_global_hook()
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
static const struct Command ShutdownHook = { "shutdown-hook", CMD_SHUTDOWN_HOOK, NULL };
static const struct Command StartupHook  = { "startup-hook",  CMD_STARTUP_HOOK,  NULL };
static const struct Command TimeoutHook  = { "timeout-hook",  CMD_TIMEOUT_HOOK,  NULL };
// clang-format on

static const struct CommandTest ShutdownTests[] = {
  // clang-format off
  // shutdown-hook <command>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "'<shell-escape>touch ~/test<enter>'" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

static const struct CommandTest StartupTests[] = {
  // clang-format off
  // startup-hook  <command>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "'exec sync-mailbox'" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

static const struct CommandTest TimeoutTests[] = {
  // clang-format off
  // timeout-hook  <command>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "'exec sync-mailbox'" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

static void test_parse_shutdown_hook(void)
{
  struct Buffer *line = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();
  enum CommandResult rc;

  for (int i = 0; ShutdownTests[i].line; i++)
  {
    TEST_CASE(ShutdownTests[i].line);
    parse_error_reset(pe);
    buf_strcpy(line, ShutdownTests[i].line);
    buf_seek(line, 0);
    rc = parse_global_hook(&ShutdownHook, line, pc, pe);
    TEST_CHECK_NUM_EQ(rc, ShutdownTests[i].rc);
  }

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&line);
}

static void test_parse_startup_hook(void)
{
  struct Buffer *line = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();
  enum CommandResult rc;

  for (int i = 0; StartupTests[i].line; i++)
  {
    TEST_CASE(StartupTests[i].line);
    parse_error_reset(pe);
    buf_strcpy(line, StartupTests[i].line);
    buf_seek(line, 0);
    rc = parse_global_hook(&StartupHook, line, pc, pe);
    TEST_CHECK_NUM_EQ(rc, StartupTests[i].rc);
  }

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&line);
}

static void test_parse_timeout_hook(void)
{
  struct Buffer *line = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();
  enum CommandResult rc;

  for (int i = 0; TimeoutTests[i].line; i++)
  {
    TEST_CASE(TimeoutTests[i].line);
    parse_error_reset(pe);
    buf_strcpy(line, TimeoutTests[i].line);
    buf_seek(line, 0);
    rc = parse_global_hook(&TimeoutHook, line, pc, pe);
    TEST_CHECK_NUM_EQ(rc, TimeoutTests[i].rc);
  }

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&line);
}

void test_parse_hook_global(void)
{
  test_parse_shutdown_hook();
  test_parse_startup_hook();
  test_parse_timeout_hook();
}
