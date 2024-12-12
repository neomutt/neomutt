/**
 * @file
 * Quoted-Email colours
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

#ifndef MUTT_COLOR_QUOTED_H
#define MUTT_COLOR_QUOTED_H

#include "config.h"
#include <stdbool.h>
#include "core/lib.h"
#include "attr.h"
#include "color.h"

struct Buffer;

/// Ten colours, quoted0..quoted9 (quoted and quoted0 are equivalent)
#define COLOR_QUOTES_MAX 10

extern struct AttrColor QuotedColors[];

#define COLOR_QUOTED(cid) ((cid) == MT_COLOR_QUOTED)

void quoted_colors_init   (void);
void quoted_colors_reset  (void);
void quoted_colors_cleanup(void);

struct AttrColor * quoted_colors_get(int q);
int                quoted_colors_num_used(void);

bool               quoted_colors_parse_color  (enum ColorId cid, struct AttrColor *ac_val, int q_level, int *rc, struct Buffer *err);
enum CommandResult quoted_colors_parse_uncolor(enum ColorId cid, int q_level, struct Buffer *err);

#endif /* MUTT_COLOR_QUOTED_H */
