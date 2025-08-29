/**
 * @file
 * Time and date handling routines
 *
 * @authors
 * Copyright (C) 2017-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019 Victor Fernandes <criw@pm.me>
 * Copyright (C) 2019-2020 Pietro Cerutti <gahr@gahr.ch>
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

#ifndef MUTT_MUTT_DATE_H
#define MUTT_MUTT_DATE_H

#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#ifdef __APPLE__
#include <xlocale.h>
#endif

struct Buffer;
struct timespec;

/* theoretically time_t can be float but it is integer on most (if not all) systems */
#define TIME_T_MAX ((((time_t) 1 << (sizeof(time_t) * 8 - 2)) - 1) * 2 + 1)
#define TIME_T_MIN (-TIME_T_MAX - 1)
#define TM_YEAR_MAX                                                            \
  (1970 + (((((TIME_T_MAX - 59) / 60) - 59) / 60) - 23) / 24 / 366)
#define TM_YEAR_MIN (1970 - (TM_YEAR_MAX - 1970) - 1)

/**
 * struct Tz - List of recognised Timezones
 */
struct Tz
{
  char tzname[8];         ///< Name, e.g. UTC
  unsigned char zhours;   ///< Hours away from UTC
  unsigned char zminutes; ///< Minutes away from UTC
  bool zoccident;         ///< True if west of UTC, False if East
};

time_t    mutt_date_add_timeout(time_t now, time_t timeout);
int       mutt_date_check_month(const char *s);
time_t    mutt_date_now(void);
uint64_t  mutt_date_now_ms(void);
struct tm mutt_date_gmtime(time_t t);
size_t    mutt_date_localtime_format(char *buf, size_t buflen, const char *format, time_t t);
size_t    mutt_date_localtime_format_locale(char *buf, size_t buflen, const char *format, time_t t, locale_t loc);
struct tm mutt_date_localtime(time_t t);
int       mutt_date_local_tz(time_t t);
void      mutt_date_make_date(struct Buffer *buf, bool local);
int       mutt_date_make_imap(struct Buffer *buf, time_t timestamp);
time_t    mutt_date_make_time(struct tm *t, bool local);
int       mutt_date_make_tls(char *buf, size_t buflen, time_t timestamp);
void      mutt_date_normalize_time(struct tm *tm);
time_t    mutt_date_parse_date(const char *s, struct Tz *tz_out);
time_t    mutt_date_parse_imap(const char *s);
void      mutt_date_sleep_ms(size_t ms);
void      mutt_time_now(struct timespec *tp);

#endif /* MUTT_MUTT_DATE_H */
