/**
 * @file
 * Test code for mutt_str_atoui()
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

struct TestValue
{
  const char *str;      ///< String to test
  int retval;           ///< Expected return value
  unsigned long result; ///< Expected result (outparam)
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

  { "4294967293",  0,    4294967293 },
  { "4294967294",  0,    4294967294 },
  { "4294967295",  0,    4294967295 },

  // Out of range tests
#if LONG_IS_64
  { "4294967296",  -2,   0 },
  { "4294967297",  -2,   0 },
  { "4294967298",  -2,   0 },
#else
  { "4294967296",  -1,   0 },
  { "4294967297",  -1,   0 },
  { "4294967298",  -1,   0 },
#endif
  { "18446744073709551616", -1, 0 },

  // Invalid tests
#if LONG_IS_64
  { "-3",          -2,   0 },
  { " -3",         -2,   0 },
  { "  -3",        -2,   0 },
#else
  { "-3",          0,   4294967293 },
  { " -3",         0,   4294967293 },
  { "  -3",        0,   4294967293 },
#endif
  { "abc",         1,    0 },
  { "a123",        1,    0 },
  { "a-123",       1,    0 },
  { "0a",          1,    0 },
  { "123a",        1,    123 },
  { "1,234",       1,    1 },
  { "1.234",       1,    1 },
  { ".123",        1,    0 },
  { "3 ",          1,    3 },
};
// clang-format on

static const int UNEXPECTED = -9999;

void test_mutt_str_atoui(void)
{
  // int mutt_str_atoui(const char *str, unsigned int *dst);

  unsigned int result = UNEXPECTED;

  // Degenerate tests
  TEST_CHECK(mutt_str_atoui(NULL, &result) == NULL);
  TEST_CHECK(result == 0);
  TEST_CHECK(mutt_str_atoui("42", NULL) != NULL);

  // Normal tests
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    TEST_CASE(tests[i].str);

    result = UNEXPECTED;
    const char *end = mutt_str_atoui(tests[i].str, &result);

    if ((tests[i].retval == 0) && (!end || *end))
    {
      TEST_MSG("retval: Expected: \\0, Got: %s", end);
    }
    else if ((tests[i].retval == -1) && end)
    {
      TEST_MSG("retval: Expected: NULL, Got: %s", end);
    }
    else if ((tests[i].retval == -2) && end)
    {
      TEST_MSG("retval: Expected: NULL, Got: %s", end);
    }
  }
}
