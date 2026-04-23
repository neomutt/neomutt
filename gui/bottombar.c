/**
 * @file
 * Bottom Bar Container
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
 * @page gui_bottombar Bottom Bar Container
 *
 * The Bottom Bar is a horizontal container at the bottom of the screen.
 * It holds the Message Container on the left and the Utility Window on
 * the right.
 *
 * ## Windows
 *
 * | Name       | Type         | Constructor     |
 * | :--------- | :----------- | :-------------- |
 * | Bottom Bar | #WT_CONTAINER | bottombar_new() |
 *
 * **Parent**
 * - @ref gui_rootwin
 *
 * **Children**
 * - @ref gui_msgcont
 * - @ref gui_utilwin
 *
 * ## Data
 *
 * The Bottom Bar has no data.
 *
 * ## Events
 *
 * The Bottom Bar does not implement MuttWindow::recalc() or MuttWindow::repaint().
 * It is a pure shaping container managed by the reflow system.
 */

#include "config.h"
#include "core/lib.h"
#include "bottombar.h"
#include "module_data.h"
#include "mutt_window.h"

/**
 * bottombar_new - Create the Bottom Bar Container
 * @retval ptr New Bottom Bar Container Window
 *
 * The Bottom Bar is a horizontal container that holds the Message Container
 * and the Utility Window side by side.  Its height (req_rows) is updated
 * dynamically by msgwin_set_rows() and the msgcont push/pop functions.
 */
struct MuttWindow *bottombar_new(void)
{
  struct GuiModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_GUI);
  struct MuttWindow *win = mutt_window_new(WT_CONTAINER, MUTT_WIN_ORIENT_HORIZONTAL,
                                           MUTT_WIN_SIZE_FIXED,
                                           MUTT_WIN_SIZE_UNLIMITED, 1);
  if (mod_data)
    mod_data->bottom_bar = win;
  return win;
}
