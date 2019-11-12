/**
 * @file
 * Time and date handling routines
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
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
 * @page date Time and date handling routines
 *
 * Some commonly used time and date functions.
 */

#include "config.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "date.h"
#include "logging.h"
#include "memory.h"
#include "string2.h"

// clang-format off
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
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/**
 * TimeZones - Lookup table of Time Zones
 *
 * @note Keep in alphabetical order
 */
static const struct Tz TimeZones[] = {
  { "aat",   1,  0, true  }, /* Atlantic Africa Time */
  { "adt",   4,  0, false }, /* Arabia DST */
  { "ast",   3,  0, false }, /* Arabia */
//{ "ast",   4,  0, true  }, /* Atlantic */
  { "bst",   1,  0, false }, /* British DST */
  { "cat",   1,  0, false }, /* Central Africa */
  { "cdt",   5,  0, true  },
  { "cest",  2,  0, false }, /* Central Europe DST */
  { "cet",   1,  0, false }, /* Central Europe */
  { "cst",   6,  0, true  },
//{ "cst",   8,  0, false }, /* China */
//{ "cst",   9, 30, false }, /* Australian Central Standard Time */
  { "eat",   3,  0, false }, /* East Africa */
  { "edt",   4,  0, true  },
  { "eest",  3,  0, false }, /* Eastern Europe DST */
  { "eet",   2,  0, false }, /* Eastern Europe */
  { "egst",  0,  0, false }, /* Eastern Greenland DST */
  { "egt",   1,  0, true  }, /* Eastern Greenland */
  { "est",   5,  0, true  },
  { "gmt",   0,  0, false },
  { "gst",   4,  0, false }, /* Presian Gulf */
  { "hkt",   8,  0, false }, /* Hong Kong */
  { "ict",   7,  0, false }, /* Indochina */
  { "idt",   3,  0, false }, /* Israel DST */
  { "ist",   2,  0, false }, /* Israel */
//{ "ist",   5, 30, false }, /* India */
  { "jst",   9,  0, false }, /* Japan */
  { "kst",   9,  0, false }, /* Korea */
  { "mdt",   6,  0, true  },
  { "met",   1,  0, false }, /* This is now officially CET */
  { "msd",   4,  0, false }, /* Moscow DST */
  { "msk",   3,  0, false }, /* Moscow */
  { "mst",   7,  0, true  },
  { "nzdt", 13,  0, false }, /* New Zealand DST */
  { "nzst", 12,  0, false }, /* New Zealand */
  { "pdt",   7,  0, true  },
  { "pst",   8,  0, true  },
  { "sat",   2,  0, false }, /* South Africa */
  { "smt",   4,  0, false }, /* Seychelles */
  { "sst",  11,  0, true  }, /* Samoa */
//{ "sst",   8,  0, false }, /* Singapore */
  { "utc",   0,  0, false },
  { "wat",   0,  0, false }, /* West Africa */
  { "west",  1,  0, false }, /* Western Europe DST */
  { "wet",   0,  0, false }, /* Western Europe */
  { "wgst",  2,  0, true  }, /* Western Greenland DST */
  { "wgt",   3,  0, true  }, /* Western Greenland */
  { "wst",   8,  0, false }, /* Western Australia */
};
// clang-format on

/**
 * compute_tz - Calculate the number of seconds east of UTC
 * @param g   Local time
 * @param utc UTC time
 * @retval num Seconds east of UTC
 *
 * returns the seconds east of UTC given 'g' and its corresponding gmtime()
 * representation
 */
static time_t compute_tz(time_t g, struct tm *utc)
{
  struct tm lt = mutt_date_localtime(g);

  time_t t = (((lt.tm_hour - utc->tm_hour) * 60) + (lt.tm_min - utc->tm_min)) * 60;

  int yday = (lt.tm_yday - utc->tm_yday);
  if (yday != 0)
  {
    /* This code is optimized to negative timezones (West of Greenwich) */
    if ((yday == -1) || /* UTC passed midnight before localtime */
        (yday > 1))     /* UTC passed new year before localtime */
    {
      t -= (24 * 60 * 60);
    }
    else
    {
      t += (24 * 60 * 60);
    }
  }

  return t;
}

/**
 * is_leap_year_feb - Is a given February in a leap year
 * @param tm Date to be tested
 * @retval true if it's a leap year
 */
static int is_leap_year_feb(struct tm *tm)
{
  if (tm->tm_mon != 1)
    return 0;

  int y = tm->tm_year + 1900;
  return ((y & 3) == 0) && (((y % 100) != 0) || ((y % 400) == 0));
}

