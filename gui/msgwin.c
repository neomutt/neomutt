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
 * The Message Window is a one-line interactive window at the bottom of the
 * screen.  It's used for asking the user questions, displaying messages and
 * for a progress bar.
 *
 * ## Behaviour
 *
 * The Message Window has two modes of behaviour: passive, active.
 *
 * ### Passive
 *
 * Most of the time, the Message Window will be passively displaying messages
 * to the user (or empty).  This is characterised by the Window focus being
 * somewhere else.  In this mode, the Message Window is responsible for drawing
 * itself.
 *
 * @sa mutt_message(), mutt_error()
 *
 * ### Active
 *
 * The Message Window can be hijacked by other code to be used for user
 * interaction, commonly for simple questions, "Are you sure? [Y/n]".
 * In this active state the Window will have focus and it's the responsibility
 * of the hijacker to perform the drawing.
 *
 * @sa mutt_yesorno(), @ref progress_progress
 *
 * ## Windows
 *
 * | Name           | Type        | Constructor  |
 * | :------------- | :---------- | :----------- |
 * | Message Window | #WT_MESSAGE | msgwin_new() |
 *
 * **Parent**
 * - @ref gui_rootwin
 *
 * **Children**
 * - None
 *
 * ## Data
 * - #MsgWinPrivateData
 *
 * The Message Window caches the formatted string.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type            | Handler                   |
 * | :-------------------- | :------------------------ |
 * | #NT_WINDOW            | msgwin_window_observer()  |
 * | MuttWindow::recalc()  | msgwin_recalc()           |
 * | MuttWindow::repaint() | msgwin_repaint()          |
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "color/lib.h"
#include "mutt_curses.h"
#include "mutt_window.h"

/// Message Window for messages, warnings, errors etc
static struct MuttWindow *MessageWindow = NULL;

/**
 * struct MsgWinPrivateData - Private data for the Message Window
 */
struct MsgWinPrivateData
{
  enum ColorId cid; ///< Colour for the text, e.g. #MT_COLOR_MESSAGE
  char *text;       ///< Cached display string
};

/**
 * msgwin_recalc - Recalculate the display of the Message Window - Implements MuttWindow::recalc() - @ingroup window_recalc
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
 * msgwin_repaint - Redraw the Message Window - Implements MuttWindow::repaint() - @ingroup window_repaint
 */
static int msgwin_repaint(struct MuttWindow *win)
{
  if (window_is_focused(win)) // someone else is using it
    return 0;

  struct MsgWinPrivateData *priv = win->wdata;

  mutt_window_move(win, 0, 0);

  mutt_curses_set_normal_backed_color_by_id(priv->cid);
  mutt_window_move(win, 0, 0);
  mutt_window_addstr(win, priv->text);
  mutt_curses_set_color_by_id(MT_COLOR_NORMAL);
  mutt_window_clrtoeol(win);

  mutt_debug(LL_DEBUG5, "repaint done\n");
  return 0;
}

/**
 * msgwin_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 *
 * This function is triggered by changes to the windows.
 *
 * - State (this window): refresh the window
 * - Delete (this window): clean up the resources held by the Message Window
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
 * msgwin_wdata_free - Free the private data attached to the Message Window - Implements MuttWindow::wdata_free() - @ingroup window_wdata_free
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
  struct MsgWinPrivateData *msgwin_data = mutt_mem_calloc(1, sizeof(struct MsgWinPrivateData));

  msgwin_data->cid = MT_COLOR_NORMAL;

  return msgwin_data;
}

/**
 * msgwin_new - Create the Message Window
 * @retval ptr New Window
 */
struct MuttWindow *msgwin_new(void)
{
  MessageWindow = mutt_window_new(WT_MESSAGE, MUTT_WIN_ORIENT_VERTICAL,
                                  MUTT_WIN_SIZE_FIXED, MUTT_WIN_SIZE_UNLIMITED, 1);
  MessageWindow->wdata = msgwin_wdata_new();
  MessageWindow->wdata_free = msgwin_wdata_free;
  MessageWindow->recalc = msgwin_recalc;
  MessageWindow->repaint = msgwin_repaint;

  notify_observer_add(MessageWindow->notify, NT_WINDOW, msgwin_window_observer, MessageWindow);

  return MessageWindow;
}

/**
 * msgwin_get_text - Get the text from the Message Window
 *
 * @note Do not free the returned string
 */
const char *msgwin_get_text(void)
{
  if (!MessageWindow)
    return NULL;

  struct MsgWinPrivateData *priv = MessageWindow->wdata;

  return priv->text;
}

/**
 * msgwin_set_text - Set the text for the Message Window
 * @param cid  Colour Id, e.g. #MT_COLOR_MESSAGE
 * @param text Text to set
 *
 * @note The text string will be copied
 */
void msgwin_set_text(enum ColorId cid, const char *text)
{
  if (!MessageWindow)
    return;

  struct MsgWinPrivateData *priv = MessageWindow->wdata;

  priv->cid = cid;
  mutt_str_replace(&priv->text, text);

  MessageWindow->actions |= WA_RECALC;
}

/**
 * msgwin_clear_text - Clear the text in the Message Window
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

  struct MuttWindow *win_cont = MessageWindow->parent;

  win_cont->req_rows = height;
  mutt_window_reflow(win_cont->parent);
}
