/**
 * @file
 * Message Window private data
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
 * @page gui_msgwin_wdata Message Window private data
 *
 * Message Window private data
 */

#include "config.h"
#include "mutt/lib.h"
#include "msgwin_wdata.h"

/**
 * msgwin_wdata_free - Free the private data attached to the Message Window - Implements MuttWindow::wdata_free() - @ingroup window_wdata_free
 */
void msgwin_wdata_free(struct MuttWindow *win, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct MsgWinWindowData *wdata = *ptr;

  FREE(&wdata->text);

  FREE(ptr);
}

/**
 * msgwin_wdata_new - Create new private data for the Message Window
 * @retval ptr New private data
 */
struct MsgWinWindowData *msgwin_wdata_new(void)
{
  struct MsgWinWindowData *msgwin_data = mutt_mem_calloc(1, sizeof(struct MsgWinWindowData));

  msgwin_data->cid = MT_COLOR_NORMAL;

  return msgwin_data;
}
