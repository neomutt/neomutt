/**
 * @file
 * Determine who the email is from
 *
 * @authors
 * Copyright (C) 1996-2000,2013 Michael R. Elkins <me@mutt.org>
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

#include "config.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "mutt/mutt.h"

/*
 * A valid message separator looks like:
 *
 * From [ <return-path> ] <weekday> <month> <day> <time> [ <timezone> ] <year>
 */
int is_from(const char *s, char *path, size_t pathlen, time_t *tp)
{
  struct tm tm;
  int yr;

  if (path)
    *path = '\0';

  if (mutt_str_strncmp("From ", s, 5) != 0)
    return 0;

  s = mutt_str_next_word(s); /* skip over the From part. */
  if (!*s)
    return 0;

  mutt_debug(3, "\nis_from(): parsing: %s\n", s);

  if (!mutt_date_is_day_name(s))
  {
    const char *p = NULL;
    size_t len;
    short q = 0;

    for (p = s; *p && (q || !ISSPACE(*p)); p++)
    {
      if (*p == '\\')
      {
        if (*++p == '\0')
          return 0;
      }
      else if (*p == '"')
      {
        q = !q;
      }
    }

    if (q || !*p)
      return 0;

    /* pipermail archives have the return_path obscured such as "me at mutt.org" */
    if (mutt_str_strncasecmp(p, " at ", 4) == 0)
    {
      p = strchr(p + 4, ' ');
      if (!p)
      {
        mutt_debug(1, "error parsing what appears to be a pipermail-style "
                      "obscured return_path: %s\n",
                   s);
        return 0;
      }
    }

    if (path)
    {
      len = (size_t)(p - s);
      if (len + 1 > pathlen)
        len = pathlen - 1;
      memcpy(path, s, len);
      path[len] = '\0';
      mutt_debug(3, "got return path: %s\n", path);
    }

    s = p + 1;
    SKIPWS(s);
    if (!*s)
      return 0;

    if (!mutt_date_is_day_name(s))
    {
      mutt_debug(1, " expected weekday, got: %s\n", s);
      return 0;
    }
  }

  s = mutt_str_next_word(s);
  if (!*s)
    return 0;

  /* do a quick check to make sure that this isn't really the day of the week.
   * this could happen when receiving mail from a local user whose login name
   * is the same as a three-letter abbreviation of the day of the week.
   */
  if (mutt_date_is_day_name(s))
  {
    s = mutt_str_next_word(s);
    if (!*s)
      return 0;
  }

  /* now we should be on the month. */
  tm.tm_mon = mutt_date_check_month(s);
  if (tm.tm_mon < 0)
    return 0;

  /* day */
  s = mutt_str_next_word(s);
  if (!*s)
    return 0;
  if (sscanf(s, "%d", &tm.tm_mday) != 1)
    return 0;
  if ((tm.tm_mday < 1) || (tm.tm_mday > 31))
    return 0;

  /* time */
  s = mutt_str_next_word(s);
  if (!*s)
    return 0;

  /* Accept either HH:MM or HH:MM:SS */
  if (sscanf(s, "%d:%d:%d", &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 3)
    ;
  else if (sscanf(s, "%d:%d", &tm.tm_hour, &tm.tm_min) == 2)
    tm.tm_sec = 0;
  else
    return 0;

  if ((tm.tm_hour < 0) || (tm.tm_hour > 23) || (tm.tm_min < 0) ||
      (tm.tm_min > 59) || (tm.tm_sec < 0) || (tm.tm_sec > 60))
  {
    return 0;
  }

  s = mutt_str_next_word(s);
  if (!*s)
    return 0;

  /* timezone? */
  if (isalpha((unsigned char) *s) || *s == '+' || *s == '-')
  {
    s = mutt_str_next_word(s);
    if (!*s)
      return 0;

    /*
     * some places have two timezone fields after the time, e.g.
     *      From xxxx@yyyyyyy.fr Wed Aug  2 00:39:12 MET DST 1995
     */
    if (isalpha((unsigned char) *s))
    {
      s = mutt_str_next_word(s);
      if (!*s)
        return 0;
    }
  }

  /* year */
  if (sscanf(s, "%d", &yr) != 1)
    return 0;
  if ((yr < 0) || (yr > 9999))
    return 0;
  tm.tm_year = yr > 1900 ? yr - 1900 : (yr < 70 ? yr + 100 : yr);

  mutt_debug(3, "month=%d, day=%d, hr=%d, min=%d, sec=%d, yr=%d.\n", tm.tm_mon,
             tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_year);

  tm.tm_isdst = -1;

  if (tp)
    *tp = mutt_date_make_time(&tm, 0);
  return 1;
}
