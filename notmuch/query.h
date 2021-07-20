/**
 * @file
 * Notmuch query functions
 *
 * @authors
 * Copyright (C) 2021 Austin Ray <austin@austinray.io>
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

#ifndef MUTT_NOTMUCH_QUERY_H
#define MUTT_NOTMUCH_QUERY_H

#include <stddef.h>
#include <stdbool.h>

/**
 * enum NmQueryType - Notmuch Query Types
 *
 * Read whole-thread or matching messages only?
 */
enum NmQueryType
{
  NM_QUERY_TYPE_MESGS = 1, ///< Default: Messages only
  NM_QUERY_TYPE_THREADS,   ///< Whole threads
  NM_QUERY_TYPE_UNKNOWN,   ///< Unknown query type. Error in notmuch query.
};

/**
 * enum NmWindowQueryRc - Return codes for nm_windowed_query_from_query()
 */
enum NmWindowQueryRc
{
  NM_WINDOW_QUERY_SUCCESS = 1,      ///< Query was successful
  NM_WINDOW_QUERY_INVALID_TIMEBASE, ///< Invalid timebase
  NM_WINDOW_QUERY_INVALID_DURATION  ///< Invalid duration
};

enum NmQueryType nm_parse_type_from_query(char *buf);
enum NmQueryType nm_string_to_query_type(const char *str);
enum NmQueryType nm_string_to_query_type_mapper(const char *str);
const char *nm_query_type_to_string(enum NmQueryType query_type);
enum NmWindowQueryRc
nm_windowed_query_from_query(char *buf, size_t buflen, const bool force_enable,
                             const short duration, const short current_pos,
                             const char *current_search, const char *timebase,
                             const char *or_terms);
bool nm_query_window_check_timebase(const char *timebase);

#endif /* MUTT_NOTMUCH_QUERY_H */
