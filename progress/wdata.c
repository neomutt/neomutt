/**
 * @file
 * Progress Bar Window Data
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
 * @page progress_wdata Progress Bar Window Data
 *
 * Progress Bar Window Data
 */

#include "config.h"
#include "mutt/lib.h"
#include "wdata.h"

/**
 * progress_wdata_new - Create new Progress Bar Window Data
 * @retval ptr New Progress Bar Window Data
 */
struct ProgressWindowData *progress_wdata_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct ProgressWindowData));
}

/**
 * progress_wdata_free - Free Progress Bar Window data - Implements MuttWindow::wdata_free() - @ingroup window_wdata_free
 */
void progress_wdata_free(struct MuttWindow *win, void **ptr)
{
  if (!win || !ptr || !*ptr)
    return;

  FREE(ptr);
}
