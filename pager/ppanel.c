/**
 * @file
 * Pager Panel
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
 * @page pager_ppanel Pager Panel
 *
 * The Pager Panel is a non-interactive container around the email list and a
 * status bar.
 *
 * ## Windows
 *
 * | Name        | Type      | Constructor  |
 * | :---------- | :-------- | :----------- |
 * | Pager Panel | #WT_PAGER | ppanel_new() |
 *
 * **Parent**
 * - @ref index_dlg_index
 *
 * **Children**
 * - @ref pager_pager
 * - @ref pager_pbar
 *
 * ## Data
 * - #PagerPrivateData
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type  | Handler                  |
 * | :---------- | :----------------------- |
 * | #NT_CONFIG  | ppanel_config_observer() |
 * | #NT_WINDOW  | ppanel_window_observer() |
 *
 * The Pager Panel does not implement MuttWindow::recalc() or MuttWindow::repaint().
 */

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "pbar.h"
#include "private_data.h"

struct IndexSharedData;

/**
 * ppanel_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 */
static int ppanel_config_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;
  struct MuttWindow *panel_pager = nc->global_data;

  if (mutt_str_equal(ev_c->name, "status_on_top"))
  {
    window_status_on_top(panel_pager, NeoMutt->sub);
    mutt_debug(LL_DEBUG5, "config done\n");
  }

  return 0;
}

/**
 * ppanel_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 */
static int ppanel_window_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_WINDOW)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;
  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct MuttWindow *panel_pager = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != panel_pager)
    return 0;

  notify_observer_remove(NeoMutt->sub->notify, ppanel_config_observer, panel_pager);
  notify_observer_remove(panel_pager->notify, ppanel_window_observer, panel_pager);
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
  struct MuttWindow *panel_pager = mutt_window_new(WT_PAGER, MUTT_WIN_ORIENT_VERTICAL,
                                                   MUTT_WIN_SIZE_MAXIMISE, MUTT_WIN_SIZE_UNLIMITED,
                                                   MUTT_WIN_SIZE_UNLIMITED);
  panel_pager->state.visible = false; // The Pager and Pager Bar are initially hidden

  struct PagerPrivateData *priv = pager_private_data_new();
  panel_pager->wdata = priv;
  panel_pager->wdata_free = pager_private_data_free;

  struct MuttWindow *win_pager = pager_window_new(shared, priv);
  panel_pager->focus = win_pager;

  struct MuttWindow *win_pbar = pbar_new(shared, priv);
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

  notify_observer_add(NeoMutt->sub->notify, NT_CONFIG, ppanel_config_observer, panel_pager);
  notify_observer_add(panel_pager->notify, NT_WINDOW, ppanel_window_observer, panel_pager);

  return panel_pager;
}
