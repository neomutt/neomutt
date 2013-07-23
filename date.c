/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include <string.h>

/* returns the seconds east of UTC given `g' and its corresponding gmtime()
   representation */
static time_t compute_tz (time_t g, struct tm *utc)
{
  struct tm *lt = localtime (&g);
  time_t t;
  int yday;

  t = (((lt->tm_hour - utc->tm_hour) * 60) + (lt->tm_min - utc->tm_min)) * 60;

  if ((yday = (lt->tm_yday - utc->tm_yday)))
  {
    /* This code is optimized to negative timezones (West of Greenwich) */
    if (yday == -1 ||	/* UTC passed midnight before localtime */
	yday > 1)	/* UTC passed new year before localtime */
      t -= 24 * 60 * 60;
    else
      t += 24 * 60 * 60;
  }

  return t;
}

/* Returns the local timezone in seconds east of UTC for the time t,
 * or for the current time if t is zero.
 */
time_t mutt_local_tz (time_t t)
{
  struct tm *ptm;
  struct tm utc;

  if (!t)
    t = time (NULL);
  ptm = gmtime (&t);
  /* need to make a copy because gmtime/localtime return a pointer to
     static memory (grr!) */
  memcpy (&utc, ptm, sizeof (utc));
  return (compute_tz (t, &utc));
}

/* converts struct tm to time_t, but does not take the local timezone into
   account unless ``local'' is nonzero */
time_t mutt_mktime (struct tm *t, int local)
{
  time_t g;

  static const int AccumDaysPerMonth[12] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
  };

  /* Compute the number of days since January 1 in the same year */
  g = AccumDaysPerMonth [t->tm_mon % 12];

  /* The leap years are 1972 and every 4. year until 2096,
   * but this algorithm will fail after year 2099 */
  g += t->tm_mday;
  if ((t->tm_year % 4) || t->tm_mon < 2)
    g--;
  t->tm_yday = g;

  /* Compute the number of days since January 1, 1970 */
  g += (t->tm_year - 70) * 365;
  g += (t->tm_year - 69) / 4;

  /* Compute the number of hours */
  g *= 24;
  g += t->tm_hour;

  /* Compute the number of minutes */
  g *= 60;
  g += t->tm_min;

  /* Compute the number of seconds */
  g *= 60;
  g += t->tm_sec;

  if (local)
    g -= compute_tz (g, t);

  return (g);
}

/* Return 1 if month is February of leap year, else 0 */
static int isLeapYearFeb (struct tm *tm)
{
  if (tm->tm_mon == 1)
  {
    int y = tm->tm_year + 1900;
    return (((y & 3) == 0) && (((y % 100) != 0) || ((y % 400) == 0)));
  }
  return (0);
}

void mutt_normalize_time (struct tm *tm)
{
  static const char DaysPerMonth[12] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
  };
  int nLeap;

  while (tm->tm_sec < 0)
  {
    tm->tm_sec += 60;
    tm->tm_min--;
  }
  while (tm->tm_sec >= 60)
  {
    tm->tm_sec -= 60;
    tm->tm_min++;
  }
  while (tm->tm_min < 0)
  {
    tm->tm_min += 60;
    tm->tm_hour--;
  }
  while (tm->tm_min >= 60)
  {
    tm->tm_min -= 60;
    tm->tm_hour++;
  }
  while (tm->tm_hour < 0)
  {
    tm->tm_hour += 24;
    tm->tm_mday--;
  }
  while (tm->tm_hour >= 24)
  {
    tm->tm_hour -= 24;
    tm->tm_mday++;
  }
  /* use loops on NNNdwmy user input values? */
  while (tm->tm_mon < 0)
  {
    tm->tm_mon += 12;
    tm->tm_year--;
  }
  while (tm->tm_mon >= 12)
  {
    tm->tm_mon -= 12;
    tm->tm_year++;
  }
  while (tm->tm_mday <= 0)
  {
    if (tm->tm_mon)
      tm->tm_mon--;
    else
    {
      tm->tm_mon = 11;
      tm->tm_year--;
    }
    tm->tm_mday += DaysPerMonth[tm->tm_mon] + isLeapYearFeb (tm);
  }
  while (tm->tm_mday > (DaysPerMonth[tm->tm_mon] + 
	(nLeap = isLeapYearFeb (tm))))
  {
    tm->tm_mday -= DaysPerMonth[tm->tm_mon] + nLeap;
    if (tm->tm_mon < 11)
      tm->tm_mon++;
    else
    {
      tm->tm_mon = 0;
      tm->tm_year++;
    }
  }
}
