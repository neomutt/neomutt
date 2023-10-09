/**
 * @file
 * Compose Bar Data
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
 * @page compose_cbar_data Compose Bar Data
 *
 * Compose Bar Data
 */

#include "config.h"
#include "mutt/lib.h"
#include "cbar_data.h"

/**
 * cbar_data_free - Free the private Compose Bar data - Implements MuttWindow::wdata_free() - @ingroup window_wdata_free
 */
void cbar_data_free(struct MuttWindow *win, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct ComposeBarData *cbar_data = *ptr;

  FREE(&cbar_data->compose_format);

  FREE(ptr);
}

/**
 * cbar_data_new - Create the private data for the Compose Bar
 * @retval ptr New ComposeBarData
 */
struct ComposeBarData *cbar_data_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct ComposeBarData));
}
