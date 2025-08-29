/**
 * @file
 * Time and date handling routines
 *
 * @authors
 * Copyright (C) 2017-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2018-2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2019 Victor Fernandes <criw@pm.me>
 * Copyright (C) 2021 Reto Brunner <reto@slightlybroken.com>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
 * Copyright (C) 2023 Rayford Shireman
 * Copyright (C) 2023 Steinar H Gunderson <steinar+neomutt@gunderson.no>
 * Copyright (C) 2023 наб <nabijaczleweli@nabijaczleweli.xyz>
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
 * @page mutt_date Time and date handling routines
 *
 * Some commonly used time and date functions.
 */

#include "config.h"
#include <ctype.h>
#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "date.h"
#include "buffer.h"
#include "eqi.h"
#include "logging2.h"
#include "memory.h"
#include "prex.h"
#include "regex3.h"
#include "string2.h"

struct timespec;

/**
 * Weekdays - Day of the week (abbreviated)
 */
static const char *const Weekdays[] = {
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
};

/**
 * Months - Months of the year (abbreviated)
 */
static const char *const Months[] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

/**
 * TimeZones - Lookup table of Time Zones
 *
 * @note Keep in alphabetical order
 */
static const struct Tz TimeZones[] = {
  // clang-format off
  { "aat",     1,  0, true  }, /* Atlantic Africa Time */
  { "adt",     4,  0, false }, /* Arabia DST */
  { "ast",     3,  0, false }, /* Arabia */
//{ "ast",     4,  0, true  }, /* Atlantic */
  { "bst",     1,  0, false }, /* British DST */
  { "cat",     1,  0, false }, /* Central Africa */
  { "cdt",     5,  0, true  },
  { "cest",    2,  0, false }, /* Central Europe DST */
  { "cet",     1,  0, false }, /* Central Europe */
  { "cst",     6,  0, true  },
//{ "cst",     8,  0, false }, /* China */
//{ "cst",     9, 30, false }, /* Australian Central Standard Time */
  { "eat",     3,  0, false }, /* East Africa */
  { "edt",     4,  0, true  },
  { "eest",    3,  0, false }, /* Eastern Europe DST */
  { "eet",     2,  0, false }, /* Eastern Europe */
  { "egst",    0,  0, false }, /* Eastern Greenland DST */
  { "egt",     1,  0, true  }, /* Eastern Greenland */
  { "est",     5,  0, true  },
  { "gmt",     0,  0, false },
  { "gst",     4,  0, false }, /* Presian Gulf */
  { "hkt",     8,  0, false }, /* Hong Kong */
  { "ict",     7,  0, false }, /* Indochina */
  { "idt",     3,  0, false }, /* Israel DST */
  { "ist",     2,  0, false }, /* Israel */
//{ "ist",     5, 30, false }, /* India */
  { "jst",     9,  0, false }, /* Japan */
  { "kst",     9,  0, false }, /* Korea */
  { "mdt",     6,  0, true  },
  { "met",     1,  0, false }, /* This is now officially CET */
  { "met dst", 2,  0, false }, /* MET in Daylight Saving Time */
  { "msd",     4,  0, false }, /* Moscow DST */
  { "msk",     3,  0, false }, /* Moscow */
  { "mst",     7,  0, true  },
  { "nzdt",   13,  0, false }, /* New Zealand DST */
  { "nzst",   12,  0, false }, /* New Zealand */
  { "pdt",     7,  0, true  },
  { "pst",     8,  0, true  },
  { "sat",     2,  0, false }, /* South Africa */
  { "smt",     4,  0, false }, /* Seychelles */
  { "sst",    11,  0, true  }, /* Samoa */
//{ "sst",     8,  0, false }, /* Singapore */
  { "utc",     0,  0, false },
  { "wat",     0,  0, false }, /* West Africa */
  { "west",    1,  0, false }, /* Western Europe DST */
  { "wet",     0,  0, false }, /* Western Europe */
  { "wgst",    2,  0, true  }, /* Western Greenland DST */
  { "wgt",     3,  0, true  }, /* Western Greenland */
  { "wst",     8,  0, false }, /* Western Australia */
  // clang-format on
};

/**
 * compute_tz - Calculate the number of seconds east of UTC
 * @param g   Local time
 * @param utc UTC time
 * @retval num Seconds east of UTC
 *
 * returns the seconds east of UTC given 'g' and its corresponding gmtime()
 * representation
 */
