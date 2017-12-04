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

#ifndef _MUTT_DATE_H
#define _MUTT_DATE_H

#include <stdbool.h>
#include <time.h>

/**
 * struct Tz - List of recognised Timezones
 */
struct Tz
{
  char tzname[5];         /**< Name, e.g. UTC */
  unsigned char zhours;   /**< Hours away from UTC */
  unsigned char zminutes; /**< Minutes away from UTC */
  bool zoccident;         /**< True if west of UTC, False if East */
};

extern const char *const Weekdays[];
extern const char *const Months[];
extern const struct Tz TimeZones[];

int     mutt_date_check_month(const char *s);
bool    mutt_date_is_day_name(const char *s);
time_t  mutt_date_local_tz(time_t t);
char   *mutt_date_make_date(char *buf, size_t buflen);
int     mutt_date_make_imap(char *buf, size_t buflen, time_t timestamp);
time_t  mutt_date_make_time(struct tm *t, int local);
void    mutt_date_normalize_time(struct tm *tm);
time_t  mutt_date_parse_date(const char *s, struct Tz *tz_out);
time_t  mutt_date_parse_imap(char *s);

#endif /* _MUTT_DATE_H */
