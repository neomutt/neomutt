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

/**
 * @page email_from Determine who the email is from
 *
 * Determine who the email is from
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include "mutt/lib.h"
#include "from.h"

/**
 * is_from - Is a string a 'From' header line?
 * @param[in]  s       String to test
 * @param[out] path    Buffer for extracted path
 * @param[in]  pathlen Length of buffer
 * @param[out] tp      Extracted time
 * @retval 1 Yes, it is
 * @retval 0 No, it isn't
 *
 * A valid message separator looks like:
 * `From <return-path> <weekday> <month> <day> <time> <year>`
 */
bool is_from(const char *s, char *path, size_t pathlen, time_t *tp)
{
  bool lax = false;

  const regmatch_t *match = mutt_prex_capture(PREX_MBOX_FROM, s);
  if (!match)
  {
    match = mutt_prex_capture(PREX_MBOX_FROM_LAX, s);
    if (!match)
    {
      mutt_debug(LL_DEBUG1, "Could not parse From line: <%s>\n", s);
      return false;
    }
    lax = true;
    mutt_debug(LL_DEBUG2, "Fallback regex for From line: <%s>\n", s);
  }

  if (path)
  {
    const regmatch_t *msender =
        &match[lax ? PREX_MBOX_FROM_LAX_MATCH_ENVSENDER : PREX_MBOX_FROM_MATCH_ENVSENDER];
    const size_t dsize = MIN(pathlen, mutt_regmatch_len(msender) + 1);
    mutt_str_copy(path, s + mutt_regmatch_start(msender), dsize);
  }

  if (tp)
  {
    // clang-format off
    const regmatch_t *mmonth = &match[lax ? PREX_MBOX_FROM_LAX_MATCH_MONTH : PREX_MBOX_FROM_MATCH_MONTH];
    const regmatch_t *mday   = &match[lax ? PREX_MBOX_FROM_LAX_MATCH_DAY   : PREX_MBOX_FROM_MATCH_DAY];
    const regmatch_t *mtime  = &match[lax ? PREX_MBOX_FROM_LAX_MATCH_TIME  : PREX_MBOX_FROM_MATCH_TIME];
    const regmatch_t *myear  = &match[lax ? PREX_MBOX_FROM_LAX_MATCH_YEAR  : PREX_MBOX_FROM_MATCH_YEAR];
    // clang-format on

    struct tm tm = { 0 };
    tm.tm_isdst = -1;
    tm.tm_mon = mutt_date_check_month(s + mutt_regmatch_start(mmonth));
    sscanf(s + mutt_regmatch_start(mday), " %d", &tm.tm_mday);
    sscanf(s + mutt_regmatch_start(mtime), "%d:%d:%d", &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
    int year = 0;
    sscanf(s + mutt_regmatch_start(myear), "%d", &year);
    if (year > 1900)
      tm.tm_year = year - 1900;
    else if (year < 70)
      tm.tm_year = year + 100;
    else
      tm.tm_year = year;
    *tp = mutt_date_make_time(&tm, false);
  }

  return true;
}
