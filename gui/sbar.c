/**
 * @file
 * Simple Bar (status)
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
 * @page gui_sbar Simple Bar (status)
 *
 * Simple Bar (status)
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "lib.h"

/**
 * struct SBarPrivateData - Private data for the Simple Bar
 */
struct SBarPrivateData
{
  char *display; ///< Cached display string
};

/**
 * sbar_recalc - Recalculate the Window data - Implements MuttWindow::recalc()
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
 * sbar_repaint - Repaint the Window - Implements MuttWindow::repaint()
 */
static int sbar_repaint(struct MuttWindow *win)
{
  if (!mutt_window_is_visible(win))
    return 0;

  struct SBarPrivateData *priv = win->wdata;

  mutt_window_move(win, 0, 0);

  mutt_curses_set_color(MT_COLOR_STATUS);
  mutt_window_move(win, 0, 0);
  mutt_paddstr(win, win->state.cols, priv->display);
  mutt_curses_set_color(MT_COLOR_NORMAL);

  mutt_debug(LL_DEBUG5, "repaint done\n");
  return 0;
}

/**
 * sbar_color_observer - Notification that a Color has changed - Implements ::observer_t
 */
static int sbar_color_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_COLOR) || !nc->global_data)
    return -1;

  struct EventColor *ev_c = nc->event_data;

  // MT_COLOR_MAX is sent on `uncolor *`
  if ((ev_c->color != MT_COLOR_STATUS) && (ev_c->color != MT_COLOR_MAX))
    return 0;

  struct MuttWindow *win_sbar = nc->global_data;

  win_sbar->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "color done, request WA_REPAINT\n");

  return 0;
}

/**
 * sbar_window_observer - Notification that a Window has changed - Implements ::observer_t
 */
static int sbar_window_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_WINDOW) || !nc->global_data || !nc->event_data)
    return -1;

  struct MuttWindow *win_sbar = nc->global_data;
  if (nc->event_subtype == NT_WINDOW_STATE)
  {
    win_sbar->actions |= WA_REPAINT;
    mutt_debug(LL_DEBUG5, "window state done, request WA_REPAINT\n");
  }
  else if (nc->event_subtype == NT_WINDOW_DELETE)
  {
    notify_observer_remove(NeoMutt->notify, sbar_color_observer, win_sbar);
    notify_observer_remove(win_sbar->notify, sbar_window_observer, win_sbar);
    mutt_debug(LL_DEBUG5, "window delete done\n");
  }

  return 0;
}

/**
 * sbar_wdata_free - Free the private data attached to the MuttWindow - Implements MuttWindow::wdata_free()
 */
static void sbar_wdata_free(struct MuttWindow *win, void **ptr)
{
  struct SBarPrivateData *priv = *ptr;

  FREE(&priv->display);

  FREE(ptr);
}

/**
 * sbar_data_new - Free the private data attached to the MuttWindow
 */
static struct SBarPrivateData *sbar_data_new(void)
{
  struct SBarPrivateData *sbar_data = mutt_mem_calloc(1, sizeof(struct SBarPrivateData));

  return sbar_data;
}

/**
 * sbar_new - Add the Simple Bar (status)
 * @param parent Parent Window
 * @retval ptr New Simple Bar
 */
struct MuttWindow *sbar_new(struct MuttWindow *parent)
{
  struct MuttWindow *win_sbar =
      mutt_window_new(WT_STATUS_BAR, MUTT_WIN_ORIENT_VERTICAL,
                      MUTT_WIN_SIZE_FIXED, MUTT_WIN_SIZE_UNLIMITED, 1);

  win_sbar->wdata = sbar_data_new();
  win_sbar->wdata_free = sbar_wdata_free;
  win_sbar->recalc = sbar_recalc;
  win_sbar->repaint = sbar_repaint;

  notify_observer_add(NeoMutt->notify, NT_COLOR, sbar_color_observer, win_sbar);
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
