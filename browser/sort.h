/**
 * @file
 * Browser sorting functions
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

#ifndef MUTT_BROWSER_SORT_H
#define MUTT_BROWSER_SORT_H

/**
 * enum BrowserSortType - Methods for sorting the Browser
 */
enum BrowserSortType
{
  BROWSER_SORT_ALPHA,      ///< Sort alphabetically by name
  BROWSER_SORT_COUNT,      ///< Sort by total message count
  BROWSER_SORT_DATE,       ///< Sort by date
  BROWSER_SORT_DESC,       ///< Sort by description
  BROWSER_SORT_NEW,        ///< Sort by count of new messages
  BROWSER_SORT_SIZE,       ///< Sort by size
  BROWSER_SORT_UNSORTED,   ///< Sort into the raw order
};

#endif /* MUTT_BROWSER_SORT_H */
