/**
 * @file
 * Progress Bar Window
 *
 * @authors
 * Copyright (C) 2022-2023 Richard Russon <rich@flatcap.org>
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
 * @page progress_window Progress Bar Window
 *
 * Progress Bar Window
 *
 * ## Windows
 *
 * | Name            | Type          | See Also              |
 * | :-------------- | :------------ | :-------------------- |
 * | Progress Window | WT_STATUS_BAR | progress_window_new() |
 *
 * **Parent**
 * - @ref gui_msgcont
 *
 * **Children**
 *
 * None.
 *
 * ## Data
 * - #ProgressWindowData
 *
 * The Progress Bar Window stores its data (#ProgressWindowData) in MuttWindow::wdata.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type            | Handler               |
 * | :-------------------- | :-------------------- |
 * | MuttWindow::recalc()  | sb_recalc()           |
 * | MuttWindow::repaint() | sb_repaint()          |
 */

#include "config.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "color/lib.h"
#include "expando/lib.h"
#include "muttlib.h"
#include "wdata.h"

/**
 * message_bar - Draw a colourful progress bar
 * @param win     Window to draw on
 * @param percent %age complete
 * @param fmt     printf(1)-like formatting string
 * @param ...     Arguments to formatting string
 */
static void message_bar(struct MuttWindow *win, int percent, const char *fmt, ...)
{
  if (!fmt || !win || !win->wdata)
    return;

  va_list ap;
  char buf[1024] = { 0 };
  struct Buffer *buf2 = buf_pool_get();
  int w = (percent * win->state.cols) / 100;
  size_t l;

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  l = mutt_strwidth(buf);
  va_end(ap);

  format_string(buf2, 0, win->state.cols - 2, JUSTIFY_LEFT, 0, buf, sizeof(buf), false);

  mutt_window_move(win, 0, 0);

  if ((percent != -1) && simple_color_is_set(MT_COLOR_PROGRESS))
  {
    if (l < w)
    {
      /* The string fits within the colour bar */
      mutt_curses_set_normal_backed_color_by_id(MT_COLOR_PROGRESS);
      mutt_window_addstr(win, buf_string(buf2));
      w -= l;
      while (w-- > 0)
      {
        mutt_window_addch(win, ' ');
      }
      mutt_curses_set_color_by_id(MT_COLOR_NORMAL);
    }
    else
    {
      /* The string is too long for the colour bar */
      int off = mutt_wstr_trunc(buf_string(buf2), buf2->dsize, w, NULL);

      char ch = buf_at(buf2, off);
      buf2->data[off] = '\0';
      mutt_curses_set_normal_backed_color_by_id(MT_COLOR_PROGRESS);
      mutt_window_addstr(win, buf_string(buf2));
      buf2->data[off] = ch;
      mutt_curses_set_color_by_id(MT_COLOR_NORMAL);
      mutt_window_addstr(win, buf2->data + off);
    }
  }
  else
  {
    mutt_window_addstr(win, buf_string(buf2));
  }

  mutt_window_clrtoeol(win);
  buf_pool_release(&buf2);
}

/**
 * progress_window_recalc - Recalculate the Progress Bar - Implements MuttWindow::recalc() - @ingroup window_recalc
 */
static int progress_window_recalc(struct MuttWindow *win)
{
  if (!win || !win->wdata)
    return -1;

  struct ProgressWindowData *wdata = win->wdata;
  wdata->display_pos = wdata->update_pos;
  wdata->display_time = wdata->update_time;

  if (wdata->is_bytes)
    mutt_str_pretty_size(wdata->pretty_pos, sizeof(wdata->pretty_pos), wdata->display_pos);

  if ((wdata->update_percent < 0) && (wdata->size != 0))
    wdata->display_percent = 100 * wdata->display_pos / wdata->size;
  else
    wdata->display_percent = wdata->update_percent;

  win->actions |= WA_REPAINT;
  return 0;
}

/**
 * progress_window_repaint - Repaint the Progress Bar - Implements MuttWindow::repaint() - @ingroup window_repaint
 */
static int progress_window_repaint(struct MuttWindow *win)
{
  if (!win || !win->wdata)
    return -1;

  struct ProgressWindowData *wdata = win->wdata;
  if (wdata->msg[0] == '\0')
    return 0;

  if (wdata->size == 0)
  {
    if (wdata->display_percent >= 0)
    {
      if (wdata->is_bytes)
      {
        /* L10N: Progress bar: `%s` loading text, `%s` pretty size (e.g. 4.6K),
           `%d` is the number, `%%` is the percent symbol.
           `%d` and `%%` may be reordered, or space inserted, if you wish. */
        message_bar(wdata->win, wdata->display_percent, _("%s %s (%d%%)"),
                    wdata->msg, wdata->pretty_pos, wdata->display_percent);
      }
      else
      {
        /* L10N: Progress bar: `%s` loading text, `%zu` position,
           `%d` is the number, `%%` is the percent symbol.
           `%d` and `%%` may be reordered, or space inserted, if you wish. */
        message_bar(wdata->win, wdata->display_percent, _("%s %zu (%d%%)"),
                    wdata->msg, wdata->display_pos, wdata->display_percent);
      }
    }
    else
    {
      if (wdata->is_bytes)
      {
        /* L10N: Progress bar: `%s` loading text, `%s` position/size */
        message_bar(wdata->win, -1, _("%s %s"), wdata->msg, wdata->pretty_pos);
      }
      else
      {
        /* L10N: Progress bar: `%s` loading text, `%zu` position */
        message_bar(wdata->win, -1, _("%s %zu"), wdata->msg, wdata->display_pos);
      }
    }
  }
  else
  {
    if (wdata->is_bytes)
    {
      /* L10N: Progress bar: `%s` loading text, `%s/%s` position/size,
         `%d` is the number, `%%` is the percent symbol.
         `%d` and `%%` may be reordered, or space inserted, if you wish. */
      message_bar(wdata->win, wdata->display_percent, _("%s %s/%s (%d%%)"),
                  wdata->msg, wdata->pretty_pos, wdata->pretty_size, wdata->display_percent);
    }
    else
    {
      /* L10N: Progress bar: `%s` loading text, `%zu/%zu` position/size,
         `%d` is the number, `%%` is the percent symbol.
         `%d` and `%%` may be reordered, or space inserted, if you wish. */
      message_bar(wdata->win, wdata->display_percent, _("%s %zu/%zu (%d%%)"),
                  wdata->msg, wdata->display_pos, wdata->size, wdata->display_percent);
    }
  }

  return 0;
}