static int compute_tz(time_t g, struct tm *utc)
{
  struct tm lt = mutt_date_localtime(g);

  int tz = (((lt.tm_hour - utc->tm_hour) * 60) + (lt.tm_min - utc->tm_min)) * 60;

  int yday = (lt.tm_yday - utc->tm_yday);
  if (yday != 0)
  {
    /* This code is optimized to negative timezones (West of Greenwich) */
    if ((yday == -1) || /* UTC passed midnight before localtime */
        (yday > 1))     /* UTC passed new year before localtime */
    {
      tz -= (24 * 60 * 60);
    }
    else
    {
      tz += (24 * 60 * 60);
    }
  }

  return tz;
}

/**
 * add_tz_offset - Compute and add a timezone offset to an UTC time
 * @param t UTC time
 * @param w True if west of UTC, false if east
 * @param h Number of hours in the timezone
 * @param m Number of minutes in the timezone
 * @retval num Timezone offset in seconds
 */
static time_t add_tz_offset(time_t t, bool w, time_t h, time_t m)
{
  if ((t != TIME_T_MAX) && (t != TIME_T_MIN))
    return t + (w ? 1 : -1) * (((time_t) h * 3600) + ((time_t) m * 60));
  else
    return t;
}

/**
 * find_tz - Look up a timezone
 * @param s Timezone to lookup
 * @param len Length of the s string
 * @retval ptr Pointer to the Tz struct
 * @retval NULL Not found
 */
static const struct Tz *find_tz(const char *s, size_t len)
{
  for (size_t i = 0; i < mutt_array_size(TimeZones); i++)
  {
    if (mutt_istrn_equal(TimeZones[i].tzname, s, len))
      return &TimeZones[i];
  }
  return NULL;
}

/**
 * is_leap_year_feb - Is a given February in a leap year
 * @param tm Date to be tested
 * @retval true It's a leap year
 */
static int is_leap_year_feb(struct tm *tm)
{
  if (tm->tm_mon != 1)
    return 0;

  int y = tm->tm_year + 1900;
  return ((y & 3) == 0) && (((y % 100) != 0) || ((y % 400) == 0));
}

/**
 * mutt_date_local_tz - Calculate the local timezone in seconds east of UTC
 * @param t Time to examine
 * @retval num Seconds east of UTC
 *
 * Returns the local timezone in seconds east of UTC for the time t,
 * or for the current time if t is zero.
 */
int mutt_date_local_tz(time_t t)
{
  /* Check we haven't overflowed the time (on 32-bit arches) */
  if ((t == TIME_T_MAX) || (t == TIME_T_MIN))
    return 0;

  if (t == 0)
    t = mutt_date_now();

  struct tm tm = mutt_date_gmtime(t);
  return compute_tz(t, &tm);
}

/**
 * mutt_date_make_time - Convert `struct tm` to `time_t`
 * @param t     Time to convert
 * @param local Should the local timezone be considered
 * @retval num        Time in Unix format
 * @retval #TIME_T_MIN Error
 *
 * Convert a struct tm to time_t, but don't take the local timezone into
 * account unless "local" is nonzero
 */
time_t mutt_date_make_time(struct tm *t, bool local)
{
  if (!t)
    return TIME_T_MIN;

  static const int AccumDaysPerMonth[mutt_array_size(Months)] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334,
  };

  /* Prevent an integer overflow, with some arbitrary limits. */
  if (t->tm_year > 10000)
    return TIME_T_MAX;
  if (t->tm_year < -10000)
    return TIME_T_MIN;

  if ((t->tm_mday < 1) || (t->tm_mday > 31))
    return TIME_T_MIN;
  if ((t->tm_hour < 0) || (t->tm_hour > 23) || (t->tm_min < 0) ||
      (t->tm_min > 59) || (t->tm_sec < 0) || (t->tm_sec > 60))
  {
    return TIME_T_MIN;
  }
  if (t->tm_year > 9999)
    return TIME_T_MAX;

  /* Compute the number of days since January 1 in the same year */
  int yday = AccumDaysPerMonth[t->tm_mon % mutt_array_size(Months)];

  /* The leap years are 1972 and every 4. year until 2096,
   * but this algorithm will fail after year 2099 */
  yday += t->tm_mday;
  if ((t->tm_year % 4) || (t->tm_mon < 2))
    yday--;
  t->tm_yday = yday;

  time_t g = yday;

  /* Compute the number of days since January 1, 1970 */
  g += (t->tm_year - 70) * (time_t) 365;
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
    g -= compute_tz(g, t);

  return g;
}

