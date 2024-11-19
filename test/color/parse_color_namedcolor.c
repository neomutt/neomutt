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
#include "mutt/lib.h"
#include "core/lib.h"
#include "color/lib.h"
#include "test_common.h"

enum CommandResult parse_color_namedcolor(const char *s, struct ColorElement *elem,
                                          struct Buffer *err);

struct NamedTest
{
  const char *str;
  int cid;
};

void test_parse_color_namedcolor(void)
{
  // enum CommandResult parse_color_namedcolor(const char *s, struct ColorElement *elem, struct Buffer *err);

  {
    enum CommandResult rc;
    const char *str = "red";
    struct ColorElement elem = { 0 };
    struct Buffer *err = buf_pool_get();

    rc = parse_color_namedcolor(NULL, &elem, err);
    TEST_CHECK_NUM_EQ(rc, MUTT_CMD_ERROR);

    rc = parse_color_namedcolor(str, NULL, err);
    TEST_CHECK_NUM_EQ(rc, MUTT_CMD_ERROR);

    buf_pool_release(&err);
  }

  {
    struct NamedTest tests[] = {
      // clang-format off
      { "default", -1 },
      { "black",    0 },
      { "blue",     4 },
      { "cyan",     6 },
      { "green",    2 },
      { "magenta",  5 },
      { "red",      1 },
      { "white",    7 },
      { "yellow",   3 },
      { NULL,       0 },
      // clang-format on
    };

    struct Buffer *err = buf_pool_get();

    for (int i = 0; tests[i].str; i++)
    {
      enum CommandResult rc;
      struct ColorElement elem = { 0 };

      TEST_CASE(tests[i].str);
      rc = parse_color_namedcolor(tests[i].str, &elem, err);
      TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
      TEST_MSG("%s", buf_string(err));

      TEST_CHECK(elem.color == tests[i].cid);
      TEST_MSG("cid: Expected %d, Got %d\n", tests[i].cid, elem.color);

      TEST_CHECK_NUM_EQ(elem.type, CT_SIMPLE);

      TEST_CHECK_NUM_EQ(elem.prefix, COLOR_PREFIX_NONE);
    }

    buf_pool_release(&err);
  }

  {
    struct NamedTest tests[] = {
      // clang-format off
      { "lightblack",    0 },
      { "lightblue",     4 },
      { "lightcyan",     6 },
      { "lightgreen",    2 },
      { "lightmagenta",  5 },
      { "lightred",      1 },
      { "lightwhite",    7 },
      { "lightyellow",   3 },
      { NULL,       0 },
      // clang-format on
    };

    struct Buffer *err = buf_pool_get();

    for (int i = 0; tests[i].str; i++)
    {
      enum CommandResult rc;
      struct ColorElement elem = { 0 };

      TEST_CASE(tests[i].str);
      rc = parse_color_namedcolor(tests[i].str, &elem, err);
      TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
      TEST_MSG("%s", buf_string(err));

      TEST_CHECK(elem.color == tests[i].cid);
      TEST_MSG("cid: Expected %d, Got %d\n", tests[i].cid, elem.color);

      TEST_CHECK_NUM_EQ(elem.type, CT_SIMPLE);

      TEST_CHECK_NUM_EQ(elem.prefix, COLOR_PREFIX_LIGHT);
    }

    buf_pool_release(&err);
  }

  {
    const char *tests[] = { "blacklight", "brown", "blac", "lightdefault", NULL };

    enum CommandResult rc;
    struct ColorElement elem = { 0 };

    for (int i = 0; tests[i]; i++)
    {
      TEST_CASE(tests[i]);
      rc = parse_color_namedcolor(tests[i], &elem, NULL);
      TEST_CHECK_NUM_EQ(rc, MUTT_CMD_WARNING);
    }
  }
}
