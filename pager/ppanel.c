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
 * @page pager_ppanel GUI display a file/email/help in a viewport with paging
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
#include "menu/lib.h"
#include "ncrypt/lib.h"
#include "pager/lib.h"
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
 * ppanel_config_observer - Notification that a Config Variable has changed - Implements ::observer_t
 */
static int ppanel_config_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_CONFIG) || !nc->global_data || !nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;
  struct MuttWindow *panel_pager = nc->global_data;

  if (mutt_str_equal(ev_c->name, "status_on_top"))
    window_status_on_top(panel_pager, NeoMutt->sub);

  mutt_debug(LL_DEBUG5, "config done\n");
  return 0;
}

/**
 * ppanel_window_observer - Notification that a Window has changed - Implements ::observer_t
 */
static int ppanel_window_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_WINDOW) || !nc->global_data || !nc->event_data)
    return -1;

  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct MuttWindow *panel_pager = nc->global_data;

  notify_observer_remove(NeoMutt->notify, ppanel_config_observer, panel_pager);
  notify_observer_remove(NeoMutt->notify, ppanel_window_observer, panel_pager);
  mutt_debug(LL_DEBUG5, "window delete done\n");

  return 0;
}

/**
 * ppanel_new - Create the Windows for the Pager panel
 * @param status_on_top true, if the Pager bar should be on top
 * @param shared        Shared Index data
 * @retval ptr New Pager Panel
 */
struct MuttWindow *ppanel_new(bool status_on_top, struct IndexSharedData *shared)
{
  struct MuttWindow *panel_pager =
      mutt_window_new(WT_PAGER, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  panel_pager->state.visible = false; // The Pager and Pager Bar are initially hidden

  struct PagerPrivateData *priv = pager_private_data_new();
  panel_pager->wdata = priv;
  panel_pager->wdata_free = pager_private_data_free;

  struct MuttWindow *win_pager = pager_window_new(panel_pager, shared, priv);
  panel_pager->focus = win_pager;

  struct MuttWindow *win_pbar = pbar_new(panel_pager, shared, priv);
  if (status_on_top)
  {
    mutt_window_add_child(panel_pager, win_pbar);
    mutt_window_add_child(panel_pager, win_pager);
  }
  else
  {
    mutt_window_add_child(panel_pager, win_pager);
    mutt_window_add_child(panel_pager, win_pbar);
  }

  notify_observer_add(NeoMutt->notify, NT_CONFIG, ppanel_config_observer, panel_pager);
  notify_observer_add(NeoMutt->notify, NT_WINDOW, ppanel_window_observer, panel_pager);

  return panel_pager;
}
