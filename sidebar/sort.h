/**
 * @file
 * Sidebar sorting functions
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

#ifndef MUTT_SIDEBAR_SORT_H
#define MUTT_SIDEBAR_SORT_H

/**
 * enum SidebarSortType - Methods for sorting the Sidebar
 */
enum SidebarSortType
{
  SB_SORT_COUNT,       ///< Sort by total message count
  SB_SORT_DESC,        ///< Sort by mailbox description
  SB_SORT_FLAGGED,     ///< Sort by count of flagged messages
  SB_SORT_PATH,        ///< Sort by mailbox path (alphabetically)
  SB_SORT_UNREAD,      ///< Sort by count of unread messages
  SB_SORT_UNSORTED,    ///< Sort into the order the mailboxes were configured
};

#endif /* MUTT_SIDEBAR_SORT_H */

