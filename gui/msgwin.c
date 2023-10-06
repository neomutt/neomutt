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
 * screen.  See @ref gui_mw
 *
 * @defgroup gui_mw GUI: Message Windows
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
 * @sa query_yesorno(), @ref progress_progress
 *
 * ## Windows
 *
 * | Name           | Type        | Constructor  |
 * | :------------- | :---------- | :----------- |
 * | Message Window | #WT_MESSAGE | msgwin_new() |
 *
 * **Parent**
 * - @ref gui_msgcont
 *
 * **Children**
 * - None
 *
 * ## Data
 * - #MsgWinWindowData
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
#include <stdbool.h>
#include <string.h>
#include <wchar.h>
#include "mutt/lib.h"
#include "msgwin.h"
#include "color/lib.h"
#include "msgcont.h"
#include "msgwin_wdata.h"
#include "mutt_curses.h"
#include "mutt_window.h"

/**
 * measure - Measure a string in bytes and cells
 * @param chars    Results
 * @param str      String to measure
 * @param ac_color Colour to associate
 *
 * Calculate the size of each character in a string in bytes and screen cells.
 */
void measure(struct MwCharArray *chars, const char *str, const struct AttrColor *ac_color)
{
  if (!str || !*str)
    return;

  mbstate_t mbstate = { 0 };
  struct MwChar mwc = { 0 };

  size_t str_len = mutt_str_len(str);

  while (*str && (str_len > 0))
  {
    wchar_t wc = L'\0';
    size_t consumed = mbrtowc(&wc, str, str_len, &mbstate);
    if (consumed == 0)
      break;

    if (consumed == ICONV_ILLEGAL_SEQ)
    {
      memset(&mbstate, 0, sizeof(mbstate));
      wc = ReplacementChar;
      consumed = 1;
    }
    else if (consumed == ICONV_BUF_TOO_SMALL)
    {
      wc = ReplacementChar;
      consumed = str_len;
    }

    int wchar_width = wcwidth(wc);
    if (wchar_width < 0)
      wchar_width = 1;

    if (wc == 0xfe0f) // Emoji variation
    {
      size_t size = ARRAY_SIZE(chars);
      if (size > 0)
      {
        struct MwChar *es_prev = ARRAY_GET(chars, size - 1);
        if (es_prev->width == 1)
          es_prev->width = 2;
      }
    }

    mwc = (struct MwChar){ wchar_width, consumed, ac_color };
    ARRAY_ADD(chars, mwc);

    str += consumed;
    str_len -= consumed;
  }
}

/**
 * msgwin_recalc - Recalculate the display of the Message Window - Implements MuttWindow::recalc() - @ingroup window_recalc
 */
static int msgwin_recalc(struct MuttWindow *win)
{
  win->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "recalc done, request WA_REPAINT\n");
  return 0;
}

/**
 * msgwin_calc_rows - How many rows will a string need?
 * @param wdata Window data
 * @param cols  Columns to wrap at
 * @param str   Text to measure
 * @retval num Number of rows required
 * @retval   0 Error
 *
 * Divide the width of a string by the width of the Message Window.
 */
int msgwin_calc_rows(struct MsgWinWindowData *wdata, int cols, const char *str)
{
  if (!wdata || !str || !*str)
    return 0;

  for (int i = 0; i < MSGWIN_MAX_ROWS; i++)
  {
    ARRAY_FREE(&wdata->rows[i]);
  }

  int width = 0;  ///< Screen width used
  int offset = 0; ///< Offset into str
  int row = 0;    ///< Current row
  bool new_row = false;

  struct MwChunk *chunk = NULL;
  struct MwChar *mwc = NULL;
  ARRAY_FOREACH(mwc, &wdata->chars)
  {
    const bool nl = (mwc->bytes == 1) && (str[offset] == '\n');
    if (nl)
    {
      new_row = true;
      offset += mwc->bytes;
      continue;
    }

    if (((width + mwc->width) > cols) || new_row)
    {
      // ROW IS FULL
      new_row = false;

      row++;
      if (row >= MSGWIN_MAX_ROWS)
      {
        // NO MORE ROOM
        break;
      }

      // Start a new row
      struct MwChunk tmp = { offset, mwc->bytes, mwc->width, mwc->ac_color };

      mutt_debug(LL_DEBUG5, "row = %d\n", row);
      ARRAY_ADD(&wdata->rows[row], tmp);
      chunk = ARRAY_LAST(&wdata->rows[row]);

      width = 0;
    }
    else if (!chunk || (mwc->ac_color != chunk->ac_color))
    {
      // CHANGE OF COLOUR
      struct MwChunk tmp = { offset, mwc->bytes, mwc->width, mwc->ac_color };
      ARRAY_ADD(&wdata->rows[row], tmp);
      chunk = ARRAY_LAST(&wdata->rows[row]);
    }
    else
    {
      // MORE OF THE SAME
      chunk->bytes += mwc->bytes;
      chunk->width += mwc->width;
    }

    offset += mwc->bytes;
    width += mwc->width;
  }

  mutt_debug(LL_DEBUG5, "msgwin_calc_rows() => %d\n", row + 1);
  return row + 1;
}

