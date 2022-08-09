/**
 * @file
 * State of text entry
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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
 * @page enter_state State of text entry
 *
 * State of text entry
 */

#include "config.h"
#include "mutt/lib.h"
#include "state.h"

/**
 * enter_state_free - Free an EnterState
 * @param[out] ptr EnterState to free
 */
void enter_state_free(struct EnterState **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct EnterState *es = *ptr;

  FREE(&es->wbuf);
  FREE(ptr);
}

/**
 * enter_state_resize - Make the buffer bigger
 * @param es  State of the Enter buffer
 * @param num Number of wide characters
 */
void enter_state_resize(struct EnterState *es, size_t num)
{
  if (!es)
    return;

  if (num <= es->wbuflen)
    return;

  num = ROUND_UP(num + 4, 128);
  mutt_mem_realloc(&es->wbuf, num * sizeof(wchar_t));

  memset(es->wbuf + es->wbuflen, 0, (num - es->wbuflen) * sizeof(wchar_t));

  es->wbuflen = num;
}

/**
 * enter_state_new - Create a new EnterState
 * @retval ptr New EnterState
 */
struct EnterState *enter_state_new(void)
{
  struct EnterState *es = mutt_mem_calloc(1, sizeof(struct EnterState));

  enter_state_resize(es, 1);

  return es;
}
