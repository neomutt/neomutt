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

#ifndef MUTT_COLOR_SIMPLE2_H
#define MUTT_COLOR_SIMPLE2_H

#include "config.h"
#include <stdbool.h>
#include "attr.h"
#include "color.h"

extern struct AttrColor SimpleColors[];

struct AttrColor *simple_color_get      (enum ColorId cid);
bool              simple_color_is_header(enum ColorId cid);
bool              simple_color_is_set   (enum ColorId cid);
void              simple_color_reset    (enum ColorId cid);
struct AttrColor *simple_color_set      (enum ColorId cid, int fg, int bg, int attrs);

void              simple_colors_clear(void);
void              simple_colors_init(void);

#endif /* MUTT_COLOR_SIMPLE2_H */
