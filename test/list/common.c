/**
 * @file
 * Shared Testing Code
 *
 * @authors
 * Copyright (C) 2017-2018 Richard Russon <rich@flatcap.org>
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

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"

struct ListHead test_list_create(const char *items[], bool copy)
{
  struct ListHead lh = STAILQ_HEAD_INITIALIZER(lh);

  for (size_t i = 0; items[i]; i++)
  {
    struct ListNode *np = mutt_mem_calloc(1, sizeof(struct ListNode));
    if (copy)
      np->data = mutt_str_dup(items[i]);
    else
      np->data = (char *) items[i];
    STAILQ_INSERT_TAIL(&lh, np, entries);
  }

  return lh;
}
