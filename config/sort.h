/**
 * @file
 * Type representing a sort option
 *
 * @authors
 * Copyright (C) 2017-2019 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_CONFIG_SORT_H
#define MUTT_CONFIG_SORT_H

#define mutt_numeric_cmp(a,b) ((a) < (b) ? -1 : ((a) > (b) ? 1 : 0))

/* `$sort` and `$sort_aux` are shorts, and are a composite of a constant sort
 * operation number and a set of compounded bitflags.
 *
 * Everything below SORT_MASK is a constant. There's room for SORT_MASK
 * constant SORT_ values.
 *
 * Everything above is a bitflag. It's OK to move SORT_MASK down by powers of 2
 * if we need more, so long as we don't collide with the constants above. (Or
 * we can just expand sort and sort_aux to uint32_t.)
 */
#define SORT_MASK    ((1 << 8) - 1) ///< Mask for the sort id
#define SORT_REVERSE  (1 << 8)      ///< Reverse the order of the sort
#define SORT_LAST     (1 << 9)      ///< Sort thread by last-X, e.g. received date

#endif /* MUTT_CONFIG_SORT_H */
