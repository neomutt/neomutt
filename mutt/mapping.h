/**
 * @file
 * Store links between a user-readable string and a constant
 *
 * @authors
 * Copyright (C) 2017-2021 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
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

#ifndef MUTT_MUTT_MAPPING_H
#define MUTT_MUTT_MAPPING_H

#include <stddef.h>

/**
 * struct Mapping - Mapping between user-readable string and a constant
 */
struct Mapping
{
  const char *name; ///< String value
  int value;        ///< Integer value
};

const char *mutt_map_get_name(int val, const struct Mapping *map);
int         mutt_map_get_value(const char *name, const struct Mapping *map);
int         mutt_map_get_value_n(const char *name, size_t len, const struct Mapping *map);

#endif /* MUTT_MUTT_MAPPING_H */
