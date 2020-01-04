/**
 * @file
 * Convenience wrapper for the gui headers
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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
 * @page gui GUI: Graphical code
 *
 * Curses and Window code.
 *
 * | File                | Description                |
 * | :------------------ | :------------------------- |
 * | gui/color.c         | @subpage gui_color         |
 * | gui/curs_lib.c      | @subpage gui_curs_lib      |
 * | gui/mutt_curses.c   | @subpage gui_curses        |
 * | gui/mutt_window.c   | @subpage gui_window        |
 * | gui/reflow.c        | @subpage gui_reflow        |
 * | gui/terminal.c      | @subpage gui_terminal      |
 */

#ifndef MUTT_GUI_LIB_H
#define MUTT_GUI_LIB_H

// IWYU pragma: begin_exports
#include "color.h"
#include "curs_lib.h"
#include "mutt_curses.h"
#include "mutt_window.h"
#include "reflow.h"
#include "terminal.h"
// IWYU pragma: end_exports

#endif /* MUTT_GUI_LIB_H */
