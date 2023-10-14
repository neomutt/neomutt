/**
 * @file
 * Test code for Colour Parsing
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
#include <stddef.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "color/lib.h"

enum CommandResult parse_color_name(const char *s, struct ColorElement *elem,
                                    struct Buffer *err);

void test_parse_color_name(void)
{
  // enum CommandResult parse_color_name(const char *s, struct ColorElement *elem, struct Buffer *err);

  struct Buffer *err = buf_pool_get();
  struct ColorElement elem = { 0 };
  enum CommandResult rc;

  const char *tests[] = { "#11AAFF", "color123", "brightred", NULL };

  for (int i = 0; tests[i]; i++)
  {
    rc = parse_color_name(tests[i], &elem, err);
    TEST_CHECK(rc == MUTT_CMD_SUCCESS);
  }

  rc = parse_color_name("junk", &elem, err);
  TEST_CHECK(rc == MUTT_CMD_WARNING);

  buf_pool_release(&err);
}
