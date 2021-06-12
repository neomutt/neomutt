/**
 * @file
 * Index Observers
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
 * @page index_observer Index Observers
 *
 * Index Observers
 */

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "shared_data.h"

/**
 * config_pager_index_lines - React to changes to $pager_index_lines
 * @param dlg Index Dialog
 * @retval  0 Successfully handled
 * @retval -1 Error
 */
int config_pager_index_lines(struct MuttWindow *dlg)
{
  struct MuttWindow *panel_index = mutt_window_find(dlg, WT_INDEX);
  struct MuttWindow *panel_pager = mutt_window_find(dlg, WT_PAGER);
  struct MuttWindow *win_index = mutt_window_find(panel_index, WT_MENU);
  struct MuttWindow *win_pager = mutt_window_find(panel_pager, WT_MENU);
  if (!win_index || !win_pager)
    return -1;

  struct MuttWindow *parent = win_pager->parent;
  if (parent->state.visible)
  {
    struct IndexSharedData *shared = dlg->wdata;
    const short c_pager_index_lines =
        cs_subset_number(NeoMutt->sub, "pager_index_lines");
    int vcount = shared->mailbox ? shared->mailbox->vcount : 0;
    win_index->req_rows = MIN(c_pager_index_lines, vcount);
    win_index->size = MUTT_WIN_SIZE_FIXED;

    panel_index->size = MUTT_WIN_SIZE_MINIMISE;
    panel_index->state.visible = (c_pager_index_lines != 0);
  }
  else
  {
    win_index->req_rows = MUTT_WIN_SIZE_UNLIMITED;
    win_index->size = MUTT_WIN_SIZE_MAXIMISE;

    panel_index->size = MUTT_WIN_SIZE_MAXIMISE;
    panel_index->state.visible = true;
  }

  mutt_window_reflow(dlg);
  mutt_debug(LL_DEBUG5, "config, request WA_REFLOW\n");
  return 0;
}
