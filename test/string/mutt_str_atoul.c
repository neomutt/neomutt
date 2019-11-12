/**
 * @file
 * Test code for mutt_str_atoul()
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
  const char *str;      ///< String to test
  int retval;           ///< Expected return value
  unsigned long result; ///< Expected result (outparam)
};

// clang-format off
static const struct TestValue tests[] = {
  // Valid tests
  { "0",   0, 0 },
  { "1",   0, 1 },
  { "2",   0, 2 },
  { "3",   0, 3 },
  { " 3",  0, 3 },
  { "  3", 0, 3 },

#if (__WORDSIZE == 64)
  { "18446744073709551613",  0, 18446744073709551613UL },
  { "18446744073709551614",  0, 18446744073709551614UL },
  { "18446744073709551615",  0, 18446744073709551615UL },
#else
  { "4294967293",            0, 4294967293UL },
  { "4294967294",            0, 4294967294UL },
  { "4294967295",            0, 4294967295UL },
#endif

#if (__WORDSIZE == 64)
  // Out of range tests
  { "18446744073709551616", -1, ULONG_MAX },
  { "18446744073709551617", -1, ULONG_MAX },
  { "18446744073709551618", -1, ULONG_MAX },
#else
  // Out of range tests
  { "4294967296",           -1, ULONG_MAX },
  { "4294967297",           -1, ULONG_MAX },
  { "4294967298",           -1, ULONG_MAX },
#endif

  // Invalid tests
#if (__WORDSIZE == 64)
  { "-3",          0,    18446744073709551613UL },
  { " -3",         0,    18446744073709551613UL },
  { "  -3",        0,    18446744073709551613UL },
#else
  { "-3",          0,    4294967293UL },
  { " -3",         0,    4294967293UL },
  { "  -3",        0,    4294967293UL },
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

void test_mutt_str_atoul(void)
{
  // int mutt_str_atoul(const char *str, unsigned long *dst);

  unsigned long result = UNEXPECTED;
  int retval = 0;

  // Degenerate tests
  TEST_CHECK(mutt_str_atoul(NULL, &result) == 0);
  TEST_CHECK(mutt_str_atoul("42", NULL) == 0);

  // Normal tests
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    result = UNEXPECTED;
    retval = mutt_str_atoul(tests[i].str, &result);

    const bool success = (retval == tests[i].retval) && (result == tests[i].result);
    if (!TEST_CHECK_(success, "Testing '%s'\n", tests[i].str))
    {
      TEST_MSG("retval: Expected: %d, Got: %d\n", tests[i].retval, retval);
      TEST_MSG("result: Expected: %lu, Got: %lu\n", tests[i].result, result);
    }
  }
}
