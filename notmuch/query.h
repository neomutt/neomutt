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

/**
 * enum NmQueryType - Notmuch Query Types
 *
 * Read whole-thread or matching messages only?
 */
enum NmQueryType
{
  NM_QUERY_TYPE_MESGS = 1, ///< Default: Messages only
  NM_QUERY_TYPE_THREADS,   ///< Whole threads
};

enum NmQueryType nm_string_to_query_type(const char *str);
const char *nm_query_type_to_string(enum NmQueryType query_type);

#endif /* MUTT_NOTMUCH_QUERY_H */
