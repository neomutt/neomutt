/**
 * @file
 * GUI display a file/email/help in a viewport with paging
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
 * @page pager_pager GUI display a file/email/help in a viewport with paging
 *
 * GUI display a file/email/help in a viewport with paging
 */

#include "config.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h> // IWYU pragma: keep
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wchar.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "lib.h"
#include "index/lib.h"
#include "menu/lib.h"
#include "ncrypt/lib.h"
#include "question/lib.h"
#include "send/lib.h"
#include "commands.h"
#include "context.h"
#include "format_flags.h"
#include "hdrline.h"
#include "hook.h"
#include "keymap.h"
#include "mutt_attach.h"
#include "mutt_globals.h"
#include "mutt_header.h"
#include "mutt_logging.h"
#include "mutt_mailbox.h"
#include "muttlib.h"
#include "mx.h"
#include "opcodes.h"
#include "options.h"
#include "pbar.h"
#include "private_data.h"
#include "protos.h"
#include "recvattach.h"
#include "recvcmd.h"
#include "status.h"
#ifdef USE_SIDEBAR
#include "sidebar/lib.h"
#endif
#ifdef USE_NNTP
#include "nntp/lib.h"
#include "nntp/mdata.h" // IWYU pragma: keep
#endif
#ifdef ENABLE_NLS
#include <libintl.h>
#endif

/**
 * config_pager_index_lines - React to changes to $pager_index_lines
 * @param win Pager Window
 * @retval  0 Successfully handled
 * @retval -1 Error
 */
static int config_pager_index_lines(struct MuttWindow *win)
{
  if (!mutt_window_is_visible(win))
    return 0;

  struct MuttWindow *dlg = dialog_find(win);
  struct MuttWindow *panel_index = mutt_window_find(dlg, WT_INDEX);
  struct MuttWindow *win_index = mutt_window_find(panel_index, WT_MENU);
  if (!win_index)
    return -1;

  const short c_pager_index_lines =
      cs_subset_number(NeoMutt->sub, "pager_index_lines");

  if (c_pager_index_lines > 0)
  {
    struct IndexSharedData *shared = dlg->wdata;
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

/**
 * pager_config_observer - Listen for config changes affecting the Pager - Implements ::observer_t
 */
static int pager_config_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;

  struct EventConfig *ev_c = nc->event_data;
  struct MuttWindow *win_pager = nc->global_data;

  if (mutt_str_equal(ev_c->name, "pager_index_lines"))
    return config_pager_index_lines(win_pager);

  mutt_debug(LL_DEBUG5, "config done\n");
  return 0;
}

/**
 * pager_window_observer - Listen for window changes affecting the Pager - Implements ::observer_t
 */
static int pager_window_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_WINDOW)
    return 0;
  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct MuttWindow *win = nc->global_data;

  notify_observer_remove(NeoMutt->notify, pager_config_observer, win);
  notify_observer_remove(win->notify, pager_window_observer, win);

  mutt_debug(LL_DEBUG5, "window delete done\n");

  return 0;
}

/**
 * pager_window_new - Create a new Pager Window (list of Emails)
 * @retval ptr New Window
 */
struct MuttWindow *pager_window_new(void)
{
  struct MuttWindow *win = menu_new_window(MENU_PAGER, NeoMutt->sub);

  notify_observer_add(NeoMutt->notify, NT_CONFIG, pager_config_observer, win);
  notify_observer_add(win->notify, NT_WINDOW, pager_window_observer, win);

  return win;
}
