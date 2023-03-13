/**
 * @file
 * Test code for parsing config files
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
#include "core/lib.h"
#include "parse/lib.h"

static struct Command mutt_commands[] = {
  // clang-format off
  { "reset",  parse_set, MUTT_SET_RESET },
  { "set",    parse_set, MUTT_SET_SET },
  { "toggle", parse_set, MUTT_SET_INV },
  { "unset",  parse_set, MUTT_SET_UNSET },
  // clang-format on
};

size_t commands_array(struct Command **first)
{
  *first = mutt_commands;
  return mutt_array_size(mutt_commands);
}

void test_parse_rc(void)
{
  enum CommandResult rc = MUTT_CMD_ERROR;

  TEST_CASE("parse_rc_line");
  rc = parse_rc_line(NULL, NULL);
  TEST_CHECK(rc == MUTT_CMD_ERROR);

  TEST_CASE("parse_rc_buffer");
  rc = parse_rc_buffer(NULL, NULL, NULL);
  TEST_CHECK(rc == MUTT_CMD_SUCCESS);
}
