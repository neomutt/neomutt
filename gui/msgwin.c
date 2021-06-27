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
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "color.h"
#include "curs_lib.h"
#include "mutt_curses.h"
#include "mutt_window.h"

struct MuttWindow *MessageWindow = NULL; ///< Message Window, ":set", etc

/**
 * struct MsgWinPrivateData - Private data for the Message Window
 */
struct MsgWinPrivateData
{
  enum ColorId color; ///< Colour for the text, e.g. #MT_COLOR_MESSAGE
  char *text;         ///< Cached display string
};

/**
 * msgwin_recalc - Recalculate the display of the Message Window - Implements MuttWindow::recalc()
 */
static int msgwin_recalc(struct MuttWindow *win)
{
  if (window_is_focused(win)) // Someone else is using it
    return 0;

  win->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "recalc done, request WA_REPAINT\n");
  return 0;
}

/**
 * msgwin_repaint - Redraw the Message Window - Implements MuttWindow::repaint()
 */
static int msgwin_repaint(struct MuttWindow *win)
{
  if (!mutt_window_is_visible(win) || window_is_focused(win)) // or someone else is using it
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
 * msgwin_window_observer - Notification that a Window has changed - Implements ::observer_t
 */
static int msgwin_window_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_WINDOW) || !nc->global_data || !nc->event_data)
    return -1;

  struct MuttWindow *win_msg = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win_msg)
    return 0;

  if (nc->event_subtype == NT_WINDOW_STATE)
  {
    win_msg->actions |= WA_RECALC;
    mutt_debug(LL_NOTIFY, "window state done, request WA_RECALC\n");
  }
  else if (nc->event_subtype == NT_WINDOW_DELETE)
  {
    notify_observer_remove(win_msg->notify, msgwin_window_observer, win_msg);
    MessageWindow = NULL;
    mutt_debug(LL_DEBUG5, "window delete done\n");
  }
  return 0;
}

/**
 * msgwin_wdata_free - Free the private data attached to the Message Window - Implements MuttWindow::wdata_free()
 */
static void msgwin_wdata_free(struct MuttWindow *win, void **ptr)
{
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
 * msgwin_new - Create the Message Window
 * @retval ptr New Window
 */
struct MuttWindow *msgwin_new(void)
{
  struct MuttWindow *win =
      mutt_window_new(WT_MESSAGE, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED,
                      MUTT_WIN_SIZE_UNLIMITED, 1);

  win->wdata = msgwin_wdata_new();
  win->wdata_free = msgwin_wdata_free;
  win->recalc = msgwin_recalc;
  win->repaint = msgwin_repaint;

  notify_observer_add(win->notify, NT_WINDOW, msgwin_window_observer, win);

  MessageWindow = win;
  return win;
}

/**
 * msgwin_set_text - Set the text for the Message Window
 * @param color Colour, e.g. #MT_COLOR_MESSAGE
 * @param text  Text to set
 *
 * @note Changes will appear on screen immediately
 *
 * @note The text string will be copied
 */
void msgwin_set_text(enum ColorId color, const char *text)
{
  if (!MessageWindow)
    return;

  struct MsgWinPrivateData *priv = MessageWindow->wdata;

  priv->color = color;
  mutt_str_replace(&priv->text, text);

  MessageWindow->actions |= WA_RECALC;
  window_redraw(MessageWindow);
}

/**
 * msgwin_clear_text - Clear the text in the Message Window
 *
 * @note Changes will appear on screen immediately
 */
void msgwin_clear_text(void)
{
  msgwin_set_text(MT_COLOR_NORMAL, NULL);
}

/**
 * msgwin_get_window - Get the Message Window pointer
 * @retval ptr Message Window
 *
 * Allow some users direct access to the Message Window.
 */
struct MuttWindow *msgwin_get_window(void)
{
  return MessageWindow;
}

/**
 * msgwin_get_width - Get the width of the Message Window
 * @retval num Width of Message Window
 */
size_t msgwin_get_width(void)
{
  if (!MessageWindow)
    return 0;

  return MessageWindow->state.cols;
}

/**
 * msgwin_set_height - Resize the Message Window
 * @param height Number of rows required
 *
 * Resize the other Windows to allow a multi-line message to be displayed.
 */
void msgwin_set_height(short height)
{
  if (!MessageWindow)
    return;

  if (height < 1)
    height = 1;
  else if (height > 3)
    height = 3;

  MessageWindow->req_rows = height;
  mutt_window_reflow(MessageWindow->parent);
}
