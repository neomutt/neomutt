static char rcsid[]="$Id$";
/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
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
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */ 

#include "mutt.h"
#include <string.h>

/* returns the seconds west of UTC given `g' and its corresponding gmtime()
   representation */
static time_t mutt_compute_tz (time_t g, struct tm *utc)
{
  struct tm *lt = localtime (&g);
  time_t t;

  t = (((utc->tm_hour - lt->tm_hour) * 60) + (utc->tm_min - lt->tm_min)) * 60;
  switch (utc->tm_yday - lt->tm_yday)
  {
    case 0:
      break;

    case 1:
    case -364:
    case -365:
      t += 24 * 60 * 60;
      break;

    case -1:
    case 364:
    case 365:
      t -= 24 * 60 * 60;
      break;

    default:
      mutt_error _("Please report this program error in the function mutt_mktime.");
  }

  return t;
}

time_t mutt_local_tz (void)
{
  struct tm *ptm;
  struct tm utc;
  time_t now;

  now = time (NULL);
  ptm = gmtime (&now);
  /* need to make a copy because gmtime/localtime return a pointer to
     static memory (grr!) */
  memcpy (&utc, ptm, sizeof (utc));
  return (mutt_compute_tz (now, &utc));
}

/* converts struct tm to time_t, but does not take the local timezone into
   account unless ``local'' is nonzero */
time_t mutt_mktime (struct tm *t, int local)
{
  time_t g;

  static int AccumDaysPerMonth[12] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
  };

  /* Compute the number of days since January 1 in the same year */
  g = AccumDaysPerMonth [t->tm_mon % 12];

  /* The leap years are 1972 and every 4. year until 2096,
   * but this algoritm will fail after year 2099 */
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
    g += mutt_compute_tz (g, t);

  return (g);
}

void mutt_normalize_time (struct tm *tm)
{
  static char DaysPerMonth[12] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
  };

  while (tm->tm_sec < 0)
  {
    tm->tm_sec += 60;
    tm->tm_min--;
  }
  while (tm->tm_min < 0)
  {
    tm->tm_min += 60;
    tm->tm_hour--;
  }
  while (tm->tm_hour < 0)
  {
    tm->tm_hour += 24;
    tm->tm_mday--;
  }
  while (tm->tm_mon < 0)
  {
    tm->tm_mon += 12;
    tm->tm_year--;
  }
  while (tm->tm_mday < 0)
  {
    if (tm->tm_mon)
      tm->tm_mon--;
    else
    {
      tm->tm_mon = 11;
      tm->tm_year--;
    }
    tm->tm_mday += DaysPerMonth[tm->tm_mon];
  }
}
