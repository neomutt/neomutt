/**
 * @file
 * Sidebar Window data
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page sidebar_wdata Sidebar Window data
 *
 * Sidebar Window data
 */

#include "config.h"
#include <stddef.h>
#include "private.h"
#include "mutt/lib.h"
#include "gui/lib.h"

/**
 * sb_wdata_new - Create new Window data for the Sidebar
 * @retval ptr New Window data
 */
struct SidebarWindowData *sb_wdata_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct SidebarWindowData));
}

/**
 * sb_wdata_free - Free Sidebar Window data
 * @param win Sidebar Window
 * @param ptr Window data to free
 */
void sb_wdata_free(struct MuttWindow *win, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct SidebarWindowData *wdata = *ptr;

  for (size_t i = 0; i < wdata->entry_count; i++)
    FREE(&wdata->entries[i]);

  FREE(&wdata->entries);

  FREE(ptr);
}

/**
 * sb_wdata_get - Get the Sidebar data for this window
 * @param win Window
 */
struct SidebarWindowData *sb_wdata_get(struct MuttWindow *win)
{
  if (!win || (win->type != WT_SIDEBAR))
    return NULL;

  return win->wdata;
}
