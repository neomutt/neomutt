/**
 * @file
 * Colour used by libsidebar
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_SIDEBAR_COLOR_H
#define MUTT_SIDEBAR_COLOR_H

/**
 * enum ColorSidebar - Sidebar Colours
 *
 * @sa CD_SIDEBAR, ColorDomain
 */
enum ColorSidebar
{
  CD_SID_BACKGROUND = 1,           ///< Background colour for the Sidebar
  CD_SID_DIVIDER,                  ///< Line dividing sidebar from the index/pager
  CD_SID_FLAGGED,                  ///< Mailbox with flagged messages
  CD_SID_HIGHLIGHT,                ///< Select cursor
  CD_SID_INDICATOR,                ///< Current open mailbox
  CD_SID_NEW,                      ///< Mailbox with new mail
  CD_SID_ORDINARY,                 ///< Mailbox with no new or flagged messages
  CD_SID_SPOOL_FILE,               ///< $spool_file (Spool mailbox)
  CD_SID_UNREAD,                   ///< Mailbox with unread mail
};

void sidebar_colors_init(void);

#endif /* MUTT_SIDEBAR_COLOR_H */
