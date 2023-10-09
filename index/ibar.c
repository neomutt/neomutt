/**
 * @file
 * Index Bar (status)
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
 * @page index_ibar Index Bar (status)
 *
 * The Index Bar Window displays status info about the email list.
 *
 * ## Windows
 *
 * | Name             | Type          | See Also   |
 * | :--------------- | :------------ | :--------- |
 * | Index Bar Window | WT_STATUS_BAR | ibar_new() |
 *
 * **Parent**
 * - @ref index_ipanel
 *
 * **Children**
 *
 * None.
 *
 * ## Data
 * - #IBarPrivateData
 *
 * The Index Bar Window stores its data (#IBarPrivateData) in
 * MuttWindow::wdata.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type            | Handler                 |
 * | :-------------------- | :---------------------- |
 * | #NT_COLOR             | ibar_color_observer()   |
 * | #NT_CONFIG            | ibar_config_observer()  |
 * | #NT_INDEX             | ibar_index_observer()   |
 * | #NT_MENU              | ibar_menu_observer()    |
 * | #NT_WINDOW            | ibar_window_observer()  |
 * | MuttWindow::recalc()  | ibar_recalc()           |
 * | MuttWindow::repaint() | ibar_repaint()          |
 */

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "color/lib.h"
#include "private_data.h"
#include "shared_data.h"
#include "status.h"

/**
 * struct IBarPrivateData - Data to draw the Index Bar
 */
struct IBarPrivateData
{
  struct IndexSharedData *shared; ///< Shared Index data
  struct IndexPrivateData *priv;  ///< Private Index data
  char *status_format;            ///< Cached screen status string
  char *ts_status_format;         ///< Cached terminal status string
  char *ts_icon_format;           ///< Cached terminal icon string
};

/**
 * ibar_recalc - Recalculate the Window data - Implements MuttWindow::recalc() - @ingroup window_recalc
 */
static int ibar_recalc(struct MuttWindow *win)
{
  char buf[1024] = { 0 };

  struct IBarPrivateData *ibar_data = win->wdata;
  struct IndexSharedData *shared = ibar_data->shared;
  struct IndexPrivateData *priv = ibar_data->priv;

  const char *c_status_format = cs_subset_string(shared->sub, "status_format");
  menu_status_line(buf, sizeof(buf), shared, priv->menu, win->state.cols,
                   NONULL(c_status_format));

  if (!mutt_str_equal(buf, ibar_data->status_format))
  {
    mutt_str_replace(&ibar_data->status_format, buf);
    win->actions |= WA_REPAINT;
    mutt_debug(LL_DEBUG5, "recalc done, request WA_REPAINT\n");
  }

  const bool c_ts_enabled = cs_subset_bool(shared->sub, "ts_enabled");
  if (c_ts_enabled && TsSupported)
  {
    const char *c_ts_status_format = cs_subset_string(shared->sub, "ts_status_format");
    menu_status_line(buf, sizeof(buf), shared, priv->menu, sizeof(buf),
                     NONULL(c_ts_status_format));
    if (!mutt_str_equal(buf, ibar_data->ts_status_format))
    {
      mutt_str_replace(&ibar_data->ts_status_format, buf);
      win->actions |= WA_REPAINT;
      mutt_debug(LL_DEBUG5, "recalc done, request WA_REPAINT\n");
    }

    const char *c_ts_icon_format = cs_subset_string(shared->sub, "ts_icon_format");
    menu_status_line(buf, sizeof(buf), shared, priv->menu, sizeof(buf),
                     NONULL(c_ts_icon_format));
    if (!mutt_str_equal(buf, ibar_data->ts_icon_format))
    {
      mutt_str_replace(&ibar_data->ts_icon_format, buf);
      win->actions |= WA_REPAINT;
      mutt_debug(LL_DEBUG5, "recalc done, request WA_REPAINT\n");
    }
  }

  return 0;
}

/**
 * ibar_repaint - Repaint the Window - Implements MuttWindow::repaint() - @ingroup window_repaint
 */
static int ibar_repaint(struct MuttWindow *win)
{
  struct IBarPrivateData *ibar_data = win->wdata;
  struct IndexSharedData *shared = ibar_data->shared;

  mutt_window_move(win, 0, 0);
  mutt_curses_set_normal_backed_color_by_id(MT_COLOR_STATUS);
  mutt_window_clrtoeol(win);

  mutt_window_move(win, 0, 0);
  mutt_draw_statusline(win, win->state.cols, ibar_data->status_format,
                       mutt_str_len(ibar_data->status_format));
  mutt_curses_set_color_by_id(MT_COLOR_NORMAL);

  const bool c_ts_enabled = cs_subset_bool(shared->sub, "ts_enabled");
  if (c_ts_enabled && TsSupported)
  {
    mutt_ts_status(ibar_data->ts_status_format);
    mutt_ts_icon(ibar_data->ts_icon_format);
  }

  mutt_debug(LL_DEBUG5, "repaint done\n");
  return 0;
}

/**
 * ibar_color_observer - Notification that a Color has changed - Implements ::observer_t - @ingroup observer_api
 */
static int ibar_color_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_COLOR)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct EventColor *ev_c = nc->event_data;

  // MT_COLOR_MAX is sent on `uncolor *`
  if ((ev_c->cid != MT_COLOR_STATUS) && (ev_c->cid != MT_COLOR_NORMAL) &&
      (ev_c->cid != MT_COLOR_MAX))
  {
    return 0;
  }

  struct MuttWindow *win_ibar = nc->global_data;
  win_ibar->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "color done, request WA_REPAINT\n");

  return 0;
}