/**
 * percent_needs_update - Do we need to update, given the current percentage?
 * @param wdata   Private data
 * @param percent Updated percentage
 * @retval true Progress needs an update
 */
static bool percent_needs_update(const struct ProgressWindowData *wdata, int percent)
{
  return (percent > wdata->display_percent);
}

/**
 * pos_needs_update - Do we need to update, given the current pos?
 * @param wdata Private data
 * @param pos   Current pos
 * @retval true Progress needs an update
 */
static bool pos_needs_update(const struct ProgressWindowData *wdata, long pos)
{
  const unsigned shift = wdata->is_bytes ? 10 : 0;
  return pos >= (wdata->display_pos + (wdata->size_inc << shift));
}

/**
 * time_needs_update - Do we need to update, given the current time?
 * @param wdata Private data
 * @param now   Time
 * @retval true Progress needs an update
 */
static bool time_needs_update(const struct ProgressWindowData *wdata, size_t now)
{
  if (wdata->time_inc == 0)
    return true;

  if (now < wdata->display_time)
    return true;

  const size_t elapsed = (now - wdata->display_time);
  return (wdata->time_inc < elapsed);
}

/**
 * progress_window_update - Update the Progress Bar Window
 * @param win      Window to draw on
 * @param pos      Position, or count
 * @param percent  Percentage complete
 * @retval true Screen update is needed
 */
bool progress_window_update(struct MuttWindow *win, size_t pos, int percent)
{
  if (!win || !win->wdata)
    return false;

  struct ProgressWindowData *wdata = win->wdata;

  if (percent >= 0)
  {
    if (!percent_needs_update(wdata, percent))
      return false;
  }
  else
  {
    if (!pos_needs_update(wdata, pos))
      return false;
  }

  const uint64_t now = mutt_date_now_ms();
  if (!time_needs_update(wdata, now))
    return false;

  wdata->update_pos = pos;
  wdata->update_percent = percent;
  wdata->update_time = now;
  win->actions |= WA_RECALC;
  return true;
}

/**
 * progress_window_new - Create a new Progress Bar Window
 * @param size     Expected number of records or size of traffic
 * @param size_inc Size increment (step size)
 * @param time_inc Time increment
 * @param is_bytes true if measuring bytes
 * @retval ptr New Progress Window
 */
struct MuttWindow *progress_window_new(size_t size, size_t size_inc,
                                       size_t time_inc, bool is_bytes)
{
  if (size_inc == 0) // The user has disabled the progress bar
    return NULL;

  struct MuttWindow *win = mutt_window_new(WT_STATUS_BAR, MUTT_WIN_ORIENT_VERTICAL,
                                           MUTT_WIN_SIZE_FIXED,
                                           MUTT_WIN_SIZE_UNLIMITED, 1);
  win->recalc = progress_window_recalc;
  win->repaint = progress_window_repaint;
  win->actions |= WA_RECALC;

  struct ProgressWindowData *wdata = progress_wdata_new();
  wdata->win = win;
  wdata->size = size;
  wdata->size_inc = size_inc;
  wdata->time_inc = time_inc;
  wdata->is_bytes = is_bytes;

  if (is_bytes)
    mutt_str_pretty_size(wdata->pretty_size, sizeof(wdata->pretty_size), size);

  win->wdata = wdata;
  win->wdata_free = progress_wdata_free;

  return win;
}

/**
 * progress_window_set_message - Set the progress message
 * @param win Window to draw on
 * @param fmt printf format string
 * @param ap  printf arguments
 */
void progress_window_set_message(struct MuttWindow *win, const char *fmt, va_list ap)
{
  if (!win || !win->wdata || !fmt)
    return;

  struct ProgressWindowData *wdata = win->wdata;

  vsnprintf(wdata->msg, sizeof(wdata->msg), fmt, ap);

  win->actions |= WA_RECALC;
}

/**
 * progress_window_set_size - Set the progress size
 * @param win  Window to draw on
 * @param size New size
 */
void progress_window_set_size(struct MuttWindow *win, size_t size)
{
  if (!win || !win->wdata)
    return;

  struct ProgressWindowData *wdata = win->wdata;

  wdata->size = size;
  wdata->display_pos = 0;
  wdata->display_percent = 0;
  wdata->display_time = 0;
  win->actions |= WA_RECALC;
}
