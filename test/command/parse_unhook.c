/**
 * @file
 * Test code for parse_unhook()
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
#include "common.h"
#include "test_common.h"

// clang-format off
static const struct Command FolderHook = { "folder-hook", CMD_FOLDER_HOOK, NULL, CMD_NO_DATA };
static const struct Command UnHook     = { "unhook",      CMD_UNHOOK,      NULL, CMD_NO_DATA };
// clang-format on

static struct ConfigDef Vars[] = {
  // clang-format off
  { "default_hook", DT_STRING, IP "~f %s !~P | (~P ~C %s)", 0, NULL, },
  { NULL },
  // clang-format on
};

const struct Command unhook_test_commands[] = {
  // clang-format off
  { "folder-hook", CMD_FOLDER_HOOK, parse_hook_folder, CMD_NO_DATA },
  { NULL, CMD_NONE, NULL, CMD_NO_DATA },
  // clang-format on
};

static const struct CommandTest Tests[] = {
  // clang-format off
  // unhook { * | <hook-type> }
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "folder-hook" },
  { MUTT_CMD_SUCCESS, "*" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

void test_parse_unhook(void)
{
  // enum CommandResult parse_unhook(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  TEST_CHECK(cs_register_variables(NeoMutt->sub->cs, Vars));
  commands_register(&NeoMutt->commands, unhook_test_commands);

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  // Create a folder-hook, to delete
  buf_strcpy(line, "~g 'set my_var=42'");
  buf_seek(line, 0);
  rc = parse_hook_folder(&FolderHook, line, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);

  for (int i = 0; Tests[i].line; i++)
  {
    TEST_CASE(Tests[i].line);
    buf_reset(err);
    buf_strcpy(line, Tests[i].line);
    buf_seek(line, 0);
    rc = parse_unhook(&UnHook, line, err);
    TEST_CHECK_NUM_EQ(rc, Tests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
  commands_clear(&NeoMutt->commands);
}
