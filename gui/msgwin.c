/**
 * @file
 * Message Window
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
 * @page gui_msgwin Message Window
 *
 * Message Window for displaying messages and text entry.
 */

#include "config.h"
#include "mutt/lib.h"
#include "core/lib.h"
#include "color.h"
#include "curs_lib.h"
#include "mutt_curses.h"
#include "mutt_window.h"

/**
 * struct MsgWinPrivateData - Private data for the Message Window
 */
struct MsgWinPrivateData
{
  enum ColorId color; ///< Colour for the text, e.g. #MT_COLOR_MESSAGE
  char *text;         ///< Cached display string
};

/**
 * msgwin_recalc - Recalculate the display of the Message Window
 * @param win Message Window
 */
static int msgwin_recalc(struct MuttWindow *win)
{
  if (!win)
    return -1;

  struct MuttWindow *win_focus = window_get_focus();
  if (!win_focus)
    return 0;

  // If the MessageWindow is focussed, then someone else is actively using it.
  if (win_focus == win)
    return 0;

  // If nobody is using it, we'll take charge
  win->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "recalc done, request WA_REPAINT\n");
  return 0;
}

/**
 * msgwin_repaint - Redraw the Message Window
 * @param win Message Window
 */
static int msgwin_repaint(struct MuttWindow *win)
{
  if (!mutt_window_is_visible(win))
    return 0;

  struct MuttWindow *win_focus = window_get_focus();
  if (!win_focus)
    return 0;

  // If the MessageWindow is focussed, then someone else is actively using it.
  if (win_focus == win)
    return 0;

  struct MsgWinPrivateData *priv = win->wdata;

  mutt_window_move(win, 0, 0);

  mutt_curses_set_color(priv->color);
  mutt_window_move(win, 0, 0);
  mutt_paddstr(win, win->state.cols, priv->text);
  mutt_curses_set_color(MT_COLOR_NORMAL);

  mutt_debug(LL_DEBUG5, "repaint done\n");
  return 0;
}

/**
 * msgwin_window_observer - Window has changed - Implements ::observer_t
 */
static int msgwin_window_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_WINDOW) || !nc->event_data || !nc->global_data)
    return -1;

  struct MuttWindow *win_msg = nc->global_data;

  if (nc->event_subtype == NT_WINDOW_STATE)
  {
    win_msg->actions |= WA_RECALC;
    mutt_debug(LL_NOTIFY, "state change, request WA_RECALC\n");
  }
  else if (nc->event_subtype == NT_WINDOW_DELETE)
  {
    struct EventWindow *ev_w = nc->event_data;
    if (ev_w->win != win_msg)
      return 0;

    notify_observer_remove(NeoMutt->notify, msgwin_window_observer, win_msg);
    mutt_debug(LL_DEBUG5, "window delete done\n");
  }
  return 0;
}

/**
 * msgwin_wdata_free - Free the private data attached to the Message Window - Implements MuttWindow::wdata_free()
 */
static void msgwin_wdata_free(struct MuttWindow *win, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct MsgWinPrivateData *priv = *ptr;

  FREE(&priv->text);

  FREE(ptr);
}

/**
 * msgwin_wdata_new - Create new private data for the Message Window
 * @retval ptr New private data
 */
static struct MsgWinPrivateData *msgwin_wdata_new(void)
{
  struct MsgWinPrivateData *msgwin_data =
      mutt_mem_calloc(1, sizeof(struct MsgWinPrivateData));

  msgwin_data->color = MT_COLOR_NORMAL;

  return msgwin_data;
}

/**
 * msgwin_create - Create the Message Window
 * @retval ptr New Window
 */
struct MuttWindow *msgwin_create(void)
{
  struct MuttWindow *win =
      mutt_window_new(WT_MESSAGE, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED,
                      MUTT_WIN_SIZE_UNLIMITED, 1);

  win->wdata = msgwin_wdata_new();
  win->wdata_free = msgwin_wdata_free;
  win->recalc = msgwin_recalc;
  win->repaint = msgwin_repaint;

  notify_observer_add(win->notify, NT_WINDOW, msgwin_window_observer, win);
  return win;
}