/**
 * ibar_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 */
static int ibar_config_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;
  if (!ev_c->name)
    return 0;
  if ((ev_c->name[0] != 's') && (ev_c->name[0] != 't'))
    return 0;

  if (!mutt_str_equal(ev_c->name, "status_format") &&
      !mutt_str_equal(ev_c->name, "ts_enabled") &&
      !mutt_str_equal(ev_c->name, "ts_icon_format") &&
      !mutt_str_equal(ev_c->name, "ts_status_format"))
  {
    return 0;
  }

  struct MuttWindow *win_ibar = nc->global_data;
  win_ibar->actions |= WA_RECALC;
  mutt_debug(LL_DEBUG5, "config done, request WA_RECALC\n");

  return 0;
}

/**
 * ibar_index_observer - Notification that the Index has changed - Implements ::observer_t - @ingroup observer_api
 *
 * This function receives two sorts of notification:
 * - NT_INDEX:
 *   User has changed to a different Mailbox/Email
 * - NT_ACCOUNT/NT_MVIEW/NT_MAILBOX/NT_EMAIL:
 *   The state of an object has changed
 */
static int ibar_index_observer(struct NotifyCallback *nc)
{
  if (!nc->global_data)
    return -1;

  struct MuttWindow *win_ibar = nc->global_data;
  win_ibar->actions |= WA_RECALC;
  mutt_debug(LL_DEBUG5, "index done, request WA_RECALC\n");

  return 0;
}

/**
 * ibar_menu_observer - Notification that a Menu has changed - Implements ::observer_t - @ingroup observer_api
 */
static int ibar_menu_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_MENU)
    return 0;
  if (!nc->global_data)
    return -1;

  struct MuttWindow *win_ibar = nc->global_data;
  win_ibar->actions |= WA_RECALC;
  mutt_debug(LL_DEBUG5, "menu done, request WA_RECALC\n");

  return 0;
}

/**
 * ibar_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 */
static int ibar_window_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_WINDOW)
    return 0;
  if (!nc->global_data)
    return -1;

  struct MuttWindow *win_ibar = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win_ibar)
    return 0;

  if (nc->event_subtype == NT_WINDOW_STATE)
  {
    win_ibar->actions |= WA_REPAINT;
    mutt_debug(LL_DEBUG5, "window state done, request WA_REPAINT\n");
  }
  else if (nc->event_subtype == NT_WINDOW_DELETE)
  {
    struct MuttWindow *dlg = window_find_parent(win_ibar, WT_DLG_INDEX);
    struct IndexSharedData *shared = dlg->wdata;

    mutt_color_observer_remove(ibar_color_observer, win_ibar);
    notify_observer_remove(NeoMutt->sub->notify, ibar_config_observer, win_ibar);
    notify_observer_remove(shared->notify, ibar_index_observer, win_ibar);
    notify_observer_remove(win_ibar->parent->notify, ibar_menu_observer, win_ibar);
    notify_observer_remove(win_ibar->notify, ibar_window_observer, win_ibar);

    mutt_debug(LL_DEBUG5, "window delete done\n");
  }

  return 0;
}

/**
 * ibar_data_free - Free the private data - Implements MuttWindow::wdata_free() - @ingroup window_wdata_free
 */
static void ibar_data_free(struct MuttWindow *win, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct IBarPrivateData *ibar_data = *ptr;

  FREE(&ibar_data->status_format);
  FREE(&ibar_data->ts_status_format);
  FREE(&ibar_data->ts_icon_format);

  FREE(ptr);
}

/**
 * ibar_data_new - Create the private data for the Index Bar (status)
 * @param shared Shared Index data
 * @param priv   Private Index data
 * @retval ptr New IBarPrivateData
 */
static struct IBarPrivateData *ibar_data_new(struct IndexSharedData *shared,
                                             struct IndexPrivateData *priv)
{
  struct IBarPrivateData *ibar_data = mutt_mem_calloc(1, sizeof(struct IBarPrivateData));

  ibar_data->shared = shared;
  ibar_data->priv = priv;

  return ibar_data;
}

/**
 * ibar_new - Create the Index Bar (status)
 * @param parent Parent Window
 * @param shared Shared Index data
 * @param priv   Private Index data
 * @retval ptr New Index Bar
 */
struct MuttWindow *ibar_new(struct MuttWindow *parent, struct IndexSharedData *shared,
                            struct IndexPrivateData *priv)
{
  struct MuttWindow *win_ibar = mutt_window_new(WT_STATUS_BAR, MUTT_WIN_ORIENT_VERTICAL,
                                                MUTT_WIN_SIZE_FIXED,
                                                MUTT_WIN_SIZE_UNLIMITED, 1);

  win_ibar->wdata = ibar_data_new(shared, priv);
  win_ibar->wdata_free = ibar_data_free;
  win_ibar->recalc = ibar_recalc;
  win_ibar->repaint = ibar_repaint;

  mutt_color_observer_add(ibar_color_observer, win_ibar);
  notify_observer_add(NeoMutt->sub->notify, NT_CONFIG, ibar_config_observer, win_ibar);
  notify_observer_add(shared->notify, NT_ALL, ibar_index_observer, win_ibar);
  notify_observer_add(parent->notify, NT_MENU, ibar_menu_observer, win_ibar);
  notify_observer_add(win_ibar->notify, NT_WINDOW, ibar_window_observer, win_ibar);

  return win_ibar;
}
