/**
 * @file
 * Mixmaster Chain Data
 *
 * @authors
 * Copyright (C) 2022 Richard Russon <rich@flatcap.org>
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
 * @page mixmaster_chain_data Mixmaster Chain Data
 *
 * Mixmaster Chain Data
 */

#include "config.h"
#include "mutt/lib.h"
#include "chain_data.h"

/**
 * chain_data_new - Create new Chain data
 * @retval ptr New Chain data
 */
struct ChainData *chain_data_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct ChainData));
}

/**
 * chain_data_free - Free the Chain data - Implements MuttWindow::wdata_free() - @ingroup window_wdata_free
 */
void chain_data_free(struct MuttWindow *win, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  FREE(ptr);
}
