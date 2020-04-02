/**
 * @file
 * Test code for mutt_str_atoi()
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

struct TestValue
{
  const char *str; ///< String to test
  int retval;      ///< Expected return value
  long result;     ///< Expected result (outparam)
};

// clang-format off
static const struct TestValue tests[] = {
  // Valid tests
  { "0",           0,    0 },
  { "1",           0,    1 },
  { "2",           0,    2 },
  { "3",           0,    3 },
  { " 3",          0,    3 },
  { "  3",         0,    3 },

  { "2147483645",  0,    2147483645 },
  { "2147483646",  0,    2147483646 },
  { "2147483647",  0,    2147483647 },

  { "-1",          0,    -1 },
  { "-2",          0,    -2 },
  { "-3",          0,    -3 },
  { " -3",         0,    -3 },
  { "  -3",        0,    -3 },

  { "-2147483646", 0,    -2147483646 },
  { "-2147483647", 0,    -2147483647 },
  { "-2147483648", 0,    -2147483648 },

  // Out of range tests
  { "2147483648",  -2,   0 },
  { "2147483649",  -2,   0 },
  { "2147483650",  -2,   0 },

  { "-2147483649", -2,   0 },
  { "-2147483650", -2,   0 },
  { "-2147483651", -2,   0 },

  // Invalid tests
  { "abc",         -1,   0 },
  { "a123",        -1,   0 },
  { "a-123",       -1,   0 },
  { "0a",          -1,   0 },
  { "123a",        -1,   0 },
  { "-123a",       -1,   0 },
  { "1,234",       -1,   0 },
  { "-1,234",      -1,   0 },
  { "1.234",       -1,   0 },
  { "-1.234",      -1,   0 },
  { ".123",        -1,   0 },
  { "-.123",       -1,   0 },
  { "3 ",          -1,   0 },
  { "-3 ",         -1,   0 },
};
// clang-format on

static const int UNEXPECTED = -9999;

void test_mutt_str_atoi(void)
{
  // int mutt_str_atoi(const char *str, int *dst);

  int result = UNEXPECTED;
  int retval = 0;

  // Degenerate tests
  TEST_CHECK(mutt_str_atoi(NULL, &result) == 0);
  TEST_CHECK(mutt_str_atoi("42", NULL) == 0);

  // Normal tests
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    TEST_CASE(tests[i].str);

    result = UNEXPECTED;
    retval = mutt_str_atoi(tests[i].str, &result);

    if (!TEST_CHECK((retval == tests[i].retval) && (result == tests[i].result)))
    {
      TEST_MSG("retval: Expected: %d, Got: %d\n", tests[i].retval, retval);
      TEST_MSG("result: Expected: %ld, Got: %ld\n", tests[i].result, result);
    }
  }
}
