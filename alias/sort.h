/**
 * @file
 * Address book sorting functions
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_ALIAS_SORT_H
#define MUTT_ALIAS_SORT_H

/**
 * enum AliasSortType - Methods for sorting Aliases/Queries
 */
enum AliasSortType
{
  ALIAS_SORT_ALIAS,       ///< Sort by Alias short name
  ALIAS_SORT_EMAIL,       ///< Sort by Email Address
  ALIAS_SORT_NAME,        ///< Sort by Real Name
  ALIAS_SORT_UNSORTED,    ///< Sort by the order the Aliases were configured
};

#endif /* MUTT_ALIAS_SORT_H */
