/**
 * @file
 * Simple colour
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
 * @page color_simple Simple colour
 *
 * Manage the colours of the 'simple' graphical objects -- those that can only
 * have one colour, plus attributes.
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "gui/lib.h"

int SimpleColors[MT_COLOR_MAX]; ///< Array of all fixed colours, see enum ColorId

/**
 * simple_colors_init - Initialise the simple colour definitions
 */
void simple_colors_init(void)
{
  memset(SimpleColors, A_NORMAL, MT_COLOR_MAX * sizeof(int));

  // Set some defaults
  SimpleColors[MT_COLOR_INDICATOR] = A_REVERSE;
  SimpleColors[MT_COLOR_MARKERS] = A_REVERSE;
  SimpleColors[MT_COLOR_SEARCH] = A_REVERSE;
#ifdef USE_SIDEBAR
  SimpleColors[MT_COLOR_SIDEBAR_HIGHLIGHT] = A_UNDERLINE;
#endif
  SimpleColors[MT_COLOR_STATUS] = A_REVERSE;
}

/**
 * simple_colors_clear - Reset the simple colour definitions
 */
void simple_colors_clear(void)
{
  memset(SimpleColors, A_NORMAL, MT_COLOR_MAX * sizeof(int));
}

/**
 * simple_colors_get - Get the colour of an object by its ID
 * @param id Colour ID, e.g. #MT_COLOR_SEARCH
 * @retval num Color of the object
 */
int simple_colors_get(enum ColorId id)
{
  if (id >= MT_COLOR_MAX)
  {
    mutt_error("colour overflow %d", id);
    return 0;
  }
  if (id <= MT_COLOR_NONE)
  {
    mutt_error("colour underflow %d", id);
    return 0;
  }

  return SimpleColors[id];
}

/**
 * simple_color_is_set - Is the object coloured?
 * @param id Colour ID, e.g. #MT_COLOR_SEARCH
 * @retval true Yes, a 'color' command has been used on this object
 */
bool simple_color_is_set(enum ColorId id)
{
  int color = simple_colors_get(id);

  return (color > 0);
}

/**
 * simple_color_is_header - Colour is for an Email header
 * @param color_id Colour, e.g. #MT_COLOR_HEADER
 * @retval true Colour is for an Email header
 */
bool simple_color_is_header(enum ColorId color_id)
{
  return (color_id == MT_COLOR_HEADER) || (color_id == MT_COLOR_HDRDEFAULT);
}
