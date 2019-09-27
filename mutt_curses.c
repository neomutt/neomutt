/**
 * @file
 * Define wrapper functions around Curses/Slang
 *
 * @authors
 * Copyright (C) 1996-2000,2012 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2004 g10 Code GmbH
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

#include "config.h"
#include "mutt_curses.h"
#include "color.h"

/**
 * mutt_curses_set_color - Set the current colour for text
 * @param color Colour to set, e.g. #MT_COLOR_HEADER
 *
 * If the system has bkgdset() use it rather than attrset() so that the clr*()
 * functions will properly set the background attributes all the way to the
 * right column.
 */
void mutt_curses_set_color(enum ColorId color)
{
#ifdef HAVE_BKGDSET
  if (ColorDefs[color] != 0)
    bkgdset(ColorDefs[color] | ' ');
  else
    bkgdset(ColorDefs[MT_COLOR_NORMAL] | ' ');
#else
  if (ColorDefs[color] != 0)
    attrset(ColorDefs[color]);
  else
    attrset(ColorDefs[MT_COLOR_NORMAL]);
#endif
}
