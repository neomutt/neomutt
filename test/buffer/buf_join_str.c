/**
 * @file
 * Test code for buf_join_str()
 *
 * @authors
 * Copyright (C) 2024 Dennis Schön <mail@dennis-schoen.de>
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

struct AppendTest
{
  const char *orig;
  const char *str;
  char sep;
  const char *result;
};

void test_buf_join_str(void)
{
  // void buf_join_str(struct Buffer *buf, const char *str, int sep);

  // clang-format off
  static const struct AppendTest append_tests[] =
  {
    { "",       NULL,     '/',  ""             },
    { NULL,     "banana", '/',  ""             },
    { "banana", "",       '/',  "banana"       },
    { "banana", NULL,     '/',  "banana"       },
    { "",       "banana", '/',  "banana"       },
    { "apple",  "banana", '/',  "apple/banana" },
    { "",       NULL,     ' ',  ""             },
    { NULL,     "banana", ' ',  ""             },
    { "banana", "",       ' ',  "banana"       },
    { "banana", NULL,     ' ',  "banana"       },
    { "",       "banana", ' ',  "banana"       },
    { "apple",  "banana", ' ',  "apple banana" },
  };
  // clang-format on

  {
    for (size_t i = 0; i < mutt_array_size(append_tests); i++)
    {
      const struct AppendTest *t = &append_tests[i];
      struct Buffer *buf = NULL;
      if (t->orig)
        buf = buf_new(t->orig);

      TEST_CASE_("\"%s\", \"%s\", '%c'", buf_string(buf), t->str, t->sep);
      buf_join_str(buf, t->str, t->sep);
      TEST_CHECK_STR_EQ(buf_string(buf), t->result);
      buf_free(&buf);
    }
  }
}
