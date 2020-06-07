/**
 * @file
 * Test code for mutt_date_make_time()
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
#include <stdlib.h>
#include "mutt/lib.h"

struct MakeTimeTest
{
  struct tm tm;
  time_t expected;
};

void test_mutt_date_make_time(void)
{
  // time_t mutt_date_make_time(struct tm *t, bool local);

  setenv("TZ", "UTC", 1);

  {
    TEST_CHECK(mutt_date_make_time(NULL, false) != 0);
  }

  // clang-format off
  struct MakeTimeTest time_tests[] = {
    { { 0,  0,  0,  1,  0,  100,   0 }, 946684800 },
    { { -1, 0,  0,  1,  0,  100,   0 }, TIME_T_MIN },
    { { 61, 0,  0,  1,  0,  100,   0 }, TIME_T_MIN },
    { { 0,  -1, 0,  1,  0,  100,   0 }, TIME_T_MIN },
    { { 0,  60, 0,  1,  0,  100,   0 }, TIME_T_MIN },
    { { 0,  0,  -1, 1,  0,  100,   0 }, TIME_T_MIN },
    { { 0,  0,  24, 1,  0,  100,   0 }, TIME_T_MIN },
    { { 0,  0,  0,  0,  0,  100,   0 }, TIME_T_MIN },
    { { 0,  0,  0,  32, 0,  100,   0 }, TIME_T_MIN },
    { { 0,  0,  0,  1,  0,  10001, 0 }, TIME_T_MAX },
    { { 0,  0,  0,  1,  0, -10001, 0 }, TIME_T_MIN },
  };
  // clang-format on

  {
    for (size_t i = 0; i < mutt_array_size(time_tests); i++)
    {
      struct tm *tm = &time_tests[i].tm;

      TEST_CASE_("{%d,%d,%d,%d,%d,%d,%d} = %ld", tm->tm_sec, tm->tm_min,
                 tm->tm_hour, tm->tm_mday, tm->tm_mon, tm->tm_year, tm->tm_wday,
                 time_tests[i].expected);

      time_t result = mutt_date_make_time(&time_tests[i].tm, false);
      TEST_CHECK(result == time_tests[i].expected);
    }
  }

  {
    struct tm tm = { 0, 0, 0, 1, 0, 100, 0 };
    time_t result = mutt_date_make_time(&tm, true);
    TEST_CHECK(result == 946684800);
  }
}
