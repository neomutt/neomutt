/**
 * @file
 * Test code for parsing the 'set' command
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

void mutt_buffer_expand_path(struct Buffer *buf)
{
}

void myvar_append(const char *var, const char *val)
{
}

void myvar_del(const char *var)
{
}

void myvar_set(const char *var, const char *val)
{
}

enum CommandResult set_dump(struct Buffer *buf, struct Buffer *s, intptr_t data,
                            struct Buffer *err)
{
  return MUTT_CMD_SUCCESS;
}

void test_parse_set(void)
{
  TEST_CASE("parse_set");
  enum CommandResult rc = parse_set(NULL, NULL, 0, NULL);
  TEST_CHECK(rc == MUTT_CMD_SUCCESS);
}
