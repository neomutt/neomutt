/**
 * @file
 * Test code for mutt_str_next_word()
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
#include "mutt/lib.h"

struct NextWordTest
{
  const char *str;
  size_t result;
};

void test_mutt_str_next_word(void)
{
  // const char *mutt_str_next_word(const char *s);

  // clang-format off
  struct NextWordTest word_tests[] =
  {
    { NULL,           0 },
    { "",             0 },
    { "apple",        5 },
    { "apple banana", 6 },
    { "apple ",       6 },
    { " apple",       1 },
  };
  // clang-format on

  {
    for (size_t i = 0; i < mutt_array_size(word_tests); i++)
    {
      struct NextWordTest *t = &word_tests[i];
      TEST_CASE_("%ld, '%s'", i, NONULL(t->str));
      const char *result = mutt_str_next_word(t->str);
      TEST_CHECK(result == (t->str + t->result));
    }
  }
}