/**
 * mutt_date_normalize_time - Fix the contents of a struct tm
 * @param tm Time to correct
 *
 * If values have been added/subtracted from a struct tm, it can lead to
 * invalid dates, e.g.  Adding 10 days to the 25th of a month.
 *
 * This function will correct any over/under-flow.
 */
void mutt_date_normalize_time(struct tm *tm)
{
  if (!tm)
    return;

  static const char DaysPerMonth[mutt_array_size(Months)] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
  };
  int leap;

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
    {
      tm->tm_mon--;
    }
    else
    {
      tm->tm_mon = 11;
      tm->tm_year--;
    }
    tm->tm_mday += DaysPerMonth[tm->tm_mon] + is_leap_year_feb(tm);
  }
  while (tm->tm_mday > (DaysPerMonth[tm->tm_mon] + (leap = is_leap_year_feb(tm))))
  {
    tm->tm_mday -= DaysPerMonth[tm->tm_mon] + leap;
    if (tm->tm_mon < 11)
    {
      tm->tm_mon++;
    }
    else
    {
      tm->tm_mon = 0;
      tm->tm_year++;
    }
  }
}

/**
 * mutt_date_make_date - Write a date in RFC822 format to a buffer
 * @param buf   Buffer for result
 * @param local If true, use the local timezone.  Otherwise use UTC.
 *
 * Appends the date to the passed in buffer.
 * The buffer is not cleared because some callers prepend quotes.
 */
void mutt_date_make_date(struct Buffer *buf, bool local)
{
  if (!buf)
    return;

  struct tm tm = { 0 };
  int tz = 0;

  time_t t = mutt_date_now();
  if (local)
  {
    tm = mutt_date_localtime(t);
    tz = mutt_date_local_tz(t);
  }
  else
  {
    tm = mutt_date_gmtime(t);
  }

  tz /= 60;

  buf_add_printf(buf, "%s, %d %s %d %02d:%02d:%02d %+03d%02d", Weekdays[tm.tm_wday],
                 tm.tm_mday, Months[tm.tm_mon], tm.tm_year + 1900, tm.tm_hour,
                 tm.tm_min, tm.tm_sec, tz / 60, abs(tz) % 60);
}

/**
 * mutt_date_check_month - Is the string a valid month name
 * @param s String to check (must be at least 3 bytes long)
 * @retval num Index into Months array (0-based)
 * @retval -1  Error
 *
 * @note Only the first three characters are checked
 * @note The comparison is case insensitive
 */
int mutt_date_check_month(const char *s)
{
  if (!s)
    return -1;

  char buf[4] = { 0 };
  memcpy(buf, s, 3);
  uint32_t sv;
  memcpy(&sv, buf, sizeof(sv));
  for (int i = 0; i < mutt_array_size(Months); i++)
  {
    uint32_t mv;
    memcpy(&mv, Months[i], sizeof(mv));
    if (sv == mv)
      return i;
  }

  return -1; /* error */
}

/**
 * mutt_date_now - Return the number of seconds since the Unix epoch
 * @retval num Number of seconds since the Unix epoch, or 0 on failure
 */
time_t mutt_date_now(void)
{
  return mutt_date_now_ms() / 1000;
}

/**
 * mutt_date_now_ms - Return the number of milliseconds since the Unix epoch
 * @retval num The number of ms since the Unix epoch, or 0 on failure
 */
