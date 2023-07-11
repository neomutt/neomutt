/**
 * @file
 * Test code for mutt_path_dirname()
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

void test_mutt_path_dirname(void)
{
  // char *mutt_path_dirname(const char *path);

  {
    TEST_CHECK(mutt_path_dirname(NULL) == NULL);
  }

  static const char *tests[][2] = {
    // clang-format off
    { NULL,                   NULL,            },
    { "apple",                ".",             },
    { "/",                    "/",             },
    { "/apple",               "/",             },
    { "/apple/banana",        "/apple",        },
    { "/apple/banana/cherry", "/apple/banana", },
    // clang-format on
  };

  {
    for (int i = 0; i < mutt_array_size(tests); i++)
    {
      const char *source = tests[i][0];
      const char *expected = tests[i][1];
      TEST_CASE(source);

      const char *result = mutt_path_dirname(source);
      TEST_CHECK_STR_EQ(result, expected);
      FREE(&result);
    }
  }
}
