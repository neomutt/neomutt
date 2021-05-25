/**
 * @file
 * Message Window
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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
 * @page gui_msgwin Message Window
 *
 * Message Window for displaying messages and text entry.
 */

#include "config.h"
#include "mutt_window.h"

/**
 * msgwin_create - Create the Message Window
 * @retval ptr New Window
 */
struct MuttWindow *msgwin_create(void)
{
  struct MuttWindow *win =
      mutt_window_new(WT_MESSAGE, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED,
                      MUTT_WIN_SIZE_UNLIMITED, 1);

  return win;
}
