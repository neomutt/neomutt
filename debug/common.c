/**
 * @file
 * Shared debug code
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

/**
 * @page debug_common Shared debug code
 *
 * Shared debug code
 */

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"
#include "lib.h"

void add_flag(struct Buffer *buf, bool is_set, const char *name)
{
  if (!buf || !name)
    return;

  if (is_set)
  {
    if (!buf_is_empty(buf))
      buf_addch(buf, ',');
    buf_addstr(buf, name);
  }
}
