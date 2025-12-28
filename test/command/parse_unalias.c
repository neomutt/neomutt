/**
 * @file
 * Test code for parse_unalias()
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
#include "alias/lib.h"
#include "common.h"
#include "test_common.h"

static const struct Command UnAlias = { "unalias", CMD_UNALIAS, NULL, CMD_NO_DATA };

// clang-format off
static const struct CommandTest Tests[] = {
  // unalias [ -group <name> ... ] { * | <key> ... }
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_SUCCESS, "jim" },
  { MUTT_CMD_SUCCESS, "-group work bob" },
  { MUTT_CMD_SUCCESS, "*" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

void test_parse_unalias(void)
{
  // enum CommandResult parse_unalias(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; Tests[i].line; i++)
  {
    TEST_CASE(Tests[i].line);
    buf_reset(err);
    buf_strcpy(line, Tests[i].line);
    buf_seek(line, 0);
    rc = parse_unalias(&UnAlias, line, err);
    TEST_CHECK_NUM_EQ(rc, Tests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}
