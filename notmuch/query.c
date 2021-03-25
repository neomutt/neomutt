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
 * nm_parse_type_from_query - Parse a query type out of a query
 * @param buf   Buffer for URL
 * @retval Notmuch query type.  Default: #NM_QUERY_TYPE_MESGS
 *
 * If a user writes a query for a vfolder and includes a type= statement, that
 * type= will be encoded, which Notmuch will treat as part of the query=
 * statement. This method will remove the type= and return its corresponding
 * NmQueryType representation.
 */
enum NmQueryType nm_parse_type_from_query(char *buf)
{
  if (!buf)
    return NM_QUERY_TYPE_MESGS;

  // The six variations of how type= could appear.
  const char *variants[6] = { "&type=threads", "&type=messages",
                              "type=threads&", "type=messages&",
                              "type=threads",  "type=messages" };

  enum NmQueryType query_type = NM_QUERY_TYPE_MESGS;
  int variants_size = mutt_array_size(variants);
  for (int i = 0; i < variants_size; i++)
  {
    if (mutt_istr_find(buf, variants[i]) != NULL)
    {
      // variants[] is setup such that type can be determined via modulo 2.
      query_type = ((i % 2) == 0) ? NM_QUERY_TYPE_THREADS : NM_QUERY_TYPE_MESGS;

      mutt_istr_remall(buf, variants[i]);
    }
  }

  return query_type;
}

/**
 * nm_query_type_to_string - Turn a query type into a string
 * @param query_type Query type
 * @retval ptr String
 *
 * @note This is a static string and must not be freed.
 */
const char *nm_query_type_to_string(enum NmQueryType query_type)
{
  if (query_type == NM_QUERY_TYPE_THREADS)
    return "threads";
  return "messages";
}

/**
 * nm_string_to_query_type - Lookup a query type
 *
 * If there's an unknown query type, default to NM_QUERY_TYPE_MESGS.
 *
 * @param str String to lookup
 * @retval num Query type, e.g. #NM_QUERY_TYPE_MESGS
 */
enum NmQueryType nm_string_to_query_type(const char *str)
{
  enum NmQueryType query_type = nm_string_to_query_type_mapper(str);

  if (query_type == NM_QUERY_TYPE_UNKNOWN)
  {
    mutt_error(_("failed to parse notmuch query type: %s"), NONULL(str));
    return NM_QUERY_TYPE_MESGS;
  }

  return query_type;
}

/**
 * nm_string_to_query_type_mapper - Lookup a query type
 *
 * @param str String to lookup
 * @retval num Query type
 * @retval #NM_QUERY_TYPE_UNKNOWN on error
 */
enum NmQueryType nm_string_to_query_type_mapper(const char *str)
{
  if (mutt_str_equal(str, "threads"))
    return NM_QUERY_TYPE_THREADS;
  if (mutt_str_equal(str, "messages"))
    return NM_QUERY_TYPE_MESGS;

  return NM_QUERY_TYPE_UNKNOWN;
}
