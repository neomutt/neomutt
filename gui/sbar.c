/**
 * @file
 * Simple Bar (status)
 *
 * @authors
 * Copyright (C) 2021-2023 Richard Russon <rich@flatcap.org>
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
 * @page gui_sbar Simple Bar (status)
 *
 * The Simple Bar is a simple non-interactive window to display a message or
 * trivial status information.
 *
 * ## Windows
 *
 * | Name       | Type           | Constructor |
 * | :--------- | :------------- | :---------- |
 * | Simple Bar | #WT_STATUS_BAR | sbar_new()  |
 *
 * **Parent**
 *
 * The Simple Bar has many possible parents, e.g.
 *
 * - @ref compose_dlg_compose
 * - @ref gui_simple
 * - ...
 *
 * **Children**
 * - None
 *
 * ## Data
 * - #SBarPrivateData
 *
 * The Simple Bar caches the formatted display string.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type            | Handler                 |
 * | :-------------------- | :---------------------- |
 * | #NT_COLOR             | sbar_color_observer()   |
 * | #NT_WINDOW            | sbar_window_observer()  |
 * | MuttWindow::recalc()  | sbar_recalc()           |
 * | MuttWindow::repaint() | sbar_repaint()          |
 */

#include "config.h"
#include "mutt/lib.h"
#include "sbar.h"
#include "color/lib.h"
#include "curs_lib.h"
#include "mutt_curses.h"
#include "mutt_window.h"

/**
 * struct SBarPrivateData - Private data for the Simple Bar
 */
struct SBarPrivateData
{
  char *display; ///< Cached display string
};

/**
 * sbar_recalc - Recalculate the Window data - Implements MuttWindow::recalc() - @ingroup window_recalc
 */
static int sbar_recalc(struct MuttWindow *win)
{
  if (!win)
    return -1;

  win->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "recalc done, request WA_REPAINT\n");
  return 0;
}

/**
 * sbar_repaint - Repaint the Window - Implements MuttWindow::repaint() - @ingroup window_repaint
 */
static int sbar_repaint(struct MuttWindow *win)
{
  struct SBarPrivateData *priv = win->wdata;

  mutt_window_move(win, 0, 0);

  mutt_curses_set_normal_backed_color_by_id(MT_COLOR_STATUS);
  mutt_window_move(win, 0, 0);
  mutt_paddstr(win, win->state.cols, priv->display);
  mutt_curses_set_color_by_id(MT_COLOR_NORMAL);

  mutt_debug(LL_DEBUG5, "repaint done\n");
  return 0;
}

/**
 * sbar_color_observer - Notification that a Color has changed - Implements ::observer_t - @ingroup observer_api
 *
 * This function is triggered by changes to the colour settings, from the
 * `color` or `uncolor`, `mono` or `unmono` commands.
 */
static int sbar_color_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_COLOR)
    return 0;
  if (!nc->global_data)
    return -1;

  struct EventColor *ev_c = nc->event_data;

  // MT_COLOR_MAX is sent on `uncolor *`
  if ((ev_c->cid != MT_COLOR_STATUS) && (ev_c->cid != MT_COLOR_NORMAL) &&
      (ev_c->cid != MT_COLOR_MAX))
  {
    return 0;
  }

  struct MuttWindow *win_sbar = nc->global_data;

  win_sbar->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "color done, request WA_REPAINT\n");

  return 0;
}

/**
 * sbar_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 *
 * This function is triggered by changes to the windows.
 *
 * - State (this window): refresh the window
 * - Delete (this window): clean up the resources held by the Simple Bar
 */
static int sbar_window_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_WINDOW)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct MuttWindow *win_sbar = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win_sbar)
    return 0;

  if (nc->event_subtype == NT_WINDOW_STATE)
  {
    win_sbar->actions |= WA_REPAINT;
    mutt_debug(LL_DEBUG5, "window state done, request WA_REPAINT\n");
  }
  else if (nc->event_subtype == NT_WINDOW_DELETE)
  {
    mutt_color_observer_remove(sbar_color_observer, win_sbar);
    notify_observer_remove(win_sbar->notify, sbar_window_observer, win_sbar);
    mutt_debug(LL_DEBUG5, "window delete done\n");
  }

  return 0;
}

/**
 * sbar_wdata_free - Free the private data of the Simple Bar - Implements MuttWindow::wdata_free() - @ingroup window_wdata_free
 */
static void sbar_wdata_free(struct MuttWindow *win, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct SBarPrivateData *priv = *ptr;

  FREE(&priv->display);

  FREE(ptr);
}

/**
 * sbar_data_new - Create the private data for the Simple Bar
 * @retval ptr New SBar
 */
static struct SBarPrivateData *sbar_data_new(void)
{
  return MUTT_MEM_CALLOC(1, struct SBarPrivateData);
}

/**
 * sbar_new - Add the Simple Bar (status)
 * @retval ptr New Simple Bar
 */
struct MuttWindow *sbar_new(void)
{
  struct MuttWindow *win_sbar = mutt_window_new(WT_STATUS_BAR, MUTT_WIN_ORIENT_VERTICAL,
                                                MUTT_WIN_SIZE_FIXED,
                                                MUTT_WIN_SIZE_UNLIMITED, 1);

  win_sbar->wdata = sbar_data_new();
  win_sbar->wdata_free = sbar_wdata_free;
  win_sbar->recalc = sbar_recalc;
  win_sbar->repaint = sbar_repaint;

  mutt_color_observer_add(sbar_color_observer, win_sbar);
  notify_observer_add(win_sbar->notify, NT_WINDOW, sbar_window_observer, win_sbar);

  return win_sbar;
}

/**
 * sbar_set_title - Set the title for the Simple Bar
 * @param win   Window of the Simple Bar
 * @param title String to set
 *
 * @note The title string will be copied
 */
void sbar_set_title(struct MuttWindow *win, const char *title)
{
  if (!win || !win->wdata || (win->type != WT_STATUS_BAR))
    return;

  struct SBarPrivateData *priv = win->wdata;
  mutt_str_replace(&priv->display, title);

  win->actions |= WA_REPAINT;
}
