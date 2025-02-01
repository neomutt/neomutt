/**
 * @file
 * Markup for text for the Simple Pager
 *
 * @authors
 * Copyright (C) 2024-2025 Richard Russon <rich@flatcap.org>
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
 * @page pfile_paged_text Markup for text for the Simple Pager
 *
 * Markup for text for the Simple Pager
 */

#include "config.h"
#include <stddef.h>
#include "paged_text.h"

/**
 * paged_text_markup_new - Create a new PagedTextMarkup
 * @param ptma PagedTextMarkupArray to add to
 * @retval ptr New PagedTextMarkup
 */
struct PagedTextMarkup *paged_text_markup_new(struct PagedTextMarkupArray *ptma)
{
  if (!ptma)
    return NULL;

  struct PagedTextMarkup ptm = { 0 };
  ARRAY_ADD(ptma, ptm);

  return ARRAY_LAST(ptma);
}
