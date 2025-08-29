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

#include "color.h"

#define COLOR_QUOTED(cid) (((cid) >= MT_COLOR_QUOTED0) && ((cid) <= MT_COLOR_QUOTED9))

void quoted_colors_init   (void);
void quoted_colors_reset  (void);
void quoted_colors_cleanup(void);

struct AttrColor * quoted_colors_get(int q);
int                quoted_colors_num_used(void);

#endif /* MUTT_COLOR_QUOTED_H */
