/**
 * @file
 * Define wrapper functions around Curses/Slang
 *
 * @authors
 * Copyright (C) 1996-2000,2012 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2004 g10 Code GmbH
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

#ifndef MUTT_MUTT_CURSES_H
#define MUTT_MUTT_CURSES_H

#include "config.h"
#include <stdbool.h>
#include "color.h"

#ifdef USE_SLANG_CURSES

#ifndef unix /* this symbol is not defined by the hp-ux compiler */
#define unix
#endif

#include <slang.h> /* in addition to slcurses.h, we need slang.h for the version
                      number to test for 2.x having UTF-8 support in main.c */
#ifdef bool
#undef bool
#endif

#include <slcurses.h>

#ifdef bool
#undef bool
#define bool _Bool
#endif

#define KEY_DC SL_KEY_DELETE
#define KEY_IC SL_KEY_IC

#else /* USE_SLANG_CURSES */

#ifdef HAVE_NCURSESW_NCURSES_H
#include <ncursesw/ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#else
#include <curses.h>
#endif

#endif /* USE_SLANG_CURSES */

#define ctrl(ch) ((ch) - '@')

#ifdef KEY_ENTER
#define CI_is_return(ch) (((ch) == '\r') || ((ch) == '\n') || ((ch) == KEY_ENTER))
#else
#define CI_is_return(ch) (((ch) == '\r') || ((ch) == '\n'))
#endif

/**
 * enum MuttCursorState - Cursor states for mutt_curses_set_cursor()
 */
enum MuttCursorState
{
  MUTT_CURSOR_RESTORE_LAST = -1, ///< Restore the previous cursor state
  MUTT_CURSOR_INVISIBLE    =  0, ///< Hide the cursor
  MUTT_CURSOR_VISIBLE      =  1, ///< Display a normal cursor
  MUTT_CURSOR_VERY_VISIBLE =  2, ///< Display a very visible cursor
};

void mutt_curses_set_attr(int attr);
void mutt_curses_set_color(enum ColorId color);
void mutt_curses_set_cursor(enum MuttCursorState state);
void mutt_resize_screen(void);

#endif /* MUTT_MUTT_CURSES_H */
