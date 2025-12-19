/**
 * @file
 * Test code for parse_color()
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
#include "color/lib.h"
#include "common.h"
#include "test_common.h"

static const struct Command Color = { "color", NULL, 0 };

// clang-format off
static const struct CommandTest Tests[] = {
  // color <object> [ <attribute> ... ] <foreground> <background> [ <regex> [ <num> ]]
  { MUTT_CMD_SUCCESS, "" },
  { MUTT_CMD_SUCCESS, "error bold red white" },
  { MUTT_CMD_SUCCESS, "warning color216 color15" },
  { MUTT_CMD_SUCCESS, "message #ff00ff #12f8c6" },
  { MUTT_CMD_SUCCESS, "compose header magenta green" },
  { MUTT_CMD_SUCCESS, "compose_security_none white green" },
  { MUTT_CMD_SUCCESS, "index_author red green '~f fl.*'" },
  { MUTT_CMD_SUCCESS, "status yellow blue '[0-9]+' 1" },
  { MUTT_CMD_ERROR,   NULL },
};
// clang-format on

void test_parse_color(void)
{
  // enum CommandResult parse_color(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  enum CommandResult rc;

  for (int i = 0; Tests[i].line; i++)
  {
    TEST_CASE(Tests[i].line);
    buf_reset(err);
    buf_strcpy(line, Tests[i].line);
    buf_seek(line, 0);
    rc = parse_color(&Color, line, err);
    TEST_CHECK_NUM_EQ(rc, Tests[i].rc);
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
}
