/**
 * @file
 * Pager Window
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
 * @page pager_pager Pager Window
 *
 * The Pager Window displays an email to the user.
 *
 * ## Windows
 *
 * | Name         | Type      | See Also           |
 * | :----------- | :-------- | :----------------- |
 * | Pager Window | WT_CUSTOM | pager_window_new() |
 *
 * **Parent**
 * - @ref pager_ppanel
 *
 * **Children**
 *
 * None.
 *
 * ## Data
 * - #PagerPrivateData
 *
 * The Pager Window stores its data (#PagerPrivateData) in MuttWindow::wdata.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type            | Handler                 |
 * | :-------------------- | :---------------------- |
 * | #NT_COLOR             | pager_color_observer()  |
 * | #NT_CONFIG            | pager_config_observer() |
 * | #NT_INDEX             | pager_index_observer()  |
 * | #NT_PAGER             | pager_pager_observer()  |
 * | #NT_WINDOW            | pager_window_observer() |
 * | MuttWindow::recalc()  | pager_recalc()          |
 * | MuttWindow::repaint() | pager_repaint()         |
 */

#include "config.h"
#include <inttypes.h> // IWYU pragma: keep
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "index/lib.h"
#include "menu/lib.h"
#include "display.h"
#include "private_data.h"

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
  struct MuttWindow *panel_index = window_find_child(dlg, WT_INDEX);
  struct MuttWindow *win_index = window_find_child(panel_index, WT_MENU);
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
    panel_index->state.visible = true;
  }
  else
  {
    win_index->req_rows = MUTT_WIN_SIZE_UNLIMITED;
    win_index->size = MUTT_WIN_SIZE_MAXIMISE;

    panel_index->size = MUTT_WIN_SIZE_MAXIMISE;
    panel_index->state.visible = false;
  }

  mutt_window_reflow(dlg);
  mutt_debug(LL_DEBUG5, "config, request WA_REFLOW\n");
  return 0;
}

/**
 * pager_recalc - Recalculate the Pager display - Implements MuttWindow::recalc() - @ingroup window_recalc
 */
static int pager_recalc(struct MuttWindow *win)
{
  if (!mutt_window_is_visible(win))
    return 0;

  win->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "recalc done, request WA_REPAINT\n");
  return 0;
}

/**
 * pager_repaint - Repaint the Pager display - Implements MuttWindow::repaint() - @ingroup window_repaint
 */
static int pager_repaint(struct MuttWindow *win)
{
  if (!mutt_window_is_visible(win))
    return 0;

  mutt_debug(LL_DEBUG5, "repaint done\n");
  return 0;
}

/**
 * pager_color_observer - Notification that a Color has changed - Implements ::observer_t - @ingroup observer_api
 */
static int pager_color_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_COLOR) || !nc->global_data || !nc->event_data)
    return -1;

  struct EventColor *ev_c = nc->event_data;
  struct MuttWindow *win_pager = nc->global_data;
  struct PagerPrivateData *priv = win_pager->wdata;
  if (!priv)
    return 0;

  // MT_COLOR_MAX is sent on `uncolor *`
  if ((ev_c->cid == MT_COLOR_QUOTED) || (ev_c->cid == MT_COLOR_MAX))
  {
    // rework quoted colours
    qstyle_recolour(priv->quote_list);
  }

  if (ev_c->cid == MT_COLOR_MAX)
  {
    for (size_t i = 0; i < priv->lines_max; i++)
    {
      FREE(&(priv->lines[i].syntax));
      // if (priv->search_compiled && priv->lines[i].search)
      //   FREE(&(priv->lines[i].search));
      // priv->lines[i].syntax_arr_size = 0;
    }
    priv->lines_used = 0;

    // if (priv->search_compiled)
    // {
    //   regfree(&priv->search_re);
    //   priv->search_compiled = false;
    // }
  }

  mutt_debug(LL_DEBUG5, "color done\n");
  return 0;
}

/**
 * pager_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 */
static int pager_config_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_CONFIG) || !nc->global_data || !nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;
  struct MuttWindow *win_pager = nc->global_data;

  if (mutt_str_equal(ev_c->name, "pager_index_lines"))
  {
    config_pager_index_lines(win_pager);
    mutt_debug(LL_DEBUG5, "config done\n");
  }

  return 0;
}

/**
 * pager_index_observer - Notification that the Index has changed - Implements ::observer_t - @ingroup observer_api
 */
static int pager_index_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_INDEX) || !nc->global_data)
    return -1;

  struct MuttWindow *win_pager = nc->global_data;
  if (!win_pager)
    return 0;

  struct IndexSharedData *shared = nc->event_data;
  if (!shared)
    return 0;

  struct PagerPrivateData *priv = win_pager->wdata;
  if (!priv)
    return 0;

  if (nc->event_subtype & NT_INDEX_MAILBOX)
  {
    menu_queue_redraw(priv->menu, MENU_REDRAW_FULL);
    win_pager->actions |= WA_RECALC;
    mutt_debug(LL_DEBUG5, "index done, request WA_RECALC\n");
  }

  if (nc->event_subtype & NT_INDEX_EMAIL)
  {
    menu_queue_redraw(priv->menu, MENU_REDRAW_FULL);
    win_pager->actions |= WA_RECALC;
    mutt_debug(LL_DEBUG5, "index done, request WA_RECALC\n");
  }

  return 0;
}

/**
 * pager_pager_observer - Notification that the Pager has changed - Implements ::observer_t - @ingroup observer_api
 */
static int pager_pager_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_PAGER) || !nc->global_data || !nc->event_data)
    return -1;

  mutt_debug(LL_DEBUG5, "pager done\n");
  return 0;
}

/**
 * pager_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 */
static int pager_window_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_WINDOW) || !nc->global_data || !nc->event_data)
    return -1;

  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct MuttWindow *win_pager = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win_pager)
    return 0;

  struct MuttWindow *dlg = window_find_parent(win_pager, WT_DLG_INDEX);
  if (!dlg)
    dlg = window_find_parent(win_pager, WT_DLG_DO_PAGER);

  struct IndexSharedData *shared = dlg->wdata;

  notify_observer_remove(NeoMutt->notify, pager_color_observer, win_pager);
  notify_observer_remove(NeoMutt->notify, pager_config_observer, win_pager);
  notify_observer_remove(shared->notify, pager_index_observer, win_pager);
  notify_observer_remove(shared->notify, pager_pager_observer, win_pager);
  notify_observer_remove(win_pager->notify, pager_window_observer, win_pager);

  mutt_debug(LL_DEBUG5, "window delete done\n");

  return 0;
}

/**
 * pager_window_new - Create a new Pager Window (list of Emails)
 * @param shared Shared Index Data
 * @param priv   Private Pager Data
 * @retval ptr New Window
 */
struct MuttWindow *pager_window_new(struct IndexSharedData *shared,
                                    struct PagerPrivateData *priv)
{
  struct MuttWindow *win =
      mutt_window_new(WT_CUSTOM, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  win->wdata = priv;
  win->recalc = pager_recalc;
  win->repaint = pager_repaint;

  notify_observer_add(NeoMutt->notify, NT_COLOR, pager_color_observer, win);
  notify_observer_add(NeoMutt->notify, NT_CONFIG, pager_config_observer, win);
  notify_observer_add(shared->notify, NT_INDEX, pager_index_observer, win);
  notify_observer_add(shared->notify, NT_PAGER, pager_pager_observer, win);
  notify_observer_add(win->notify, NT_WINDOW, pager_window_observer, win);

  return win;
}
