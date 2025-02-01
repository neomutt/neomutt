/**
 * @file
 * Colour Dump Command
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_COLOR_DUMP_H
#define MUTT_COLOR_DUMP_H

#include <stdbool.h>
#include "mutt/lib.h"

struct AttrColor;
struct AttrColorList;
struct ColorElement;
struct PagedFile;

/**
 * struct ColorTable - Table of Colours
 */
struct ColorTable
{
  struct AttrColor *attr_color; ///< Colour
  const char *name;             ///< Colour name
  struct Slist *attrs;          ///< Attributes
  const char *fg;               ///< Foreground
  const char *bg;               ///< Background
  const char *pattern;          ///< Pattern
  int match;                    ///< Pattern match number
};
ARRAY_HEAD(ColorTableArray, struct ColorTable);

void color_dump(struct PagedFile *pf, struct AttrColorList *acl, bool all);

const char *color_log_attrs_list (int attrs);
void        color_log_color_attrs(struct AttrColor *ac, struct Buffer *swatch);
struct Slist *color_log_attrs_list2(int attrs);
const char *color_log_name       (char *buf, int buflen, struct ColorElement *elem);
int measure_attrs(const struct Slist *sl);
const char *color_log_name2(struct ColorElement *elem);

#endif /* MUTT_COLOR_DUMP_H */

