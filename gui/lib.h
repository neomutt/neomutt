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
 * @page lib_gui Graphical code
 *
 * Curses and Window code
 *
 * | File                | Description                |
 * | :------------------ | :------------------------- |
 * | gui/curs_lib.c      | @subpage gui_curs_lib      |
 * | gui/dialog.c        | @subpage gui_dialog        |
 * | gui/global.c        | @subpage gui_global        |
 * | gui/msgcont.c       | @subpage gui_msgcont       |
 * | gui/msgwin.c        | @subpage gui_msgwin        |
 * | gui/mutt_curses.c   | @subpage gui_curses        |
 * | gui/mutt_window.c   | @subpage gui_window        |
 * | gui/reflow.c        | @subpage gui_reflow        |
 * | gui/rootwin.c       | @subpage gui_rootwin       |
 * | gui/sbar.c          | @subpage gui_sbar          |
 * | gui/simple.c        | @subpage gui_simple        |
 * | gui/terminal.c      | @subpage gui_terminal      |
 */

#ifndef MUTT_GUI_LIB_H
#define MUTT_GUI_LIB_H

// IWYU pragma: begin_exports
#include "curs_lib.h"
#include "dialog.h"
#include "global.h"
#include "msgcont.h"
#include "msgwin.h"
#include "mutt_curses.h"
#include "mutt_window.h"
#include "reflow.h"
#include "rootwin.h"
#include "sbar.h"
#include "simple.h"
#include "terminal.h"
// IWYU pragma: end_exports

#endif /* MUTT_GUI_LIB_H */