/**
 * uncomment_timezone - Strip ()s from a timezone string
 * @param buf    Buffer to store the results
 * @param buflen Length of buffer
 * @param tz     Timezone string to copy
 * @retval ptr Results buffer
 *
 * Some time formats have the timezone in ()s, e.g. (MST) or (-0700)
 *
 * @note If the timezone doesn't have any ()s the function will return a
 *       pointer to the original string.
 */
static const char *uncomment_timezone(char *buf, size_t buflen, const char *tz)
{
  char *p = NULL;
  size_t len;

  if (*tz != '(')
    return tz; /* no need to do anything */
  tz = mutt_str_skip_email_wsp(tz + 1);
  p = strpbrk(tz, " )");
  if (!p)
    return tz;
  len = p - tz;
  if (len > (buflen - 1))
    len = buflen - 1; /* LCOV_EXCL_LINE */
  memcpy(buf, tz, len);
  buf[len] = '\0';
  return buf;
}

/**
 * mutt_date_local_tz - Calculate the local timezone in seconds east of UTC
 * @param t Time to examine
 * @retval num Seconds east of UTC
 *
 * Returns the local timezone in seconds east of UTC for the time t,
 * or for the current time if t is zero.
 */
time_t mutt_date_local_tz(time_t t)
{
  /* Check we haven't overflowed the time (on 32-bit arches) */
  if ((t == TIME_T_MAX) || (t == TIME_T_MIN))
    return 0;

  if (t == 0)
    t = mutt_date_epoch();

  struct tm tm = mutt_date_gmtime(t);
  return compute_tz(t, &tm);
}

/**
 * mutt_date_make_time - Convert `struct tm` to `time_t`
 * @param t     Time to convert
 * @param local Should the local timezone be considered
 * @retval num        Time in Unix format
 * @retval TIME_T_MIN Error
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
  time_t g = AccumDaysPerMonth[t->tm_mon % mutt_array_size(Months)];

  /* The leap years are 1972 and every 4. year until 2096,
   * but this algorithm will fail after year 2099 */
  g += t->tm_mday;
  if ((t->tm_year % 4) || (t->tm_mon < 2))
    g--;
  t->tm_yday = g;

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
      tm->tm_mon--;
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
      tm->tm_mon++;
    else
    {
      tm->tm_mon = 0;
      tm->tm_year++;
    }
  }
}

/**
 * mutt_date_make_date - Write a date in RFC822 format to a buffer
 * @param buf    Buffer for result
 * @param buflen Length of buffer
 * @retval ptr Buffer containing result
 */
char *mutt_date_make_date(char *buf, size_t buflen)
{
  if (!buf)
    return NULL;

  time_t t = mutt_date_epoch();
  struct tm tm = mutt_date_localtime(t);
  time_t tz = mutt_date_local_tz(t);

  tz /= 60;

  snprintf(buf, buflen, "Date: %s, %d %s %d %02d:%02d:%02d %+03d%02d\n",
           Weekdays[tm.tm_wday], tm.tm_mday, Months[tm.tm_mon], tm.tm_year + 1900,
           tm.tm_hour, tm.tm_min, tm.tm_sec, (int) tz / 60, (int) abs((int) tz) % 60);
  return buf;
}

/**
 * mutt_date_check_month - Is the string a valid month name
 * @param s String to check
 * @retval num Index into Months array (0-based)
 * @retval -1  Error
 *
 * @note Only the first three characters are checked
 * @note The comparison is case insensitive
 */
int mutt_date_check_month(const char *s)
{
  for (int i = 0; i < mutt_array_size(Months); i++)
    if (mutt_str_startswith(s, Months[i], CASE_IGNORE))
      return i;

  return -1; /* error */
}

/**
 * mutt_date_epoch - Return the number of seconds since the Unix epoch
 * @retval s The number of s since the Unix epoch, or 0 on failure
 */
time_t mutt_date_epoch(void)
{
  return mutt_date_epoch_ms() / 1000;
}

/**
 * mutt_date_epoch_ms - Return the number of milliseconds since the Unix epoch
 * @retval ms The number of ms since the Unix epoch, or 0 on failure
 */
