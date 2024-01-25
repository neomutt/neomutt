/**
 * @file
 * Colour and attributes
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

#ifndef MUTT_COLOR_ATTR_H
#define MUTT_COLOR_ATTR_H

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"
#include "curses2.h"

/**
 * enum ColorType - Type of Colour
 */
enum ColorType
{
  CT_SIMPLE,    ///< Simple colour,  e.g. "Red"
  CT_PALETTE,   ///< Palette colour, e.g. "color207"
  CT_RGB,       ///< True colour,    e.g. "#11AAFF"
};

/**
 * ColorPrefix - Constants for colour prefixes of named colours
 */
enum ColorPrefix
{
  COLOR_PREFIX_NONE,   ///< no prefix
  COLOR_PREFIX_ALERT,  ///< "alert"  colour prefix
  COLOR_PREFIX_BRIGHT, ///< "bright" colour prefix
  COLOR_PREFIX_LIGHT,  ///< "light"  colour prefix
};

/**
 * struct ColorElement - One element of a Colour
 */
struct ColorElement
{
  color_t          color;           ///< Colour
  enum ColorType   type;            ///< Type of Colour
  enum ColorPrefix prefix;          ///< Optional Colour Modifier
};

/**
 * struct AttrColor - A curses colour and its attributes
 */
struct AttrColor
{
  struct ColorElement fg;           ///< Foreground colour
  struct ColorElement bg;           ///< Background colour
  int attrs;                        ///< Text attributes, e.g. A_BOLD
  struct CursesColor *curses_color; ///< Underlying Curses colour
  short ref_count;                  ///< Number of users
  TAILQ_ENTRY(AttrColor) entries;   ///< Linked list
};
TAILQ_HEAD(AttrColorList, AttrColor);

void              attr_color_clear (struct AttrColor *ac);
struct AttrColor  attr_color_copy  (const struct AttrColor *ac);
void              attr_color_free  (struct AttrColor **ptr);
bool              attr_color_is_set(const struct AttrColor *ac);
bool              attr_color_match (struct AttrColor *ac1, struct AttrColor *ac2);
struct AttrColor *attr_color_new   (void);

void              attr_color_list_clear(struct AttrColorList *acl);
struct AttrColor *attr_color_list_find (struct AttrColorList *acl, color_t fg, color_t bg, int attrs);

void attr_color_overwrite(struct AttrColor *ac_old, struct AttrColor *ac_new);

#endif /* MUTT_COLOR_ATTR_H */
