/**
 * @file
 * Curses Colour
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

#ifndef MUTT_COLOR_CURSES2_H
#define MUTT_COLOR_CURSES2_H

#include "config.h"
#include <stdint.h>
#include "mutt/lib.h"

/// Type for 24-bit colour value
typedef int32_t color_t;

/**
 * struct CursesColor - Colour in the ncurses palette
 *
 * Curses stores colours as a foreground/background pair.
 * There can be up to COLOR_PAIRS (65535) of these pairs.
 * To use a colour, it must be initialised using init_pair().
 */
struct CursesColor
{
  color_t fg;                       ///< Foreground colour
  color_t bg;                       ///< Background colour
  short index;                      ///< Index number
  short ref_count;                  ///< Number of users
  TAILQ_ENTRY(CursesColor) entries; ///< Linked list
};
TAILQ_HEAD(CursesColorList, CursesColor);

void                curses_color_free(struct CursesColor **ptr);
struct CursesColor *curses_color_new (color_t fg, color_t bg);

void                curses_colors_init(void);
struct CursesColor *curses_colors_find(color_t fg, color_t bg);

#endif /* MUTT_COLOR_CURSES2_H */
