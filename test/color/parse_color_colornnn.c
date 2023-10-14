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

enum CommandResult parse_color_colornnn(const char *s, struct ColorElement *elem,
                                        struct Buffer *err);

void test_parse_color_colornnn(void)
{
  // enum CommandResult parse_color_colornnn(const char *s, struct ColorElement *elem, struct Buffer *err);

  {
    enum CommandResult rc;
    const char *str = "color123";
    struct ColorElement elem = { 0 };
    struct Buffer *err = buf_pool_get();

    rc = parse_color_colornnn(NULL, &elem, err);
    TEST_CHECK(rc == MUTT_CMD_ERROR);

    rc = parse_color_colornnn(str, NULL, err);
    TEST_CHECK(rc == MUTT_CMD_ERROR);

    buf_pool_release(&err);
  }

  {
    enum CommandResult rc;
    struct Buffer *err = buf_pool_get();

    for (int i = 0; i < 256; i++)
    {
      char str[32] = { 0 };
      struct ColorElement elem = { 0 };

      snprintf(str, sizeof(str), "color%d", i);

      rc = parse_color_colornnn(str, &elem, err);
      TEST_CHECK(rc == MUTT_CMD_SUCCESS);

      TEST_CHECK(elem.type == CT_PALETTE);
      TEST_CHECK(elem.color == i);
      TEST_CHECK(elem.prefix == COLOR_PREFIX_NONE);
    }

    buf_pool_release(&err);
  }

  {
    enum CommandResult rc;
    struct Buffer *err = buf_pool_get();

    for (int i = 0; i < 256; i++)
    {
      char str[32] = { 0 };
      struct ColorElement elem = { 0 };

      snprintf(str, sizeof(str), "brightCOLOR%d", i);

      rc = parse_color_colornnn(str, &elem, err);
      TEST_CHECK(rc == MUTT_CMD_SUCCESS);

      TEST_CHECK(elem.type == CT_PALETTE);
      TEST_CHECK(elem.color == i);
      TEST_CHECK(elem.prefix == COLOR_PREFIX_BRIGHT);
    }

    buf_pool_release(&err);
  }

  {
    enum CommandResult rc;
    struct Buffer *err = buf_pool_get();
    const char *tests[] = { "red",       "color",    "colour123", "color-1",
                            "colorblue", "color256", "color1000", NULL };

    for (int i = 0; tests[i]; i++)
    {
      struct ColorElement elem = { 0 };

      rc = parse_color_colornnn(tests[i], &elem, err);
      TEST_CHECK(rc < 0);
      TEST_MSG("Case: %s", tests[i]);
    }

    buf_pool_release(&err);
  }
}
