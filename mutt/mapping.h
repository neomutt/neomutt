/**
 * @file
 * Store links between a user-readable string and a constant
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

#ifndef MUTT_LIB_MAPPING_H
#define MUTT_LIB_MAPPING_H

/**
 * struct Mapping - Mapping between user-readable string and a constant
 */
struct Mapping
{
  const char *name;
  int value;
};

const char *mutt_map_get_name(int val, const struct Mapping *map);
int         mutt_map_get_value(const char *name, const struct Mapping *map);
int         mutt_map_get_value_n(const char *name, size_t len, const struct Mapping *map);

#endif /* MUTT_LIB_MAPPING_H */
