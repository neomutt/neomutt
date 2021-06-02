/**
 * @file
 * Compose Attach Data
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
 * @page compose_attach_data Compose Attach Data
 *
 * Compose Attach Data
 */

#include "config.h"
#include "mutt/lib.h"
#include "attach_data.h"

/**
 * attach_data_free - Free the Compose Attach Data - Implements MuttWindow::wdata_free()
 */
void attach_data_free(struct MuttWindow *win, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  // struct ComposeAttachData *attach_data = *ptr;

  FREE(ptr);
}

/**
 * attach_data_new - Create new Compose Attach Data
 * @retval ptr New Compose Attach Data
 */
struct ComposeAttachData *attach_data_new(void)
{
  struct ComposeAttachData *attach_data =
      mutt_mem_calloc(1, sizeof(struct ComposeAttachData));

  return attach_data;
}
