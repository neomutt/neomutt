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

/**
 * @page nm_query Notmuch query functions
 *
 * Notmuch query functions
 *
 * @note All functions within this file MUST be unit testable. 
 */

#include "config.h"
#include "mutt/lib.h"
#include "query.h"

/**
 * nm_string_to_query_type - Lookup a query type
 *
 * If there's no query type, default to NM_QUERY_TYPE_MESGS.
 *
 * @param str String to lookup
 * @retval num Query type, e.g. #NM_QUERY_TYPE_MESGS
 */
enum NmQueryType nm_string_to_query_type(const char *str)
{
  if (mutt_str_equal(str, "threads"))
    return NM_QUERY_TYPE_THREADS;
  if (mutt_str_equal(str, "messages"))
    return NM_QUERY_TYPE_MESGS;

  mutt_error(_("failed to parse notmuch query type: %s"), NONULL(str));
  return NM_QUERY_TYPE_MESGS;
}