uint64_t mutt_date_now_ms(void)
{
  struct timeval tv = { 0, 0 };
  gettimeofday(&tv, NULL);
  /* We assume that gettimeofday doesn't modify its first argument on failure.
   * We also kind of assume that gettimeofday does not fail. */
  return ((uint64_t) tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

/**
 * mutt_time_now - Set the provided time field to the current time
 * @param[out] tp Field to set
 *
 * Uses nanosecond precision if available, if not we fallback to microseconds.
 */
void mutt_time_now(struct timespec *tp)
{
#ifdef HAVE_CLOCK_GETTIME
  if (clock_gettime(CLOCK_REALTIME, tp) != 0)
    mutt_perror("clock_gettime");
#else
  struct timeval tv = { 0, 0 };
  if (gettimeofday(&tv, NULL) != 0)
    mutt_perror("gettimeofday");
  tp->tv_sec = tv.tv_sec;
  tp->tv_nsec = tv.tv_usec * 1000;
#endif
}

/**
 * parse_small_uint - Parse a positive integer of at most 5 digits
 * @param[in]  str String to parse
 * @param[in]  end End of the string
 * @param[out] val Value
 * @retval num Number of chars parsed
 *
 * Leaves val untouched if the given string does not start with an integer;
 * ignores junk after it or any digits beyond the first five (this is so
 * that the function can never overflow, yet check if the integer is
 * larger than the maximum 4 digits supported in a year).
 * and does not support negative numbers. Empty strings are parsed as zero.
 */
static int parse_small_uint(const char *str, const char *end, int *val)
{
  const char *ptr = str;
  int v = 0;
  while ((ptr < end) && (ptr < (str + 5)) && (*ptr >= '0') && (*ptr <= '9'))
  {
    v = (v * 10) + (*ptr - '0');
    ptr++;
  }
  *val = v;
  return ptr - str;
}

/**
 * mutt_date_parse_rfc5322_strict - Parse a date string in RFC822 format
 * @param[in]  s      String to parse
 * @param[out] tz_out Timezone info (OPTIONAL)
 * @retval num Unix time in seconds
 *
 * Parse a date string in RFC822 format, without any comments or extra
 * whitespace (except a comment at the very end, since that is very common for
 * time zones).
 *
 * This is a fairly straightforward implementation in the hope of extracting
 * the valid cases quickly, i.e., without having to resort to a regex.
 * The hard cases are left to a regex implementation further down in
 * mutt_date_parse_date() (which calls us).
 *
 * Spec: https://tools.ietf.org/html/rfc5322#section-3.3
 */
static time_t mutt_date_parse_rfc5322_strict(const char *s, struct Tz *tz_out)
{
  size_t len = strlen(s);

  /* Skip over the weekday, if any. */
  if ((len >= 5) && (s[4] == ' ') &&
      (eqi4(s, "Mon,") || eqi4(s, "Tue,") || eqi4(s, "Wed,") ||
       eqi4(s, "Thu,") || eqi4(s, "Fri,") || eqi4(s, "Sat,") || eqi4(s, "Sun,")))
  {
    s += 5;
    len -= 5;
  }

  while ((len > 0) && (*s == ' '))
  {
    s++;
    len--;
  }

  if ((len == 0) || (*s < '0') || (*s > '9'))
    return -1;

  struct tm tm = { 0 };

  /* Day */
  int mday_len = parse_small_uint(s, s + len, &tm.tm_mday);
  if ((mday_len == 0) || (mday_len > 2) || (tm.tm_mday > 31))
    return -1;
  s += mday_len;
  len -= mday_len;

  if ((len == 0) || (*s != ' '))
    return -1;
  s++;
  len--;

  /* Month */
  if (len < 3)
    return -1;
  tm.tm_mon = mutt_date_check_month(s);
  if (tm.tm_mon == -1)
    return -1;
  s += 3;
  len -= 3;

  if ((len == 0) || (*s != ' '))
    return -1;
  s++;
  len--;

  /* Year */
  int year_len = parse_small_uint(s, s + len, &tm.tm_year);
  if ((year_len != 2) && (year_len != 4))
    return -1;
  if (tm.tm_year < 50)
    tm.tm_year += 100;
  else if (tm.tm_year >= 1900)
    tm.tm_year -= 1900;
  s += year_len;
  len -= year_len;

  if ((len == 0) || (*s != ' '))
    return -1;
  s++;
  len--;

  /* Hour */
  if ((len < 3) || (s[0] < '0') || (s[0] > '2') || (s[1] < '0') ||
      (s[1] > '9') || (s[2] != ':'))
  {
    return -1;
  }
  tm.tm_hour = ((s[0] - '0') * 10) + (s[1] - '0');
  if (tm.tm_hour > 23)
    return -1;
  s += 3;
  len -= 3;

  /* Minute */
  if ((len < 2) || (s[0] < '0') || (s[0] > '5') || (s[1] < '0') || (s[1] > '9'))
    return -1;
  tm.tm_min = ((s[0] - '0') * 10) + (s[1] - '0');
  if (tm.tm_min > 59)
    return -1;
  s += 2;
  len -= 2;

  /* Second (optional) */
  if ((len > 0) && (s[0] == ':'))
  {
    s++;
    len--;
    if ((len < 2) || (s[0] < '0') || (s[0] > '5') || (s[1] < '0') || (s[1] > '9'))
      return -1;
    tm.tm_sec = ((s[0] - '0') * 10) + (s[1] - '0');
    if (tm.tm_sec > 60)
      return -1;
    s += 2;
    len -= 2;
  }

  while ((len > 0) && (*s == ' '))
  {
    s++;
    len--;
  }

  /* Strip optional time zone comment and white space from the end
   * (this is the only one that is very common) */
  while ((len > 0) && (s[len - 1] == ' '))
    len--;
  if ((len >= 2) && (s[len - 1] == ')'))
  {
    for (int i = len - 1; i-- > 0;)
    {
      if (s[i] == '(')
      {
        len = i;
        break;
      }
      if (!isalpha(s[i]) && (s[i] != ' '))
        return -1; /* give up more complex comment parsing */
    }
  }
  while ((len > 0) && (s[len - 1] == ' '))
    len--;

  /* Time zone (optional) */
  int zhours = 0;
  int zminutes = 0;
  bool zoccident = false;
  if (len > 0)
  {
    if ((len == 5) && ((s[0] == '+') || (s[0] == '-')) && (s[1] >= '0') &&
        (s[1] <= '9') && (s[2] >= '0') && (s[2] <= '9') && (s[3] >= '0') &&
        (s[3] <= '9') && (s[4] >= '0') && (s[4] <= '9'))
    {
      zoccident = (s[0] == '-');
      zhours = ((s[1] - '0') * 10) + (s[2] - '0');
      zminutes = ((s[3] - '0') * 10) + (s[4] - '0');
    }
    else
    {
      for (int i = 0; i < len; ++i)
      {
        if (!isalpha(s[i]))
          return -1;
      }
      const struct Tz *tz = find_tz(s, len);
      if (tz)
      {
        zhours = tz->zhours;
        zminutes = tz->zminutes;
        zoccident = tz->zoccident;
      }
    }
  }

  if (tz_out)
  {
    tz_out->zhours = zhours;
    tz_out->zminutes = zminutes;
    tz_out->zoccident = zoccident;
  }

  return add_tz_offset(mutt_date_make_time(&tm, false), zoccident, zhours, zminutes);
}

/**
 * mutt_date_parse_date - Parse a date string in RFC822 format
 * @param[in]  s      String to parse
 * @param[out] tz_out Pointer to timezone (optional)
 * @retval num Unix time in seconds, or -1 on failure
 *
 * Parse a date of the form:
 * `[ weekday , ] day-of-month month year hour:minute:second [ timezone ]`
 *
 * The 'timezone' field is optional; it defaults to +0000 if missing.
 */
time_t mutt_date_parse_date(const char *s, struct Tz *tz_out)
{
  if (!s)
    return -1;

  const time_t strict_t = mutt_date_parse_rfc5322_strict(s, tz_out);
  if (strict_t != -1)
    return strict_t;

  const regmatch_t *match = mutt_prex_capture(PREX_RFC5322_DATE_LAX, s);
  if (!match)
  {
    mutt_debug(LL_DEBUG1, "Could not parse date: <%s>\n", s);
    return -1;
  }
  mutt_debug(LL_DEBUG2, "Fallback regex for date: <%s>\n", s);

  struct tm tm = { 0 };

  // clang-format off
  const regmatch_t *mday    = &match[PREX_RFC5322_DATE_LAX_MATCH_DAY];
  const regmatch_t *mmonth  = &match[PREX_RFC5322_DATE_LAX_MATCH_MONTH];
  const regmatch_t *myear   = &match[PREX_RFC5322_DATE_LAX_MATCH_YEAR];
  const regmatch_t *mhour   = &match[PREX_RFC5322_DATE_LAX_MATCH_HOUR];
  const regmatch_t *mminute = &match[PREX_RFC5322_DATE_LAX_MATCH_MINUTE];
  const regmatch_t *msecond = &match[PREX_RFC5322_DATE_LAX_MATCH_SECOND];
  const regmatch_t *mtz     = &match[PREX_RFC5322_DATE_LAX_MATCH_TZ];
  const regmatch_t *mtzobs  = &match[PREX_RFC5322_DATE_LAX_MATCH_TZ_OBS];
  // clang-format on

  /* Day */
  sscanf(s + mutt_regmatch_start(mday), "%d", &tm.tm_mday);
  if (tm.tm_mday > 31)
    return -1;

  /* Month */
  tm.tm_mon = mutt_date_check_month(s + mutt_regmatch_start(mmonth));

  /* Year */
  sscanf(s + mutt_regmatch_start(myear), "%d", &tm.tm_year);
  if (tm.tm_year < 50)
    tm.tm_year += 100;
  else if (tm.tm_year >= 1900)
    tm.tm_year -= 1900;

  /* Time */
  int hour = 0, min = 0, sec = 0;
  sscanf(s + mutt_regmatch_start(mhour), "%d", &hour);
  sscanf(s + mutt_regmatch_start(mminute), "%d", &min);
  if (mutt_regmatch_start(msecond) != -1)
    sscanf(s + mutt_regmatch_start(msecond), "%d", &sec);
  if ((hour > 23) || (min > 59) || (sec > 60))
    return -1;
  tm.tm_hour = hour;
  tm.tm_min = min;
  tm.tm_sec = sec;

  /* Time zone */
  int zhours = 0;
  int zminutes = 0;
  bool zoccident = false;
  if (mutt_regmatch_start(mtz) != -1)
  {
    char direction = '\0';
    sscanf(s + mutt_regmatch_start(mtz), "%c%02d%02d", &direction, &zhours, &zminutes);
    zoccident = (direction == '-');
  }
  else if (mutt_regmatch_start(mtzobs) != -1)
  {
    const struct Tz *tz = find_tz(s + mutt_regmatch_start(mtzobs),
                                  mutt_regmatch_len(mtzobs));
    if (tz)
    {
      zhours = tz->zhours;
      zminutes = tz->zminutes;
      zoccident = tz->zoccident;
    }
  }

  if (tz_out)
  {
    tz_out->zhours = zhours;
    tz_out->zminutes = zminutes;
    tz_out->zoccident = zoccident;
  }

  return add_tz_offset(mutt_date_make_time(&tm, false), zoccident, zhours, zminutes);
}

/**
 * mutt_date_make_imap - Format date in IMAP style: DD-MMM-YYYY HH:MM:SS +ZZzz
 * @param buf       Buffer to store the results
 * @param timestamp Time to format
 * @retval num Characters written to buf
 */
int mutt_date_make_imap(struct Buffer *buf, time_t timestamp)
{
  if (!buf)
    return -1;

  struct tm tm = mutt_date_localtime(timestamp);
  int tz = mutt_date_local_tz(timestamp);

  tz /= 60;

  return buf_printf(buf, "%02d-%s-%d %02d:%02d:%02d %+03d%02d", tm.tm_mday,
                    Months[tm.tm_mon], tm.tm_year + 1900, tm.tm_hour, tm.tm_min,
                    tm.tm_sec, tz / 60, abs(tz) % 60);
}

/**
 * mutt_date_make_tls - Format date in TLS certificate verification style
 * @param buf       Buffer to store the results
 * @param buflen    Length of buffer
 * @param timestamp Time to format
 * @retval num Characters written to buf
 *
 * e.g., Mar 17 16:40:46 2016 UTC. The time is always in UTC.
 *
 * Caller should provide a buffer of at least 27 bytes.
 */
int mutt_date_make_tls(char *buf, size_t buflen, time_t timestamp)
{
  if (!buf)
    return -1;

  struct tm tm = mutt_date_gmtime(timestamp);
  return snprintf(buf, buflen, "%s, %d %s %d %02d:%02d:%02d UTC",
                  Weekdays[tm.tm_wday], tm.tm_mday, Months[tm.tm_mon],
                  tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

/**
 * mutt_date_parse_imap - Parse date of the form: DD-MMM-YYYY HH:MM:SS +ZZzz
 * @param s Date in string form
 * @retval num Unix time
 * @retval 0   Error
 */
time_t mutt_date_parse_imap(const char *s)
{
  const regmatch_t *match = mutt_prex_capture(PREX_IMAP_DATE, s);
  if (!match)
    return 0;

  const regmatch_t *mday = &match[PREX_IMAP_DATE_MATCH_DAY];
  const regmatch_t *mmonth = &match[PREX_IMAP_DATE_MATCH_MONTH];
  const regmatch_t *myear = &match[PREX_IMAP_DATE_MATCH_YEAR];
  const regmatch_t *mtime = &match[PREX_IMAP_DATE_MATCH_TIME];
  const regmatch_t *mtz = &match[PREX_IMAP_DATE_MATCH_TZ];

  struct tm tm = { 0 };

  sscanf(s + mutt_regmatch_start(mday), " %d", &tm.tm_mday);
  tm.tm_mon = mutt_date_check_month(s + mutt_regmatch_start(mmonth));
  sscanf(s + mutt_regmatch_start(myear), "%d", &tm.tm_year);
  tm.tm_year -= 1900;
  sscanf(s + mutt_regmatch_start(mtime), "%d:%d:%d", &tm.tm_hour, &tm.tm_min, &tm.tm_sec);

  char direction = '\0';
  int zhours = 0, zminutes = 0;
  sscanf(s + mutt_regmatch_start(mtz), "%c%02d%02d", &direction, &zhours, &zminutes);
  bool zoccident = (direction == '-');

  return add_tz_offset(mutt_date_make_time(&tm, false), zoccident, zhours, zminutes);
}

/**
 * mutt_date_add_timeout - Safely add a timeout to a given time_t value
 * @param now     Time now
 * @param timeout Timeout in seconds
 * @retval num Unix time to timeout
 *
 * This will truncate instead of overflowing.
 */
time_t mutt_date_add_timeout(time_t now, time_t timeout)
{
  if (timeout < 0)
    return now;

  if ((TIME_T_MAX - now) < timeout)
    return TIME_T_MAX;

  return now + timeout;
}

/**
 * mutt_date_localtime - Converts calendar time to a broken-down time structure expressed in user timezone
 * @param  t  Time
 * @retval obj Broken-down time representation
 */
struct tm mutt_date_localtime(time_t t)
{
  struct tm tm = { 0 };

  struct tm *ret = localtime_r(&t, &tm);
  if (!ret)
  {
    mutt_debug(LL_DEBUG1, "Could not convert time_t via localtime_r() to struct tm: time_t = %jd\n",
               (intmax_t) t);
    struct tm default_tm = { 0 }; // 1970-01-01 00:00:00
    mktime(&default_tm); // update derived fields making tm into a valid tm.
    tm = default_tm;
  }
  return tm;
}

/**
 * mutt_date_gmtime - Converts calendar time to a broken-down time structure expressed in UTC timezone
 * @param  t  Time
 * @retval obj Broken-down time representation
 */
struct tm mutt_date_gmtime(time_t t)
{
  struct tm tm = { 0 };

  struct tm *ret = gmtime_r(&t, &tm);
  if (!ret)
  {
    mutt_debug(LL_DEBUG1, "Could not convert time_t via gmtime_r() to struct tm: time_t = %jd\n",
               (intmax_t) t);
    struct tm default_tm = { 0 }; // 1970-01-01 00:00:00
    mktime(&default_tm); // update derived fields making tm into a valid tm.
    tm = default_tm;
  }
  return tm;
}

/**
 * mutt_date_localtime_format - Format localtime
 * @param buf    Buffer to store formatted time
 * @param buflen Buffer size
 * @param format Format to apply
 * @param t      Time to format
 * @retval num   Number of Bytes added to buffer, excluding NUL byte
 */
size_t mutt_date_localtime_format(char *buf, size_t buflen, const char *format, time_t t)
{
  if (!buf || !format)
    return 0;

  struct tm tm = mutt_date_localtime(t);
  return strftime(buf, buflen, format, &tm);
}

/**
 * mutt_date_localtime_format_locale - Format localtime using a given locale
 * @param buf    Buffer to store formatted time
 * @param buflen Buffer size
 * @param format Format to apply
 * @param t      Time to format
 * @param loc    Locale to use
 * @retval num   Number of Bytes added to buffer, excluding NUL byte
 */
size_t mutt_date_localtime_format_locale(char *buf, size_t buflen,
                                         const char *format, time_t t, locale_t loc)
{
  if (!buf || !format)
    return 0;

  struct tm tm = mutt_date_localtime(t);
  return strftime_l(buf, buflen, format, &tm, loc);
}

/**
 * mutt_date_sleep_ms - Sleep for milliseconds
 * @param ms Number of milliseconds to sleep
 */
void mutt_date_sleep_ms(size_t ms)
{
  const struct timespec sleep = {
    .tv_sec = ms / 1000,
    .tv_nsec = (ms % 1000) * 1000000UL,
  };
  nanosleep(&sleep, NULL);
}
