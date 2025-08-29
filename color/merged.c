/**
 * @file
 * Merged colours
 *
 * @authors
 * Copyright (C) 2022-2023 Richard Russon <rich@flatcap.org>
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
 * @page color_merge Merged colours
 *
 * When multiple graphical objects meet, it's often necessary to merge their
 * colours.  e.g. In the Index where the colour of the Email tree is overlaid
 * by the Indicator colour.
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "attr.h"
#include "color.h"
#include "curses2.h"

struct AttrColorList MergedColors; ///< Array of user colours

/**
 * merged_colors_init - Initialise the Merged colours
 */
void merged_colors_init(void)
{
  TAILQ_INIT(&MergedColors);
}

/**
 * merged_colors_cleanup - Free the list of Merged colours
 */
void merged_colors_cleanup(void)
{
  struct AttrColor *ac = NULL;
  struct AttrColor *tmp = NULL;

  TAILQ_FOREACH_SAFE(ac, &MergedColors, entries, tmp)
  {
    TAILQ_REMOVE(&MergedColors, ac, entries);
    curses_color_free(&ac->curses_color);
    FREE(&ac);
  }
}

/**
 * merged_colors_find - Find a Merged colour
 * @param fg    Foreground colour
 * @param bg    Background colour
 * @param attrs Attributes, e.g. A_UNDERLINE
 * @retval ptr Matching Merged colour
 */
static struct AttrColor *merged_colors_find(color_t fg, color_t bg, int attrs)
{
  struct AttrColor *ac = NULL;
  TAILQ_FOREACH(ac, &MergedColors, entries)
  {
    if (ac->attrs != attrs)
      continue;

    bool has_color = (fg != COLOR_DEFAULT) || (bg != COLOR_DEFAULT);
    struct CursesColor *cc = ac->curses_color;

    // Both have only attributes
    if (!has_color && !cc)
      return ac;

    // One has colour, but not the other
    if ((has_color && !cc) || (!has_color && cc))
      continue;

    if ((cc->fg == fg) && (cc->bg == bg))
      return ac;
  }
  return NULL;
}

/**
 * merged_color_overlay - Combine two colours
 * @param base Base colour
 * @param over Overlay colour
 * @retval ptr Merged colour
 *
 * If either the foreground or background of the overlay is 'default', then the
 * base colour will show through.  The attributes of both base and overlay will
 * be OR'd together.
 */
const struct AttrColor *merged_color_overlay(const struct AttrColor *base,
                                             const struct AttrColor *over)
{
  if (!attr_color_is_set(over))
    return base;
  if (!attr_color_is_set(base))
    return over;

  struct CursesColor *cc_base = base->curses_color;
  struct CursesColor *cc_over = over->curses_color;

  color_t fg = COLOR_DEFAULT;
  color_t bg = COLOR_DEFAULT;

  if (cc_over)
  {
    fg = cc_over->fg;
    bg = cc_over->bg;
  }

  if (cc_base)
  {
    if (fg == COLOR_DEFAULT)
      fg = cc_base->fg;
    if (bg == COLOR_DEFAULT)
      bg = cc_base->bg;
  }

  int attrs = base->attrs | over->attrs;

  struct AttrColor *ac = merged_colors_find(fg, bg, attrs);
  if (ac)
    return ac;

  ac = attr_color_new();
  ac->curses_color = curses_color_new(fg, bg);
  ac->attrs = attrs;
  ac->fg = (base->fg.color == COLOR_DEFAULT) ? over->fg : base->fg;
  ac->bg = (base->bg.color == COLOR_DEFAULT) ? over->bg : base->bg;
  TAILQ_INSERT_TAIL(&MergedColors, ac, entries);

  return ac;
}
