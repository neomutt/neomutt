/**
 * @file
 * Test code for mutt_mb_width()
 *
 * @authors
 * Copyright (C) 2019-2023 Richard Russon <rich@flatcap.org>
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

struct Test
{
  const char *str;
  int col;
  int len;
};

const char *test_name(const char *str)
{
  if (!str)
    return "[NULL]";
  if (!*str)
    return "[empty]";
  return str;
}

void test_mutt_mb_width(void)
{
  // int mutt_mb_width(const char *str, int col, bool indent);

  {
    const char *str = "\377\377\377\377";

    int len = mutt_mb_width(str, 0, false);
    TEST_CHECK(len == 4);
    TEST_MSG("Expected: %d", 4);
    TEST_MSG("Actual:   %d", len);
  }

  {
    static const struct Test tests[] = {
      // clang-format off
      { NULL,         0,  0 },
      { "",           0,  0 },
      { "apple",      0,  5 },
      { "Ελληνικά",   0,  8 },
      { "Українська", 0, 10 },
      { "한국어",     0,  6 },
      { "Русский",    0,  7 },
      { "日本語",     0,  6 },
      { "中文",       0,  4 },
      // clang-format on
    };

    for (int i = 0; i < mutt_array_size(tests); i++)
    {
      TEST_CASE(test_name(tests[i].str));
      int len = mutt_mb_width(tests[i].str, tests[i].col, false);
      TEST_CHECK(len == tests[i].len);
      TEST_MSG("Expected: %d", tests[i].len);
      TEST_MSG("Actual:   %d", len);
    }
  }

  {
    static const struct Test tests[] = {
      // clang-format off
      { "xxx",     0,    3 },
      { "\txxx",   0,   11 },
      { "\txxx",   1,   10 },
      { "\txxx",   2,    9 },
      { "\txxx",   3,    8 },
      { "\txxx",   4,    7 },
      { "\txxx",   5,    6 },
      { "\txxx",   6,    5 },
      { "\txxx",   7,    4 },
      { "\txxx",   8,   11 },
      // clang-format on
    };

    for (int i = 0; i < mutt_array_size(tests); i++)
    {
      TEST_CASE(test_name(tests[i].str));
      int len = mutt_mb_width(tests[i].str, tests[i].col, false);
      TEST_CHECK(len == tests[i].len);
      TEST_MSG("Expected: %d", tests[i].len);
      TEST_MSG("Actual:   %d", len);
    }
  }

  {
    static const struct Test tests[] = {
      // clang-format off
      { "xxx",       0,     3 },
      { "xxx\nyyy",  0,     7 },
      { "xxx\n yyy", 0,    15 },
      // clang-format on
    };

    for (int i = 0; i < mutt_array_size(tests); i++)
    {
      TEST_CASE(test_name(tests[i].str));
      int len = mutt_mb_width(tests[i].str, tests[i].col, true);
      TEST_CHECK(len == tests[i].len);
      TEST_MSG("Expected: %d", tests[i].len);
      TEST_MSG("Actual:   %d", len);
    }
  }
}
