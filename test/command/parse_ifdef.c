/**
 * @file
 * Test code for parse_ifdef()
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
#include "commands.h"
#include "common.h"
#include "test_common.h"

// clang-format off
static const struct Command Ifdef  = { "ifdef",  CMD_IFDEF,  NULL, CMD_NO_DATA };
static const struct Command Ifndef = { "ifndef", CMD_IFNDEF, NULL, CMD_NO_DATA };
// clang-format on

const struct Command test_commands[] = {
  // clang-format off
  { "echo", CMD_ECHO, parse_echo, CMD_NO_DATA },
  { NULL, CMD_NONE, NULL, CMD_NO_DATA },
  // clang-format on
};

// clang-format off
static const struct CommandTest IfdefTests[] = {
  // ifdef <symbol> "<config-command> [ <args> ... ]"
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_WARNING, "folder" },
  { MUTT_CMD_SUCCESS, "folder echo variable" },
  { MUTT_CMD_SUCCESS, "hcache echo feature" },
  { MUTT_CMD_SUCCESS, "next-page echo function" },
  { MUTT_CMD_SUCCESS, "score echo command" },
  { MUTT_CMD_SUCCESS, "index_author echo color" },
  { MUTT_CMD_SUCCESS, "lmdb echo store" },
  { MUTT_CMD_SUCCESS, "HOME echo env" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

// clang-format off
static const struct CommandTest IfndefTests[] = {
  // ifndef <symbol> "<config-command> [ <args> ... ]"
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_WARNING, "folder" },
  { MUTT_CMD_SUCCESS, "folder echo variable" },
  { MUTT_CMD_SUCCESS, "hcache echo feature" },
  { MUTT_CMD_SUCCESS, "next-page echo function" },
  { MUTT_CMD_SUCCESS, "score echo command" },
  { MUTT_CMD_SUCCESS, "index_author echo color" },
  { MUTT_CMD_SUCCESS, "lmdb echo store" },
  { MUTT_CMD_SUCCESS, "HOME echo env" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

static void test_parse_ifdef2(void)
{
  // enum CommandResult parse_ifdef(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; IfdefTests[i].line; i++)
  {
    TEST_CASE(IfdefTests[i].line);
    buf_reset(err);
    buf_strcpy(line, IfdefTests[i].line);
    buf_seek(line, 0);
    rc = parse_ifdef(&Ifdef, line, err);
    TEST_CHECK_NUM_EQ(rc, IfdefTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

static void test_parse_ifndef(void)
{
  // enum CommandResult parse_ifdef(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; IfndefTests[i].line; i++)
  {
    TEST_CASE(IfndefTests[i].line);
    buf_reset(err);
    buf_strcpy(line, IfndefTests[i].line);
    buf_seek(line, 0);
    rc = parse_ifdef(&Ifndef, line, err);
    TEST_CHECK_NUM_EQ(rc, IfndefTests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}

void test_parse_ifdef(void)
{
  MuttLogger = log_disp_null;
  commands_register(&NeoMutt->commands, test_commands);

  test_parse_ifdef2();
  test_parse_ifndef();
  MuttLogger = log_disp_terminal;
  commands_clear(&NeoMutt->commands);
}
