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

#ifndef MUTT_COLOR_MERGED_H
#define MUTT_COLOR_MERGED_H


#include "attr.h"

const struct AttrColor * merged_color_overlay(const struct AttrColor *base, const struct AttrColor *over);

void               merged_colors_cleanup(struct AttrColorList *merged_colors);
void               merged_colors_init(struct AttrColorList *merged_colors);

#endif /* MUTT_COLOR_MERGED_H */
