/**
 * @file
 * Test code for mutt_path_concat()
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

void test_mutt_path_concat(void)
{
  // char *mutt_path_concat(char *dest, const char *dir, const char *file, size_t dlen);

  static const char *tests[][3] = {
    // clang-format off
    { NULL,     "",       ""             },
    { NULL,     "banana", "banana"       },
    { "",       NULL,     ""             },
    { "",       "",       ""             },
    { "",       "banana", "banana"       },
    { "apple",  NULL,     "apple"        },
    { "apple",  "",       "apple"        },
    { "apple",  "banana", "apple/banana" },
    { "apple/", NULL,     "apple/"       },
    { "apple/", "",       "apple/"       },
    { "apple/", "banana", "apple/banana" },
    // clang-format on
  };

  {
    char buf[64] = { 0 };
    TEST_CHECK(mutt_path_concat(NULL, "apple", "banana", sizeof(buf)) == NULL);
  }

  {
    char buf[64] = { 0 };
    TEST_CHECK(mutt_path_concat(buf, NULL, NULL, sizeof(buf)) == NULL);
  }

  {
    for (int i = 0; i < mutt_array_size(tests); i++)
    {
      char buf[64] = { 0 };

      const char *dir = tests[i][0];
      const char *file = tests[i][1];
      const char *expected = tests[i][2];

      TEST_CHECK(mutt_path_concat(buf, dir, file, sizeof(buf)) == buf);

      TEST_CHECK_STR_EQ(buf, expected);
    }
  }
}
