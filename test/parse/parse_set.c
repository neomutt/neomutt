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
#include "test_common.h"

enum CommandResult set_dump(enum GetElemListFlags flags, struct Buffer *err)
{
  buf_strcpy(err, "config");
  return MUTT_CMD_SUCCESS;
}

void test_parse_set(void)
{
  TEST_CASE("parse_set");
  enum CommandResult rc = parse_set(NULL, NULL, 0, NULL);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_ERROR);
}
