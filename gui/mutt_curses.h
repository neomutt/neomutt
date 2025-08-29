/**
 * @file
 * Define wrapper functions around Curses
 *
 * @authors
 * Copyright (C) 2019-2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_GUI_MUTT_CURSES_H
#define MUTT_GUI_MUTT_CURSES_H

#include "config.h"
#include "color/lib.h"

#ifdef HAVE_NCURSESW_NCURSES_H
#include <ncursesw/ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#else
#include <curses.h>
#endif

#if (((NCURSES_VERSION_MAJOR > 6) ||                                           \
      ((NCURSES_VERSION_MAJOR == 6) && (NCURSES_VERSION_MINOR >= 1))) &&       \
     defined(NCURSES_EXT_COLORS))
#define NEOMUTT_DIRECT_COLORS
#endif

// ncurses defined A_ITALIC in such a way, that we can't use #if on it.
#ifdef A_ITALIC
#define A_ITALIC_DEFINED
#else
#define A_ITALIC 0
#endif

#define ctrl(ch) ((ch) - '@')

#ifdef KEY_ENTER
#define key_is_return(ch) (((ch) == '\r') || ((ch) == '\n') || ((ch) == KEY_ENTER))
#else
#define key_is_return(ch) (((ch) == '\r') || ((ch) == '\n'))
#endif

/**
 * enum MuttCursorState - Cursor states for mutt_curses_set_cursor()
 */
enum MuttCursorState
{
  MUTT_CURSOR_INVISIBLE    =  0, ///< Hide the cursor
  MUTT_CURSOR_VISIBLE      =  1, ///< Display a normal cursor
  MUTT_CURSOR_VERY_VISIBLE =  2, ///< Display a very visible cursor
};

void                    mutt_curses_set_color(const struct AttrColor *ac);
const struct AttrColor *mutt_curses_set_color_by_id(enum ColorId cid);
enum MuttCursorState    mutt_curses_set_cursor(enum MuttCursorState state);
const struct AttrColor *mutt_curses_set_normal_backed_color_by_id(enum ColorId cid);
void                    mutt_resize_screen(void);

#endif /* MUTT_GUI_MUTT_CURSES_H */
