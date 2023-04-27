/**
 * @file
 * View of an Email
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
 * @page core_eview View of an Email
 *
 * View of an Email
 */

#include "config.h"
#include "mutt/lib.h"
#include "eview.h"

struct Email;

/**
 * eview_free - Free an Email View
 * @param ptr Email View to free
 */
void eview_free(struct EmailView **ptr)
{
  if (!ptr || !*ptr)
    return;

  // struct EmailView *ev = *ptr;

  FREE(ptr);
}

/**
 * eview_new - Create a new view of an Email
 * @param e Email to wrap
 * @retval ptr New Email View
 */
struct EmailView *eview_new(struct Email *e)
{
  struct EmailView *ev = mutt_mem_calloc(1, sizeof(struct EmailView));

  ev->email = e;

  return ev;
}