uint64_t mutt_date_epoch_ms(void)
{
  struct timeval tv = { 0, 0 };
  gettimeofday(&tv, NULL);
  /* We assume that gettimeofday doesn't modify its first argument on failure.
   * We also kind of assume that gettimeofday does not fail. */
  return (uint64_t) tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/**
 * mutt_date_is_day_name - Is the string a valid day name
 * @param s String to check
 * @retval true It's a valid day name
 *
 * @note Only the first three characters are checked
 * @note The comparison is case insensitive
 */
bool mutt_date_is_day_name(const char *s)
{
  if (!s || (strlen(s) < 3) || (s[3] == '\0') || !IS_SPACE(s[3]))
    return false;

  for (int i = 0; i < mutt_array_size(Weekdays); i++)
    if (mutt_str_startswith(s, Weekdays[i], CASE_IGNORE))
      return true;

  return false;
}

/**
 * mutt_date_parse_date - Parse a date string in RFC822 format
 * @param[in]  s      String to parse
 * @param[out] tz_out Pointer to timezone (optional)
 * @retval num Unix time in seconds
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

  int count = 0;
  int hour, min, sec;
  struct tm tm = { 0 };
  int i;
  int tz_offset = 0;
  int zhours = 0;
  int zminutes = 0;
  bool zoccident = false;
  const char *ptz = NULL;
  char tzstr[128];
  char scratch[128];

  /* Don't modify our argument. Fixed-size buffer is ok here since
   * the date format imposes a natural limit.  */

  mutt_str_strfcpy(scratch, s, sizeof(scratch));

  /* kill the day of the week, if it exists. */
  char *t = strchr(scratch, ',');
  if (t)
    t++;
  else
    t = scratch;
  t = mutt_str_skip_email_wsp(t);

  while ((t = strtok(t, " \t")))
  {
    switch (count)
    {
      case 0: /* day of the month */
        if ((mutt_str_atoi(t, &tm.tm_mday) < 0) || (tm.tm_mday < 0))
          return -1;
        if (tm.tm_mday > 31)
          return -1;
        break;

      case 1: /* month of the year */
        i = mutt_date_check_month(t);
        if ((i < 0) || (i > 11))
          return -1;
        tm.tm_mon = i;
        break;

      case 2: /* year */
        if ((mutt_str_atoi(t, &tm.tm_year) < 0) || (tm.tm_year < 0))
          return -1;
        if ((tm.tm_year < 0) || (tm.tm_year > 9999))
          return -1;
        if (tm.tm_year < 50)
          tm.tm_year += 100;
        else if (tm.tm_year >= 1900)
          tm.tm_year -= 1900;
        break;

      case 3: /* time of day */
        if (sscanf(t, "%d:%d:%d", &hour, &min, &sec) == 3)
          ;
        else if (sscanf(t, "%d:%d", &hour, &min) == 2)
          sec = 0;
        else
        {
          mutt_debug(LL_DEBUG1, "could not process time format: %s\n", t);
          return -1;
        }
        if ((hour < 0) || (hour > 23) || (min < 0) || (min > 59) || (sec < 0) || (sec > 60))
          return -1;
        tm.tm_hour = hour;
        tm.tm_min = min;
        tm.tm_sec = sec;
        break;

      case 4: /* timezone */
        /* sometimes we see things like (MST) or (-0700) so attempt to
         * compensate by uncommenting the string if non-RFC822 compliant */
        ptz = uncomment_timezone(tzstr, sizeof(tzstr), t);

        if ((*ptz == '+') || (*ptz == '-'))
        {
          if ((ptz[1] != '\0') && (ptz[2] != '\0') && (ptz[3] != '\0') && (ptz[4] != '\0') &&
              isdigit((unsigned char) ptz[1]) && isdigit((unsigned char) ptz[2]) &&
              isdigit((unsigned char) ptz[3]) && isdigit((unsigned char) ptz[4]))
          {
            zhours = (ptz[1] - '0') * 10 + (ptz[2] - '0');
            zminutes = (ptz[3] - '0') * 10 + (ptz[4] - '0');

            if (ptz[0] == '-')
              zoccident = true;
          }
        }
        else
        {
          /* This is safe to do: A pointer to a struct equals a pointer to its first element */
          struct Tz *tz =
              bsearch(ptz, TimeZones, mutt_array_size(TimeZones), sizeof(struct Tz),
                      (int (*)(const void *, const void *)) mutt_str_strcasecmp);

          if (tz)
          {
            zhours = tz->zhours;
            zminutes = tz->zminutes;
            zoccident = tz->zoccident;
          }

          /* ad hoc support for the European MET (now officially CET) TZ */
          if (mutt_str_strcasecmp(t, "MET") == 0)
          {
            t = strtok(NULL, " \t");
            if (t)
            {
              if (mutt_str_strcasecmp(t, "DST") == 0)
                zhours++;
            }
          }
        }
        tz_offset = (zhours * 3600) + (zminutes * 60);
        if (!zoccident)
          tz_offset = -tz_offset;
        break;
    }
    count++;
    t = 0;
  }

  if (count < 4) /* don't check for missing timezone */
  {
    mutt_debug(LL_DEBUG1, "error parsing date format, using received time\n");
    return -1;
  }

  if (tz_out)
  {
    tz_out->zhours = zhours;
    tz_out->zminutes = zminutes;
    tz_out->zoccident = zoccident;
  }

  time_t time = mutt_date_make_time(&tm, false);
  /* Check we haven't overflowed the time (on 32-bit arches) */
  if ((time != TIME_T_MAX) && (time != TIME_T_MIN))
    time += tz_offset;

  return time;
}

