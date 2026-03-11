/**
 * @file
 * Colour Stack utilities
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

/**
 * @page color_stack Colour Stack utilities
 *
 * Manage a stack of ColorIds for screenshot capture, conditional on
 * './configure --debug-screenshot'
 */

#include "config.h"
#include <stdbool.h>
#include <string.h>
#include "stack.h"

/**
 * color_stack_clear - Reset a ColorStack
 * @param cs ColorStack to reset
 */
void color_stack_clear(struct ColorStack *cs)
{
  if (!cs)
    return;

  cs->len = 0;
}

/**
 * color_stack_push - Push a ColorId onto a ColorStack
 * @param cs  ColorStack to update
 * @param cid ColorId to push
 * @retval true  Success
 * @retval false Stack full or invalid
 */
bool color_stack_push(struct ColorStack *cs, enum ColorId cid)
{
  if (!cs || (cs->len >= COLOR_STACK_MAX))
    return false;

  cs->ids[cs->len++] = cid;
  return true;
}

/**
 * color_stack_pop - Pop entries from a ColorStack
 * @param cs    ColorStack to update
 * @param count Number of entries to pop
 */
void color_stack_pop(struct ColorStack *cs, int count)
{
  if (!cs || (count <= 0))
    return;

  if (count > cs->len)
    count = cs->len;
  cs->len -= count;
}

/**
 * color_stack_copy - Copy a ColorStack
 * @param dst Destination ColorStack
 * @param src Source ColorStack
 */
void color_stack_copy(struct ColorStack *dst, const struct ColorStack *src)
{
  if (!dst || !src)
    return;

  memcpy(dst, src, sizeof(*dst));
}
