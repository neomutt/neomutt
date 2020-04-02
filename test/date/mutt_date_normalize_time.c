/**
 * @file
 * Test code for mutt_date_normalize_time()
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
#include <stdbool.h>
#include "mutt/lib.h"

struct NormalizeTest
{
  struct tm date;
  struct tm expected;
};

static bool tm_cmp(const struct tm *first, const struct tm *second)
{
  if (!first || !second)
    return false;
  return ((first->tm_sec == second->tm_sec) && (first->tm_min == second->tm_min) &&
          (first->tm_hour == second->tm_hour) && (first->tm_mday == second->tm_mday) &&
          (first->tm_mon == second->tm_mon) && (first->tm_year == second->tm_year));
}

void test_mutt_date_normalize_time(void)
{
  // void mutt_date_normalize_time(struct tm *tm);

  {
    mutt_date_normalize_time(NULL);
    TEST_CHECK_(1, "mutt_date_normalize_time(NULL)");
  }

  // clang-format off
  struct NormalizeTest time_tests[] = {
    // Sec Min Hour Day Mon Year       Sec Min Hour Day Mon Year
    { { 0,  0,  0,  1,  0,  100, 0 }, { 0,  0,  0,  1,  0,  100, 0 } },
    { { -1, 0,  0,  1,  0,  100, 0 }, { 59, 59, 23, 31, 11, 99,  0 } },
    { { 60, 59, 23, 31, 11, 99,  0 }, { 0,  0,  0,  1,  0,  100, 0 } },
    { { -1, 0,  0,  1,  0,  100, 0 }, { 59, 59, 23, 31, 11, 99,  0 } },
    { { 60, 59, 23, 31, 11, 99,  0 }, { 0,  0,  0,  1,  0,  100, 0 } },
    { { 0,  0,  0,  1,  -1, 100, 0 }, { 0,  0,  0,  1,  11, 99,  0 } },
    { { 0,  0,  0,  1,  12, 99,  0 }, { 0,  0,  0,  1,  0,  100, 0 } },
    { { 0,  0,  0,  -1, 6,  100, 0 }, { 0,  0,  0,  29, 5,  100, 0 } },
    { { 0,  0,  0,  42, 1,  100, 0 }, { 0,  0,  0,  13, 2,  100, 0 } },
  };
  // clang-format on

  {
    for (size_t i = 0; i < mutt_array_size(time_tests); i++)
    {
      struct tm *date = &time_tests[i].date;
      struct tm *expected = &time_tests[i].expected;

      TEST_CASE_("Date     {%d,%d,%d,%d,%d,%d}", date->tm_sec, date->tm_min,
                 date->tm_hour, date->tm_mday, date->tm_mon, date->tm_year);

      TEST_CASE_("Expected {%d,%d,%d,%d,%d,%d}", expected->tm_sec,
                 expected->tm_min, expected->tm_hour, expected->tm_mday,
                 expected->tm_mon, expected->tm_year);

      mutt_date_normalize_time(date);

      TEST_CASE_("Got      {%d,%d,%d,%d,%d,%d}", date->tm_sec, date->tm_min,
                 date->tm_hour, date->tm_mday, date->tm_mon, date->tm_year);
      TEST_CHECK(tm_cmp(date, expected));
    }
  }
}
