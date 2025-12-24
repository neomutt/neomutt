/**
 * @file
 * Test code for parse_hook_global()
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
#include "common.h"
#include "hook.h"
#include "test_common.h"

// clang-format off
static const struct Command ShutdownHook = { "shutdown-hook", NULL, MUTT_SHUTDOWN_HOOK | MUTT_GLOBAL_HOOK };
static const struct Command StartupHook  = { "startup-hook",  NULL, MUTT_STARTUP_HOOK | MUTT_GLOBAL_HOOK };
static const struct Command TimeoutHook  = { "timeout-hook",  NULL, MUTT_TIMEOUT_HOOK | MUTT_GLOBAL_HOOK };
// clang-format on

// clang-format off
static const struct CommandTest ShutdownTests[] = {
  // shutdown-hook <command>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "'<shell-escape>touch ~/test<enter>'" },
  { MUTT_CMD_ERROR,   NULL },
};

static const struct CommandTest StartupTests[] = {
  // startup-hook <command>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "'exec sync-mailbox'" },
  { MUTT_CMD_ERROR,   NULL },
};

static const struct CommandTest TimeoutTests[] = {
  // timeout-hook <command>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "'exec sync-mailbox'" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

static void test_parse_shutdown_hook(void)
{
  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; ShutdownTests[i].line; i++)
  {
    TEST_CASE(ShutdownTests[i].line);
    buf_reset(err);
    buf_strcpy(line, ShutdownTests[i].line);
    buf_seek(line, 0);
    rc = parse_hook_global(&ShutdownHook, line, err);
    TEST_CHECK_NUM_EQ(rc, ShutdownTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_startup_hook(void)
{
  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; StartupTests[i].line; i++)
  {
    TEST_CASE(StartupTests[i].line);
    buf_reset(err);
    buf_strcpy(line, StartupTests[i].line);
    buf_seek(line, 0);
    rc = parse_hook_global(&StartupHook, line, err);
    TEST_CHECK_NUM_EQ(rc, StartupTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_timeout_hook(void)
{
  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; TimeoutTests[i].line; i++)
  {
    TEST_CASE(TimeoutTests[i].line);
    buf_reset(err);
    buf_strcpy(line, TimeoutTests[i].line);
    buf_seek(line, 0);
    rc = parse_hook_global(&TimeoutHook, line, err);
    TEST_CHECK_NUM_EQ(rc, TimeoutTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

void test_parse_hook_global(void)
{
  test_parse_shutdown_hook();
  test_parse_startup_hook();
  test_parse_timeout_hook();
}
