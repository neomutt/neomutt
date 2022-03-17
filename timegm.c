/**
 * @file
 * For systems lacking timegm()
 *
 * @authors
 * Copyright (C) 2022 Claes Nästén <pekdon@gmail.com>
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

/**
 * @page neo_timegm For systems lacking timegm()
 *
 * For systems lacking timegm()
 */

#include "config.h"
#include <time.h>
#include <stdbool.h>

/**
 * is_leap_year - Is this a Leap Year?
 * @param year Year
 * @retval bool true if it is a leap year
 */
static bool is_leap_year(int year)
{
  return (year % 400 == 0) || (year % 4 == 0 && year % 100 != 0);
}

/**
 * tm_to_sec - Convert a time structure to number of seconds since the epoch
 * @param tm Time structure
 * @retval num Seconds since the epoch
 */
static time_t tm_to_sec(struct tm *tm)
{
  time_t val = tm->tm_sec + (tm->tm_min * 60) + (tm->tm_hour * 3600) + (tm->tm_yday * 86400);

  int year = 1900 + tm->tm_year;
  for (int i = 1970; i < year; i++)
  {
    if (is_leap_year(i))
      val += (366 * 86400);
    else
      val += (365 * 86400);
  }

  return val;
}

/**
 * timegm - Convert struct tm to time_t seconds since epoch
 * @param tm Time to convert to seconds since epoch
 * @retval num Seconds since epoch
 */
time_t timegm(struct tm *tm)
{
  return tm_to_sec(tm);
}
