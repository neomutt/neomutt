/**
 * @file
 * Sidebar Window data
 *
 * @authors
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020-2023 Richard Russon <rich@flatcap.org>
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

struct IndexSharedData;

/**
 * sb_wdata_new - Create new Window data for the Sidebar
 * @param win    Sidebar Window
 * @param shared Index shared data
 * @retval ptr New Window data
 */
struct SidebarWindowData *sb_wdata_new(struct MuttWindow *win, struct IndexSharedData *shared)
{
  struct SidebarWindowData *wdata = MUTT_MEM_CALLOC(1, struct SidebarWindowData);
  wdata->win = win;
  wdata->shared = shared;
  ARRAY_INIT(&wdata->entries);
  return wdata;
}

/**
 * sb_wdata_free - Free Sidebar Window data - Implements MuttWindow::wdata_free() - @ingroup window_wdata_free
 */
void sb_wdata_free(struct MuttWindow *win, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct SidebarWindowData *wdata = *ptr;

  struct SbEntry **sbep = NULL;
  ARRAY_FOREACH(sbep, &wdata->entries)
  {
    FREE(sbep);
  }
  ARRAY_FREE(&wdata->entries);

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
