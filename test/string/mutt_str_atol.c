/**
 * @file
 * Test code for mutt_str_atol()
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
#include "acutest.h"
#include "config.h"
#include <limits.h>
#include "mutt/mutt.h"

struct TestValue
{
  const char *str; ///< String to test
  int retval;      ///< Expected return value
  long result;     ///< Expected result (outparam)
};

// clang-format off
static const struct TestValue tests[] = {
  // Valid tests
  { "0",                    0,  0 },
  { "1",                    0,  1 },
  { "2",                    0,  2 },
  { "3",                    0,  3 },
  { " 3",                   0,  3 },
  { " 3",                   0,  3 },

  { "9223372036854775805",  0,  9223372036854775805 },
  { "9223372036854775806",  0,  9223372036854775806 },
  { "9223372036854775807",  0,  LONG_MAX },

  { "-1",                   0,  -1 },
  { "-2",                   0,  -2 },
  { "-3",                   0,  -3 },
  { " -3",                  0,  -3 },
  { " -3",                  0,  -3 },

  { "-9223372036854775806", 0,  -9223372036854775806 },
  { "-9223372036854775807", 0,  -9223372036854775807 },
  { "-9223372036854775808", 0,  LONG_MIN },

  // Out of range tests
  { "9223372036854775808",  -2, LONG_MAX },
  { "9223372036854775809",  -2, LONG_MAX },
  { "9223372036854775810",  -2, LONG_MAX },

  { "-9223372036854775809", -2, LONG_MIN },
  { "-9223372036854775810", -2, LONG_MIN },
  { "-9223372036854775811", -2, LONG_MIN },

  // Invalid tests
  { "abc",                  -1, 0 },
  { "a123",                 -1, 0 },
  { "a-123",                -1, 0 },
  { "0a",                   -1, 0 },

  { "123a",                 -1, 123 },
  { "-123a",                -1, -123 },

  { "1,234",                -1, 1 },
  { "-1,234",               -1, -1 },
  { "1.234",                -1, 1 },
  { "-1.234",               -1, -1 },

  { ".123",                 -1, 0 },
  { "-.123",                -1, 0 },
  { "3 ",                   -1, 3 },
  { "-3 ",                  -1, -3 },
};
// clang-format on

static const long UNEXPECTED = -9999;

void test_mutt_str_atol(void)
{
  // int mutt_str_atol(const char *str, long *dst);

  long result = UNEXPECTED;
  int retval = 0;

  // Degenerate tests
  TEST_CHECK(mutt_str_atol(NULL, &result) == 0);
  TEST_CHECK(mutt_str_atol("42", NULL) == 0);

  // Normal tests
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    result = UNEXPECTED;
    retval = mutt_str_atol(tests[i].str, &result);

    const bool success = (retval == tests[i].retval) && (result == tests[i].result);
    if (!TEST_CHECK_(success, "Testing '%s'\n", tests[i].str))
    {
      TEST_MSG("retval: Expected: %d, Got: %d\n", tests[i].retval, retval);
      TEST_MSG("result: Expected: %ld, Got: %ld\n", tests[i].result, result);
    }
  }
}
