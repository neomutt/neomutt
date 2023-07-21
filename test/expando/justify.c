/**
 * @file
 * Test text justification
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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
#include "debug/lib.h"
#include "expando/lib.h"
#include "common.h" // IWYU pragma: keep
#include "test_common.h"

void buf_justify(struct Buffer *buf, enum FormatJustify justify, size_t cols, char pad_char);

struct TestCase
{
  char *str;
  int justify;
  int cols;
  char *expected;
};

void test_expando_justify(void)
{
  // void buf_justify(struct Buffer *buf, enum FormatJustify justify, size_t cols, char pad_char);

  static const char pad_char = '.';
  static const struct TestCase tests[] = {
    // clang-format off
    { "apple", JUSTIFY_LEFT,    0, "apple" },
    { "apple", JUSTIFY_LEFT,    4, "apple" },
    { "apple", JUSTIFY_LEFT,    5, "apple" },
    { "apple", JUSTIFY_LEFT,    6, "apple." },
    { "apple", JUSTIFY_LEFT,    7, "apple.." },
    { "apple", JUSTIFY_LEFT,   10, "apple....." },

    { "apple", JUSTIFY_RIGHT,   0, "apple" },
    { "apple", JUSTIFY_RIGHT,   4, "apple" },
    { "apple", JUSTIFY_RIGHT,   5, "apple" },
    { "apple", JUSTIFY_RIGHT,   6, ".apple" },
    { "apple", JUSTIFY_RIGHT,   7, "..apple" },
    { "apple", JUSTIFY_RIGHT,  10, ".....apple" },

    { "apple", JUSTIFY_CENTER,  0, "apple" },
    { "apple", JUSTIFY_CENTER,  4, "apple" },
    { "apple", JUSTIFY_CENTER,  5, "apple" },
    { "apple", JUSTIFY_CENTER,  6, "apple." },
    { "apple", JUSTIFY_CENTER,  7, ".apple." },
    { "apple", JUSTIFY_CENTER, 10, "..apple..." },
    // clang-format off
  };

  buf_justify(NULL, JUSTIFY_LEFT, 0, 'x');

  {
    struct Buffer *buf = buf_pool_get();
    buf_justify(buf, JUSTIFY_LEFT, 10, '\0');
    buf_pool_release(&buf);
  }

  {
    struct Buffer *buf = buf_pool_get();
    buf_addstr(buf, "hello-world");
    buf_justify(buf, JUSTIFY_LEFT, 5, 'X');
    buf_pool_release(&buf);
  }

  {
    struct Buffer *buf = buf_pool_get();

    for (size_t i = 0; i < mutt_array_size(tests); i++)
    {
      const struct TestCase *test = &tests[i];
      buf_reset(buf);
      buf_addstr(buf, test->str);

      TEST_CASE_("'%s', %s, %d", test->str, name_format_justify(test->justify) + 8, test->cols);

      buf_justify(buf, test->justify, test->cols, pad_char);

      TEST_CHECK_STR_EQ(buf_string(buf), test->expected);
    }

    buf_pool_release(&buf);
  }
}
