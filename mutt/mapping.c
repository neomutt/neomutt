/**
 * @file
 * Store links between a user-readable string and a constant
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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
 * @page mapping Map between a string and a constant
 *
 * Map a string to a constant and vice versa.
 */

#include "config.h"
#include <stddef.h>
#include "mapping.h"
#include "string2.h"

/**
 * mutt_map_get_name - Lookup a string for a constant
 * @param val ID to locate in map
 * @param map NULL-terminated map of strings and constants
 * @retval ptr  String matching ID
 * @retval NULL Error, or ID not found
 */
const char *mutt_map_get_name(int val, const struct Mapping *map)
{
  if (!map)
    return NULL;

  for (size_t i = 0; map[i].name; i++)
    if (map[i].value == val)
      return map[i].name;

  return NULL;
}

/**
 * mutt_map_get_value_n - Lookup the constant for a string
 * @param name String to locate in map
 * @param len  Length of the name string (need not be null terminated)
 * @param map  NULL-terminated map of strings and constants
 * @retval num  ID matching string
 * @retval -1   Error, or string not found
 */
int mutt_map_get_value_n(const char *name, size_t len, const struct Mapping *map)
{
  if (!name || (len == 0) || !map)
    return -1;

  for (size_t i = 0; map[i].name; i++)
  {
    if (mutt_istrn_equal(map[i].name, name, len) && (map[i].name[len] == '\0'))
    {
      return map[i].value;
    }
  }

  return -1;
}

/**
 * mutt_map_get_value - Lookup the constant for a string
 * @param name String to locate in map
 * @param map  NULL-terminated map of strings and constants
 * @retval num  ID matching string
 * @retval -1   Error, or string not found
 */
int mutt_map_get_value(const char *name, const struct Mapping *map)
{
  return mutt_map_get_value_n(name, mutt_str_len(name), map);
}
