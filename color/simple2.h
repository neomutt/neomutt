/**
 * @file
 * Simple colour
 *
 * @authors
 * Copyright (C) 2021-2023 Richard Russon <rich@flatcap.org>
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
#include "color.h"

struct AttrColor;

#define COLOR_COMPOSE(cid) (((cid) >= MT_COLOR_COMPOSE_HEADER) && ((cid) <= MT_COLOR_COMPOSE_SECURITY_SIGN))

void simple_colors_init   (void);
void simple_colors_reset  (void);
void simple_colors_cleanup(void);

struct AttrColor *simple_color_get      (enum ColorId cid);
bool              simple_color_is_set   (enum ColorId cid);
void              simple_color_reset    (enum ColorId cid);
struct AttrColor *simple_color_set      (enum ColorId cid, struct AttrColor *ac_val);

#endif /* MUTT_COLOR_SIMPLE2_H */
