/**
 * @file
 * Colour Stack
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_COLOR_STACK_H
#define MUTT_COLOR_STACK_H

#include <stdbool.h>
#include "color.h"

#define COLOR_STACK_MAX 16

/**
 * struct ColorStack - Stack of ColorIds
 */
struct ColorStack
{
  enum ColorId ids[COLOR_STACK_MAX]; ///< Stack entries
  int len;                           ///< Number of entries in use
};

void color_stack_clear(struct ColorStack *cs);
void color_stack_copy (struct ColorStack *dst, const struct ColorStack *src);
void color_stack_pop  (struct ColorStack *cs, int count);
bool color_stack_push (struct ColorStack *cs, enum ColorId cid);

#endif /* MUTT_COLOR_STACK_H */
