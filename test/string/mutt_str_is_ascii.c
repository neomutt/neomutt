/**
 * @file
 * Test code for mutt_str_is_ascii()
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

struct IsAsciiTest
{
  const char *str;
  int len;
  bool result;
};

void test_mutt_str_is_ascii(void)
{
  // bool mutt_str_is_ascii(const char *p, size_t len);

  // clang-format off
  struct IsAsciiTest ascii_tests[] =
  {
    { NULL,    10, true },
    { "apple", 0,  true },
    { "",      10, true },
    { "apple", 5,  true },

    { "\200apple", 6,  false },
    { "ap\200ple", 6,  false },
    { "apple\200", 6,  false },
    { "apple\200", 5,  true },
  };
  // clang-format on

  {
    for (size_t i = 0; i < mutt_array_size(ascii_tests); i++)
    {
      struct IsAsciiTest *t = &ascii_tests[i];
      TEST_CASE_("%s", NONULL(t->str));
      bool result = mutt_str_is_ascii(t->str, t->len);
      TEST_CHECK(result == t->result);
    }
  }
}
