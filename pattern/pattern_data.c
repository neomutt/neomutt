/**
 * @file
 * Private Pattern Data
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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
 * @page pattern_pattern_data Private Pattern Data
 *
 * Private Pattern Data
 */

#include "config.h"
#include <stddef.h>
#include "pattern_data.h"

/**
 * pattern_data_new - Create new Pattern Data
 * @retval ptr New Pattern Data
 */
struct PatternData *pattern_data_new(void)
{
  struct PatternData *pd = mutt_mem_calloc(1, sizeof(struct PatternData));

  ARRAY_INIT(&pd->entries);

  return pd;
}

/**
 * pattern_data_free - Free Pattern Data - Implements Menu::mdata_free() - @ingroup menu_mdata_free
 */
void pattern_data_free(struct Menu *menu, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct PatternData *pd = *ptr;

  struct PatternEntry *pe = NULL;
  ARRAY_FOREACH(pe, &pd->entries)
  {
    FREE(&pe->tag);
    FREE(&pe->expr);
    FREE(&pe->desc);
  }

  ARRAY_FREE(&pd->entries);
  FREE(ptr);
}
