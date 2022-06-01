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

const int BufferStepSize = 128;

#define ROUND_UP(NUM, STEP) ((((NUM) + (STEP) -1) / (STEP)) * (STEP))

/**
 * mutt_enter_state_free - Free an EnterState
 * @param[out] ptr EnterState to free
 */
void mutt_enter_state_free(struct EnterState **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct EnterState *es = *ptr;

  FREE(&es->wbuf);
  FREE(ptr);
}

/**
 * mutt_enter_state_resize - Make the buffer bigger
 * @param es  State of the Enter buffer
 * @param num Number of wide characters
 */
void mutt_enter_state_resize(struct EnterState *es, size_t num)
{
  if (!es)
    return;

  if (num <= es->wbuflen)
    return;

  num = ROUND_UP(num, 128);
  es->wbuflen = num;
  mutt_mem_realloc(&es->wbuf, num * sizeof(wchar_t));
}

/**
 * mutt_enter_state_new - Create a new EnterState
 * @retval ptr New EnterState
 */
struct EnterState *mutt_enter_state_new(void)
{
  struct EnterState *es = mutt_mem_calloc(1, sizeof(struct EnterState));

  mutt_enter_state_resize(es, 1);

  return es;
}
