/**
 * @file
 * Test code for parsing the 'set' command
 *
 * @authors
 * Copyright (C) 2023-2024 Richard Russon <rich@flatcap.org>
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
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "parse/lib.h"
#include "common.h"
#include "test_common.h"

void test_parse_set(void)
{
  // enum CommandResult parse_set(const struct Command *cmd, struct Buffer *line, struct Buffer *err)

  // reset <variable> [ <variable> ... ]
  // set { [ no | inv | & ] <variable> [?] | <variable> [=|+=|-=] value } [... ]
  // toggle <variable> [ <variable> ... ]
  // unset <variable> [ <variable> ... ]

  struct Buffer *line = buf_pool_get();
  struct Buffer *err = buf_pool_get();

  TEST_CASE("parse_set");
  enum CommandResult rc = parse_set(&mutt_commands[MUTT_SET_SET], NULL, line, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);

  buf_pool_release(&line);
  buf_pool_release(&err);
}
