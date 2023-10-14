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

void test_parse_color_pair(void)
{
  // enum CommandResult parse_color_pair(struct Buffer *buf, struct Buffer *s, struct AttrColor *ac, struct Buffer *err);

  struct Buffer *buf = buf_pool_get();
  struct Buffer *s = buf_pool_get();
  struct Buffer *err = buf_pool_get();

  struct AttrColor *ac = attr_color_new();

  const char *first[] = { "blue", "color86", "#BB2288", NULL };
  const char *second[] = { "brightyellow", "alertcolor86", "#4F8E3A", NULL };

  for (int i = 0; first[i]; i++)
  {
    for (int j = 0; second[j]; j++)
    {
      buf_printf(s, "%s %s", first[i], second[j]);
      buf_seek(s, 0);

      enum CommandResult rc = parse_color_pair(buf, s, ac, err);
      TEST_CHECK(rc == MUTT_CMD_SUCCESS);
      TEST_MSG("%s\n", buf_string(err));
    }
  }

  const char *tests[] = {
    "", "red", "underline red", "normal yellow", "bold junk", "'' red", NULL
  };

  for (int i = 0; tests[i]; i++)
  {
    buf_strcpy(s, tests[i]);
    buf_seek(s, 0);

    enum CommandResult rc = parse_color_pair(buf, s, ac, err);
    TEST_CHECK(rc < 0);
    TEST_MSG("%s\n", buf_string(err));
  }

  attr_color_free(&ac);

  buf_pool_release(&buf);
  buf_pool_release(&s);
  buf_pool_release(&err);
}
