/**
 * @file
 * Holds state of a search
 *
 * @authors
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
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
 * @page pattern_search_state Holds state of a search
 *
 * Holds state of a search
 */

#include "config.h"
#include "mutt/lib.h"
#include "search_state.h"
#include "lib.h"

/**
 * search_state_new - Create a new SearchState
 * @retval ptr New SearchState
 */
struct SearchState *search_state_new(void)
{
  struct SearchState *s = mutt_mem_calloc(1, sizeof(struct SearchState));
  s->string = buf_pool_get();
  s->string_expn = buf_pool_get();
  return s;
}

/**
 * search_state_free - Free a SearchState
 * @param ptr SearchState to free
 */
void search_state_free(struct SearchState **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct SearchState *s = *ptr;
  mutt_pattern_free(&s->pattern);
  buf_pool_release(&s->string);
  buf_pool_release(&s->string_expn);

  FREE(ptr);
}
