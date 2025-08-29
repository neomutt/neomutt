/**
 * @file
 * Type representing an enumeration
 *
 * @authors
 * Copyright (C) 2019-2021 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_CONFIG_ENUM_H
#define MUTT_CONFIG_ENUM_H

/**
 * struct EnumDef - An enumeration
 */
struct EnumDef
{
  const char     *name;       ///< Config variable
  int             count;      ///< Number of documented options
  struct Mapping *lookup;     ///< Lookup table
};

#endif /* MUTT_CONFIG_ENUM_H */
