/**
 * @file
 * Test code for parse_source()
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

static const struct Command Source = { "source", CMD_SOURCE, NULL, CMD_NO_DATA };

const struct Command source_test_commands[] = {
  // clang-format off
  { "set", CMD_SET, parse_set, CMD_NO_DATA },
  { NULL, CMD_NONE, NULL, CMD_NO_DATA },
  // clang-format on
};

static const struct CommandTest Tests[] = {
  // clang-format off
  // source <filename>
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "%s/source/test.rc" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

void test_parse_source(void)
{
  // enum CommandResult parse_source(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  commands_register(&NeoMutt->commands, source_test_commands);

  struct Buffer *line = buf_pool_get();
  struct Buffer *file = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; Tests[i].line; i++)
  {
    TEST_CASE(Tests[i].line);
    buf_reset(err);
    buf_strcpy(line, Tests[i].line);
    test_gen_path(file, buf_string(line));
    buf_seek(file, 0);
    rc = parse_source(&Source, file, err);
    TEST_CHECK_NUM_EQ(rc, Tests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&file);
  buf_pool_release(&line);
  commands_clear(&NeoMutt->commands);
}
