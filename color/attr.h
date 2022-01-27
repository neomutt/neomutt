/**
 * @file
 * Colour and attributes
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

#ifndef MUTT_COLOR_ATTR_H
#define MUTT_COLOR_ATTR_H

#include "config.h"
#include <stdint.h>
#include "mutt/lib.h"

/**
 * struct AttrColor - A curses colour and its attributes
 */
struct AttrColor
{
  struct CursesColor *curses_color; ///< Underlying Curses colour
  int attrs;                        ///< Text attributes, e.g. A_BOLD
  short ref_count;                  ///< Number of users
  TAILQ_ENTRY(AttrColor) entries;   ///< Linked list
};
TAILQ_HEAD(AttrColorList, AttrColor);

void              attr_color_clear (struct AttrColor *ac);
struct AttrColor  attr_color_copy  (struct AttrColor *ac);
void              attr_color_free  (struct AttrColor **ptr);
bool              attr_color_is_set(struct AttrColor *ac);
bool              attr_color_match (struct AttrColor *ac1, struct AttrColor *ac2);
struct AttrColor *attr_color_new   (void);

void              attr_color_list_clear(struct AttrColorList *acl);
struct AttrColor *attr_color_list_find (struct AttrColorList *acl, uint32_t fg, uint32_t bg, int attrs);

#endif /* MUTT_COLOR_ATTR_H */
