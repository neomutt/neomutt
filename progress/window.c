/**
 * @file
 * Progress Bar Window
 *
 * @authors
 * Copyright (C) 2022 Richard Russon <rich@flatcap.org>
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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "color/lib.h"
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
  if (!fmt || !win)
    return;

  va_list ap;
  char buf[256], buf2[256];
  int w = (percent * win->state.cols) / 100;
  size_t l;

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  l = mutt_strwidth(buf);
  va_end(ap);

  mutt_simple_format(buf2, sizeof(buf2), 0, win->state.cols - 2, JUSTIFY_LEFT,
                     0, buf, sizeof(buf), false);

  mutt_window_move(win, 0, 0);

  if (simple_color_is_set(MT_COLOR_PROGRESS))
  {
    if (l < w)
    {
      /* The string fits within the colour bar */
      mutt_curses_set_normal_backed_color_by_id(MT_COLOR_PROGRESS);
      mutt_window_addstr(win, buf2);
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
      int off = mutt_wstr_trunc(buf2, sizeof(buf2), w, NULL);

      char ch = buf2[off];
      buf2[off] = '\0';
      mutt_curses_set_normal_backed_color_by_id(MT_COLOR_PROGRESS);
      mutt_window_addstr(win, buf2);
      buf2[off] = ch;
      mutt_curses_set_color_by_id(MT_COLOR_NORMAL);
      mutt_window_addstr(win, &buf2[off]);
    }
  }
  else
  {
    mutt_window_addstr(win, buf2);
  }

  mutt_window_clrtoeol(win);
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

  if (wdata->update_percent < 0)
    wdata->display_percent = 100.0 * wdata->display_pos / wdata->size;
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

  if (wdata->size == 0)
  {
    message_bar(wdata->win, wdata->display_percent, "%s %zu (%d%%)", wdata->msg,
                wdata->display_pos, wdata->display_percent);
  }
  else
  {
    if (wdata->is_bytes)
    {
      message_bar(wdata->win, wdata->display_percent, "%s %s/%s (%d%%)", wdata->msg,
                  wdata->pretty_pos, wdata->pretty_size, wdata->display_percent);
    }
    else
    {
      message_bar(wdata->win, wdata->display_percent, "%s %zu/%zu (%d%%)", wdata->msg,
                  wdata->display_pos, wdata->size, wdata->display_percent);
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

  if (wdata->size == 0)
  {
    if (!percent_needs_update(wdata, percent))
      return false;
  }
  else
  {
    if (!pos_needs_update(wdata, pos))
      return false;
  }

  const uint64_t now = mutt_date_epoch_ms();
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
 * @param msg      Progress message to display
 * @param size     Expected number of records or size of traffic
 * @param size_inc Size increment (step size)
 * @param time_inc Time increment
 * @param is_bytes true if measuing bytes
 * @retval ptr New Progress Window
 */
struct MuttWindow *progress_window_new(const char *msg, size_t size, size_t size_inc,
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
  mutt_str_copy(wdata->msg, msg, sizeof(wdata->msg));

  if (is_bytes)
    mutt_str_pretty_size(wdata->pretty_size, sizeof(wdata->pretty_size), size);

  win->wdata = wdata;

  return win;
}
