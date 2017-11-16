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
 *
 * | Function              | Description
 * | :-------------------- | :-------------------------------
 * | mutt_getnamebyvalue() | Lookup a string for a constant
 * | mutt_getvaluebyname() | Lookup the constant for a string
 */

#include "mapping.h"
#include "string2.h"

/**
 * mutt_getnamebyvalue - Lookup a string for a constant
 * @param val ID to locate in map
 * @param map NULL-terminated map of strings and constants
 * @retval str  String matching ID
 * @retval NULL ID not found
 */
const char *mutt_getnamebyvalue(int val, const struct Mapping *map)
{
  for (int i = 0; map[i].name; i++)
    if (map[i].value == val)
      return map[i].name;
  return NULL;
}

/**
 * mutt_getvaluebyname - Lookup the constant for a string
 * @param name String to locate in map
 * @param map  NULL-terminated map of strings and constants
 * @retval num  ID matching string
 * @retval -1   String not found
 */
int mutt_getvaluebyname(const char *name, const struct Mapping *map)
{
  for (int i = 0; map[i].name; i++)
    if (mutt_strcasecmp(map[i].name, name) == 0)
      return map[i].value;
  return -1;
}