/**
 * msgwin_repaint - Redraw the Message Window - Implements MuttWindow::repaint() - @ingroup window_repaint
 */
static int msgwin_repaint(struct MuttWindow *win)
{
  struct MsgWinWindowData *wdata = win->wdata;

  const char *str = buf_string(wdata->text);
  for (int i = 0; i < MSGWIN_MAX_ROWS; i++)
  {
    mutt_window_move(win, 0, i);
    if (ARRAY_EMPTY(&wdata->rows[i]))
      break;

    struct MwChunk *chunk = NULL;
    ARRAY_FOREACH(chunk, &wdata->rows[i])
    {
      mutt_curses_set_color(chunk->ac_color);
      mutt_window_addnstr(win, str + chunk->offset, chunk->bytes);
    }
    mutt_curses_set_color_by_id(MT_COLOR_NORMAL);
    mutt_window_clrtoeol(win);
  }
  mutt_curses_set_color_by_id(MT_COLOR_NORMAL);
  mutt_window_clrtoeol(win);

  mutt_window_get_coords(win, &wdata->col, &wdata->row);

  mutt_debug(LL_DEBUG5, "msgwin repaint done\n");
  return 0;
}

/**
 * msgwin_recursor - Recursor the Message Window - Implements MuttWindow::recursor() - @ingroup window_recursor
 */
static bool msgwin_recursor(struct MuttWindow *win)
{
  struct MsgWinWindowData *wdata = win->wdata;

  mutt_window_move(win, wdata->col, wdata->row);
  mutt_curses_set_cursor(MUTT_CURSOR_VISIBLE);

  mutt_debug(LL_DEBUG5, "msgwin recursor done\n");
  return true;
}

/**
 * msgwin_set_rows - Resize the Message Window
 * @param win  Message Window
 * @param rows Number of rows required
 *
 * Resize the other Windows to allow a multi-line message to be displayed.
 *
 * @note rows will be clamped between 1 and 3 (#MSGWIN_MAX_ROWS) inclusive
 */
void msgwin_set_rows(struct MuttWindow *win, short rows)
{
  if (!win)
    win = msgcont_get_msgwin();
  if (!win)
    return;

  rows = CLAMP(rows, 1, MSGWIN_MAX_ROWS);

  if (rows != win->state.rows)
  {
    win->req_rows = rows;
    mutt_window_reflow(NULL);
  }
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
  if (nc->event_type != NT_WINDOW)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct MuttWindow *win = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win)
    return 0;

  if (nc->event_subtype == NT_WINDOW_STATE)
  {
    if (ev_w->flags & WN_HIDDEN)
    {
      msgwin_clear_text(win);
    }

    if (ev_w->flags & (WN_NARROWER | WN_WIDER))
    {
      struct MsgWinWindowData *wdata = win->wdata;
      msgwin_set_rows(win, msgwin_calc_rows(wdata, win->state.cols,
                                            buf_string(wdata->text)));
      win->actions |= WA_RECALC;
    }
    else
    {
      win->actions |= WA_REPAINT;
    }
    mutt_debug(LL_DEBUG5, "window state done, request WA_RECALC\n");
  }
  else if (nc->event_subtype == NT_WINDOW_DELETE)
  {
    notify_observer_remove(win->notify, msgwin_window_observer, win);
    mutt_debug(LL_DEBUG5, "window delete done\n");
  }
  return 0;
}

/**
 * msgwin_new - Create the Message Window
 * @retval ptr New Window
 */
struct MuttWindow *msgwin_new(bool interactive)
{
  struct MuttWindow *win = mutt_window_new(WT_MESSAGE, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED,
                                           MUTT_WIN_SIZE_UNLIMITED, 1);

