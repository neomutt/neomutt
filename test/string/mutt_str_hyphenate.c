/**
 * @file
 * Test code for mutt_str_hyphenate()
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
#include "test_common.h"

struct HyphenTest
{
  const char *input;
  const char *expected;
};

void test_mutt_str_hyphenate(void)
{
  // void mutt_str_hyphenate(char *buf, size_t buflen, const char *str);

  static const struct HyphenTest tests[] = {
    // clang-format off
    { "",             ""             },
    { "apple",        "apple"        },
    { "apple_banana", "apple-banana" },
    { "a_b_c",        "a-b-c"        },
    { "_apple",       "-apple"       },
    { "__apple",      "--apple"      },
    { "apple_",       "apple-"       },
    { "apple__",      "apple--"      },
    { "_",            "-"            },
    { "__",           "--"           },
    { "___",          "---"          },
    // clang-format on
  };

  {
    char result[128] = { 0 };
    mutt_str_hyphenate(NULL, sizeof(result), "apple");
    mutt_str_hyphenate(result, 0, "apple");
    mutt_str_hyphenate(result, sizeof(result), NULL);
  }

  {
    // buffer too small
    char result[10] = { 0 };
    mutt_str_hyphenate(result, sizeof(result), "apple_banana_cherry");
    TEST_CHECK_STR_EQ(result, "apple-ban");
  }

  {
    char result[128];

    for (size_t i = 0; i < mutt_array_size(tests); i++)
    {
      memset(result, 0, sizeof(result));
      TEST_CASE(tests[i].input);
      mutt_str_hyphenate(result, sizeof(result), tests[i].input);
      TEST_CHECK_STR_EQ(result, tests[i].expected);
    }
  }
}
