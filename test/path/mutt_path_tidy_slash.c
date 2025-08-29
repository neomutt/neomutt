/**
 * @file
 * Test code for mutt_path_tidy_slash()
 *
 * @authors
 * Copyright (C) 2019-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
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
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "test_common.h"

void test_mutt_path_tidy_slash(void)
{
  // bool mutt_path_tidy_slash(char *buf);

  static const char *tests[][2] = {
    // clang-format off
    { NULL,                     NULL,            },
    { "/",                      "/",             },
    { "//",                     "/",             },
    { "///",                    "/",             },
    { "/apple/",                "/apple",        },
    { "/apple//",               "/apple",        },
    { "/apple///",              "/apple",        },
    { "/apple/banana",          "/apple/banana", },
    { "/apple//banana",         "/apple/banana", },
    { "/apple///banana",        "/apple/banana", },
    { "/apple/banana/",         "/apple/banana", },
    { "/apple/banana//",        "/apple/banana", },
    { "/apple/banana///",       "/apple/banana", },
    { "//.///././apple/banana", "/apple/banana", },
    { "/apple/.///././banana",  "/apple/banana", },
    { "/apple/banana/.///././", "/apple/banana", },
    { "/apple/banana/",         "/apple/banana", },
    { "/apple/banana/.",        "/apple/banana", },
    { "/apple/banana/./",       "/apple/banana", },
    { "/apple/banana//",        "/apple/banana", },
    { "/apple/banana//.",       "/apple/banana", },
    { "/apple/banana//./",      "/apple/banana", },
    { "////apple/banana",       "/apple/banana", },
    { "/.//apple/banana",       "/apple/banana", },
    // clang-format on
  };

  {
    TEST_CHECK(!mutt_path_tidy_slash(NULL, true));
  }

  {
    char buf[64];
    for (size_t i = 0; i < mutt_array_size(tests); i++)
    {
      TEST_CASE(tests[i][0]);

      mutt_str_copy(buf, tests[i][0], sizeof(buf));
      mutt_path_tidy_slash(buf, true);
      TEST_CHECK_STR_EQ(buf, tests[i][1]);
    }
  }
}
