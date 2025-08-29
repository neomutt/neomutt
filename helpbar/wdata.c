/**
 * @file
 * Help Bar Window data
 *
 * @authors
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
 * @page helpbar_wdata Data for the Help Bar
 *
 * #HelpbarWindowData stores the state of the Help Bar.
 */

#include "config.h"
#include <stddef.h>
#include "private.h"
#include "mutt/lib.h"
#include "gui/lib.h"

/**
 * helpbar_wdata_new - Create new Window data for the Helpbar
 * @retval ptr New Window data
 */
struct HelpbarWindowData *helpbar_wdata_new(void)
{
  return MUTT_MEM_CALLOC(1, struct HelpbarWindowData);
}

/**
 * helpbar_wdata_free - Free Helpbar Window data - Implements MuttWindow::wdata_free() - @ingroup window_wdata_free
 */
void helpbar_wdata_free(struct MuttWindow *win, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct HelpbarWindowData *wdata = *ptr;

  // We don't own the help_data
  FREE(&wdata->help_str);

  FREE(ptr);
}

/**
 * helpbar_wdata_get - Get the Helpbar data for this window
 * @param win Window
 */
struct HelpbarWindowData *helpbar_wdata_get(struct MuttWindow *win)
{
  if (!win || (win->type != WT_HELP_BAR))
    return NULL;

  return win->wdata;
}
