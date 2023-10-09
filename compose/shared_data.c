/**
 * @file
 * Compose Shared Data
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
 * @page compose_shared_data Compose Shared Data
 *
 * Compose Shared Data
 */

#include "config.h"
#include "mutt/lib.h"
#include "shared_data.h"

/**
 * compose_shared_data_free - Free the compose shared data - Implements MuttWindow::wdata_free() - @ingroup window_wdata_free
 */
void compose_shared_data_free(struct MuttWindow *win, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  FREE(ptr);
}

/**
 * compose_shared_data_new - Free the compose shared data
 * @retval ptr New compose shared data
 */
struct ComposeSharedData *compose_shared_data_new(void)
{
  struct ComposeSharedData *shared = mutt_mem_calloc(1, sizeof(struct ComposeSharedData));

  return shared;
}
