/**
 * @file
 * Pattern Colour
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_COLOR_PATTERN_H
#define MUTT_COLOR_PATTERN_H

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"
#include "attr.h"
#include "color.h"

struct UserColor;

/**
 * struct PatternColor - A pattern and a color to highlight an object
 */
struct PatternColor
{
  struct AttrColor attr_color;         ///< Colour and attributes to apply
  char *pattern;                       ///< Raw Pattern string
  struct PatternList *color_pattern;   ///< Compiled pattern

  STAILQ_ENTRY(PatternColor) entries;  ///< Linked list
};
STAILQ_HEAD(PatternColorList, PatternColor);

void pattern_colors_init   (void);
void pattern_colors_reset  (void);
void pattern_colors_cleanup(void);

void                 pattern_color_clear(struct PatternColor *rcol);
void                 pattern_color_free (struct PatternColorList *list, struct PatternColor **ptr);
struct PatternColor *pattern_color_new  (void);

struct PatternColorList *pattern_colors_get_list(enum ColorId cid);

void                     pattern_color_list_clear(struct PatternColorList *pcl);
struct PatternColorList *pattern_color_list_new(void);

bool pattern_colors_parse_color_list(struct UserColor *uc, const char *pat, struct AttrColor *ac, struct Buffer *err);
bool pattern_colors_parse_uncolor   (enum ColorId cid, const char *pat);

#endif /* MUTT_COLOR_PATTERN_H */
