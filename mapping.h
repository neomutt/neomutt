/**
 * @file
 * Store links between user-readable string and a constant
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

#ifndef _MUTT_MAPPING_H
#define _MUTT_MAPPING_H

#include <stddef.h>

/**
 * struct Mapping - Mapping between user-readable string and a constant
 */
struct Mapping
{
  const char *name;
  int value;
};

const char *mutt_getnamebyvalue(int val, const struct Mapping *map);
char *mutt_compile_help(char *buf, size_t buflen, int menu, const struct Mapping *items);

int mutt_getvaluebyname(const char *name, const struct Mapping *map);

#endif /* _MUTT_MAPPING_H */
