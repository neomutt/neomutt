/**
 * @file
 * Test code for mutt_path_parent()
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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
#include "test_common.h"

void test_mutt_path_parent(void)
{
  // bool mutt_path_parent(char *buf);

  static const char *tests[][2] = {
    // clang-format off
    { "/apple",         "/"      },
    { "/apple/",        "/"      },
    { "/apple/banana",  "/apple" },
    { "/apple/banana/", "/apple" },
    // clang-format on
  };

  {
    TEST_CHECK(!mutt_path_parent(NULL));
  }

  {
    struct Buffer *path = buf_new(NULL);
    TEST_CHECK(!mutt_path_parent(path));
    buf_free(&path);
  }

  {
    struct Buffer *path = buf_new("/");
    TEST_CHECK(!mutt_path_parent(path));
    buf_free(&path);
  }

  {
    for (size_t i = 0; i < mutt_array_size(tests); i++)
    {
      TEST_CASE(tests[i][0]);

      struct Buffer *path = buf_new(tests[i][0]);

      TEST_CHECK(mutt_path_parent(path));
      const char *expected = tests[i][1];
      TEST_CHECK_STR_EQ(buf_string(path), expected);

      buf_free(&path);
    }
  }
}
