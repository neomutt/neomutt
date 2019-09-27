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

/* The prototypes for these four functions use "(char*)",
 * whereas the ncurses versions use "(const char*)" */
#undef addnstr
#undef addstr
#undef mvaddnstr
#undef mvaddstr

/* We redefine the helper macros to hide the compiler warnings */
#define addnstr(str, n)         waddnstr(stdscr, ((char *) str), (n))
#define addstr(x)               waddstr(stdscr, ((char *) x))
#define mvaddnstr(y, x, str, n) mvwaddnstr(stdscr, (y), (x), ((char *) str), (n))
#define mvaddstr(y, x, str)     mvwaddstr(stdscr, (y), (x), ((char *) str))

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

#define BEEP()                                                                 \
  do                                                                           \
  {                                                                            \
    if (C_Beep)                                                                \
      beep();                                                                  \
  } while (false)

#if !(defined(USE_SLANG_CURSES) || defined(HAVE_CURS_SET))
#define curs_set(x)
#endif

#if (defined(USE_SLANG_CURSES) || defined(HAVE_CURS_SET))
void mutt_curs_set(int cursor);
#else
#define mutt_curs_set(x)
#endif

#define ctrl(ch) ((ch) - '@')

#ifdef KEY_ENTER
#define CI_is_return(ch) (((ch) == '\r') || ((ch) == '\n') || ((ch) == KEY_ENTER))
#else
#define CI_is_return(ch) (((ch) == '\r') || ((ch) == '\n'))
#endif

/**
 * struct KeyEvent - An event such as a keypress
 */
struct KeyEvent
{
  int ch; /**< raw key pressed */
  int op; /**< function op */
};

void mutt_resize_screen(void);

/* If the system has bkgdset() use it rather than attrset() so that the clr*()
 * functions will properly set the background attributes all the way to the
 * right column.
 */
#ifdef HAVE_BKGDSET
#define SET_COLOR(X)                                                            \
  do                                                                           \
  {                                                                            \
    if (ColorDefs[X] != 0)                                                     \
      bkgdset(ColorDefs[X] | ' ');                                             \
    else                                                                       \
      bkgdset(ColorDefs[MT_COLOR_NORMAL] | ' ');                               \
  } while (false)
#define ATTR_SET(X) bkgdset(X | ' ')
#else
#define SET_COLOR(X)                                                            \
  do                                                                           \
  {                                                                            \
    if (ColorDefs[X] != 0)                                                     \
      attrset(ColorDefs[X]);                                                   \
    else                                                                       \
      attrset(ColorDefs[MT_COLOR_NORMAL]);                                     \
  } while (false)
#define ATTR_SET attrset
#endif

/* reset the color to the normal terminal color as defined by 'color normal ...' */
#ifdef HAVE_BKGDSET
#define NORMAL_COLOR bkgdset(ColorDefs[MT_COLOR_NORMAL] | ' ')
#else
#define NORMAL_COLOR attrset(ColorDefs[MT_COLOR_NORMAL])
#endif

#endif /* MUTT_MUTT_CURSES_H */