/**
 * mutt_date_make_imap - Format date in IMAP style: DD-MMM-YYYY HH:MM:SS +ZZzz
 * @param buf       Buffer to store the results
 * @param buflen    Length of buffer
 * @param timestamp Time to format
 * @retval num Characters written to buf
 *
 * Caller should provide a buffer of at least 27 bytes.
 */
int mutt_date_make_imap(char *buf, size_t buflen, time_t timestamp)
{
  if (!buf)
    return -1;

  struct tm tm = mutt_date_localtime(timestamp);
  time_t tz = mutt_date_local_tz(timestamp);

  tz /= 60;

  return snprintf(buf, buflen, "%02d-%s-%d %02d:%02d:%02d %+03d%02d",
                  tm.tm_mday, Months[tm.tm_mon], tm.tm_year + 1900, tm.tm_hour,
                  tm.tm_min, tm.tm_sec, (int) tz / 60, (int) abs((int) tz) % 60);
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
  if (!s)
    return 0;

  struct tm t;
  time_t tz;

  t.tm_mday = ((s[0] == ' ') ? s[1] - '0' : (s[0] - '0') * 10 + (s[1] - '0'));
  s += 2;
  if (*s != '-')
    return 0;
  s++;
  t.tm_mon = mutt_date_check_month(s);
  s += 3;
  if (*s != '-')
    return 0;
  s++;
  t.tm_year = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 +
              (s[3] - '0') - 1900;
  s += 4;
  if (*s != ' ')
    return 0;
  s++;

  /* time */
  t.tm_hour = (s[0] - '0') * 10 + (s[1] - '0');
  s += 2;
  if (*s != ':')
    return 0;
  s++;
  t.tm_min = (s[0] - '0') * 10 + (s[1] - '0');
  s += 2;
  if (*s != ':')
    return 0;
  s++;
  t.tm_sec = (s[0] - '0') * 10 + (s[1] - '0');
  s += 2;
  if (*s != ' ')
    return 0;
  s++;

  /* timezone */
  tz = ((s[1] - '0') * 10 + (s[2] - '0')) * 3600 + ((s[3] - '0') * 10 + (s[4] - '0')) * 60;
  if (s[0] == '+')
    tz = -tz;

  return mutt_date_make_time(&t, false) + tz;
}

/**
 * mutt_date_add_timeout - Safely add a timeout to a given time_t value
 * @param now     Time now
 * @param timeout Timeout in seconds
 * @retval num Unix time to timeout
 *
 * This will truncate instead of overflowing.
 */
time_t mutt_date_add_timeout(time_t now, long timeout)
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
 *
 * Uses current time if t is #MUTT_DATE_NOW
 */
struct tm mutt_date_localtime(time_t t)
{
  struct tm tm = { 0 };

  if (t == MUTT_DATE_NOW)
    t = mutt_date_epoch();

  localtime_r(&t, &tm);
  return tm;
}

/**
 * mutt_date_gmtime - Converts calendar time to a broken-down time structure expressed in UTC timezone
 * @param  t  Time
 * @retval obj Broken-down time representation
 *
 * Uses current time if t is #MUTT_DATE_NOW
 */
struct tm mutt_date_gmtime(time_t t)
{
  struct tm tm = { 0 };

  if (t == MUTT_DATE_NOW)
    t = mutt_date_epoch();

  gmtime_r(&t, &tm);
  return tm;
}

/**
 * mutt_date_localtime_format - Format localtime
 * @param buf    Buffer to store formatted time
 * @param buflen Buffer size
 * @param format Format to apply
 * @param t      Time to format
 * @retval num   Number of Bytes added to buffer, excluding null byte.
 */
size_t mutt_date_localtime_format(char *buf, size_t buflen, const char *format, time_t t)
{
  if (!buf || !format)
    return 0;

  struct tm tm = mutt_date_localtime(t);
  return strftime(buf, buflen, format, &tm);
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
