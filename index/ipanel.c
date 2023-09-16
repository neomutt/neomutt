/**
 * @file
 * Index Panel
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
 * @page index_ipanel Index Panel
 *
 * The Index Panel is a non-interactive container around the email list and a
 * status bar.
 *
 * ## Windows
 *
 * | Name        | Type      | Constructor  |
 * | :---------- | :-------- | :----------- |
 * | Index Panel | #WT_INDEX | ipanel_new() |
 *
 * **Parent**
 * - @ref index_dlg_index
 *
 * **Children**
 * - @ref index_index
 * - @ref index_ibar
 *
 * ## Data
 * - #IndexPrivateData
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type  | Handler                  |
 * | :---------- | :----------------------- |
 * | #NT_CONFIG  | ipanel_config_observer() |
 * | #NT_WINDOW  | ipanel_window_observer() |
 *
 * The Index Panel does not implement MuttWindow::recalc() or MuttWindow::repaint().
 */

#include "config.h"
#include <stdbool.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "ibar.h"
#include "private_data.h"

struct IndexSharedData;

/**
 * ipanel_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 */
static int ipanel_config_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;
  struct MuttWindow *panel_index = nc->global_data;

  if (mutt_str_equal(ev_c->name, "status_on_top"))
  {
    window_status_on_top(panel_index, NeoMutt->sub);
    mutt_debug(LL_DEBUG5, "config done\n");
  }

  return 0;
}

/**
 * ipanel_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 */
static int ipanel_window_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_WINDOW)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;
  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct MuttWindow *panel_index = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != panel_index)
    return 0;

  notify_observer_remove(NeoMutt->sub->notify, ipanel_config_observer, panel_index);
  notify_observer_remove(panel_index->notify, ipanel_window_observer, panel_index);
  mutt_debug(LL_DEBUG5, "window delete done\n");

  return 0;
}

/**
 * ipanel_new - Create the Windows for the Index panel
 * @param status_on_top true, if the Index bar should be on top
 * @param shared        Shared Index data
 * @retval ptr New Index Panel
 */
struct MuttWindow *ipanel_new(bool status_on_top, struct IndexSharedData *shared)
{
  struct MuttWindow *panel_index = mutt_window_new(WT_INDEX, MUTT_WIN_ORIENT_VERTICAL,
                                                   MUTT_WIN_SIZE_MAXIMISE, MUTT_WIN_SIZE_UNLIMITED,
                                                   MUTT_WIN_SIZE_UNLIMITED);

  struct IndexPrivateData *priv = index_private_data_new(shared);
  panel_index->wdata = priv;
  panel_index->wdata_free = index_private_data_free;

  struct MuttWindow *win_index = index_window_new(priv);

  struct MuttWindow *win_ibar = ibar_new(panel_index, shared, priv);
  if (status_on_top)
  {
    mutt_window_add_child(panel_index, win_ibar);
    mutt_window_add_child(panel_index, win_index);
  }
  else
  {
    mutt_window_add_child(panel_index, win_index);
    mutt_window_add_child(panel_index, win_ibar);
  }

  notify_observer_add(NeoMutt->sub->notify, NT_CONFIG, ipanel_config_observer, panel_index);
  notify_observer_add(panel_index->notify, NT_WINDOW, ipanel_window_observer, panel_index);

  return panel_index;
}