  struct MsgWinWindowData *wdata = msgwin_wdata_new();

  win->wdata = wdata;
  win->wdata_free = msgwin_wdata_free;
  win->recalc = msgwin_recalc;
  win->repaint = msgwin_repaint;

  if (interactive)
    win->recursor = msgwin_recursor;

  // Copy the container's dimensions
  win->state = MessageContainer->state;

  notify_observer_add(win->notify, NT_WINDOW, msgwin_window_observer, win);

  return win;
}

/**
 * msgwin_get_text - Get the text from the Message Window
 * @param win Message Window
 * @retval ptr Window text
 *
 * @note Do not free the returned string
 */
const char *msgwin_get_text(struct MuttWindow *win)
{
  if (!win)
    win = msgcont_get_msgwin();
  if (!win)
    return NULL;

  struct MsgWinWindowData *wdata = win->wdata;

  return buf_string(wdata->text);
}

/**
 * msgwin_add_text - Add text to the Message Window
 * @param win      Message Window
 * @param text     Text to add
 * @param ac_color Colour for text
 */
void msgwin_add_text(struct MuttWindow *win, const char *text, const struct AttrColor *ac_color)
{
  if (!win)
    win = msgcont_get_msgwin();
  if (!win)
    return;

  struct MsgWinWindowData *wdata = win->wdata;

  if (text)
  {
    buf_addstr(wdata->text, text);
    measure(&wdata->chars, text, ac_color);
    mutt_debug(LL_DEBUG5, "MW ADD: %zu, %s\n", buf_len(wdata->text),
               buf_string(wdata->text));
  }
  else
  {
    int rows = msgwin_calc_rows(wdata, win->state.cols, buf_string(wdata->text));
    msgwin_set_rows(win, rows);
    win->actions |= WA_RECALC;
  }
}

/**
 * msgwin_add_text_n - Add some text to the Message Window
 * @param win      Message Window
 * @param text     Text to add
 * @param bytes    Number of bytes of text to add
 * @param ac_color Colour for text
 */
void msgwin_add_text_n(struct MuttWindow *win, const char *text, int bytes,
                       const struct AttrColor *ac_color)
{
  if (!win)
    win = msgcont_get_msgwin();
  if (!win)
    return;

  struct MsgWinWindowData *wdata = win->wdata;

  if (text)
  {
    const char *dptr = wdata->text->dptr;
    buf_addstr_n(wdata->text, text, bytes);
    measure(&wdata->chars, dptr, ac_color);
    mutt_debug(LL_DEBUG5, "MW ADD: %zu, %s\n", buf_len(wdata->text),
               buf_string(wdata->text));
  }
  else
  {
    int rows = msgwin_calc_rows(wdata, win->state.cols, buf_string(wdata->text));
    msgwin_set_rows(win, rows);
    win->actions |= WA_RECALC;
  }
}

/**
 * msgwin_set_text - Set the text for the Message Window
 * @param win   Message Window
 * @param text  Text to set
 * @param color Colour for text
 *
 * @note The text string will be copied
 */
void msgwin_set_text(struct MuttWindow *win, const char *text, enum ColorId color)
{
  if (!win)
    win = msgcont_get_msgwin();
  if (!win)
    return;

  struct MsgWinWindowData *wdata = win->wdata;

  if (mutt_str_equal(buf_string(wdata->text), text))
    return;

  buf_strcpy(wdata->text, text);
  ARRAY_FREE(&wdata->chars);
  if (wdata->text)
  {
    const struct AttrColor *ac_color = simple_color_get(color);
    measure(&wdata->chars, buf_string(wdata->text), ac_color);
  }

  mutt_debug(LL_DEBUG5, "MW SET: %zu, %s\n", buf_len(wdata->text),
             buf_string(wdata->text));

  int rows = msgwin_calc_rows(wdata, win->state.cols, buf_string(wdata->text));
  msgwin_set_rows(win, rows);
  win->actions |= WA_RECALC;
}

/**
 * msgwin_clear_text - Clear the text in the Message Window
 * @param win Message Window
 */
void msgwin_clear_text(struct MuttWindow *win)
{
  msgwin_set_text(win, NULL, MT_COLOR_NORMAL);
}

/**
 * msgwin_get_window - Get the Message Window pointer
 * @retval ptr Message Window
 *
 * Allow some users direct access to the Message Window.
 */
struct MuttWindow *msgwin_get_window(void)
{
  return msgcont_get_msgwin();
}
