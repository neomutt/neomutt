/**
 * @file
 * Progress bar
 *
 * @authors
 * Copyright (C) 2018-2021 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
 * @page progress_progress Progress Bar
 *
 * Progress bar
 */

#include "config.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "mutt_logging.h"
#include "muttlib.h"
#include "options.h"

/**
 * struct Progress - A Progress Bar
 */
struct Progress
{
  struct MuttWindow *win; ///< Window to draw on
  char msg[1024];         ///< Message to display
  char sizestr[24];       ///< String for percentage/size
  size_t pos;             ///< Current postion
  size_t size;            ///< Total expected size
  size_t inc;             ///< Increment size
  uint64_t timestamp;     ///< Time of last update
  bool is_bytes;          ///< true if measuring bytes
};

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

  if (mutt_color(MT_COLOR_PROGRESS) == 0)
  {
    mutt_window_addstr(win, buf2);
  }
  else
  {
    if (l < w)
    {
      /* The string fits within the colour bar */
      mutt_curses_set_color(MT_COLOR_PROGRESS);
      mutt_window_addstr(win, buf2);
      w -= l;
      while (w-- > 0)
      {
        mutt_window_addch(win, ' ');
      }
      mutt_curses_set_color(MT_COLOR_NORMAL);
    }
    else
    {
      /* The string is too long for the colour bar */
      int off = mutt_wstr_trunc(buf2, sizeof(buf2), w, NULL);

      char ch = buf2[off];
      buf2[off] = '\0';
      mutt_curses_set_color(MT_COLOR_PROGRESS);
      mutt_window_addstr(win, buf2);
      buf2[off] = ch;
      mutt_curses_set_color(MT_COLOR_NORMAL);
      mutt_window_addstr(win, &buf2[off]);
    }
  }

  mutt_window_clrtoeol(win);
  mutt_refresh();
}

/**
 * choose_increment - Choose the right increment given a ProgressType
 * @param type ProgressType
 * @retval Increment value
 */
static size_t choose_increment(enum ProgressType type)
{
  const short c_read_inc = cs_subset_number(NeoMutt->sub, "read_inc");
  const short c_write_inc = cs_subset_number(NeoMutt->sub, "write_inc");
  const short c_net_inc = cs_subset_number(NeoMutt->sub, "net_inc");
  const short *incs[] = { &c_read_inc, &c_write_inc, &c_net_inc };
  return (type >= mutt_array_size(incs)) ? 0 : *incs[type];
}

/**
 * pos_needs_update - Do we need to update, given the current pos?
 * @param progress Progress
 * @param pos      Current pos
 * @retval true Progress needs an update
 */
static bool pos_needs_update(const struct Progress *progress, long pos)
{
  const unsigned shift = progress->is_bytes ? 10 : 0;
  return pos >= (progress->pos + (progress->inc << shift));
}

/**
 * time_needs_update - Do we need to update, given the current time?
 * @param progress Progress
 * @param now      Current time
 * @retval true Progress needs an update
 */
static bool time_needs_update(const struct Progress *progress, size_t now)
{
  const size_t elapsed = (now - progress->timestamp);
  const short c_time_inc = cs_subset_number(NeoMutt->sub, "time_inc");
  return (c_time_inc == 0) || (now < progress->timestamp) || (c_time_inc < elapsed);
}

/**
 * progress_update - Update the state of the progress bar
 * @param progress Progress bar
 * @param pos      Position, or count
 * @param percent  Percentage complete
 *
 * If percent is -1, then the percentage will be calculated using pos and the
 * size in progress.
 *
 * If percent is positive, it is displayed as percentage, otherwise
 * percentage is calculated from progress->size and pos if progress
 * was initialized with positive size, otherwise no percentage is shown
 */
void progress_update(struct Progress *progress, size_t pos, int percent)
{
  if (!progress || OptNoCurses)
    return;

  const uint64_t now = mutt_date_epoch_ms();

  const bool update =
      (pos == 0) /* always show the first update */ ||
      (pos_needs_update(progress, pos) && time_needs_update(progress, now));

  if ((progress->inc != 0) && update)
  {
    progress->pos = pos;
    progress->timestamp = now;

    char posstr[128];
    if (progress->is_bytes)
    {
      const size_t round_pos =
          (progress->pos / (progress->inc << 10)) * (progress->inc << 10);
      mutt_str_pretty_size(posstr, sizeof(posstr), round_pos);
    }
    else
    {
      snprintf(posstr, sizeof(posstr), "%zu", progress->pos);
    }

    mutt_debug(LL_DEBUG4, "updating progress: %s\n", posstr);

    if (progress->size != 0)
    {
      if (percent < 0)
      {
        percent = 100.0 * progress->pos / progress->size;
      }
      message_bar(progress->win, percent, "%s %s/%s (%d%%)", progress->msg,
                  posstr, progress->sizestr, percent);
    }
    else
    {
      if (percent > 0)
        message_bar(progress->win, percent, "%s %s (%d%%)", progress->msg, posstr, percent);
      else
        mutt_message("%s %s", progress->msg, posstr);
    }
  }

  if (progress->pos >= progress->size)
    mutt_clear_error();
}

/**
 * progress_free - Free a Progress Bar
 * @param ptr Progress Bar to free
 */
void progress_free(struct Progress **ptr)
{
  if (!ptr || !*ptr)
    return;

  // struct Progress *progress = *ptr;

  FREE(ptr);
}

/**
 * progress_new - Create a new Progress Bar
 * @param msg  Message to display; this is copied into the Progress object
 * @param type Type, e.g. #MUTT_PROGRESS_READ
 * @param size Total size of expected file / traffic
 * @retval ptr New Progress Bar
 */
struct Progress *progress_new(const char *msg, enum ProgressType type, size_t size)
{
  if (OptNoCurses)
    return NULL;

  struct Progress *progress = mutt_mem_calloc(1, sizeof(struct Progress));

  /* Initialize Progress structure */
  progress->win = msgwin_get_window();
  mutt_str_copy(progress->msg, msg, sizeof(progress->msg));
  progress->size = size;
  progress->inc = choose_increment(type);
  progress->is_bytes = (type == MUTT_PROGRESS_NET);

  /* Generate the size string, if a total size was specified */
  if (progress->size != 0)
  {
    if (progress->is_bytes)
    {
      mutt_str_pretty_size(progress->sizestr, sizeof(progress->sizestr),
                           progress->size);
    }
    else
    {
      snprintf(progress->sizestr, sizeof(progress->sizestr), "%zu", progress->size);
    }
  }

  if (progress->inc == 0)
  {
    /* This progress bar does not increment - write the initial message */
    if (progress->size == 0)
    {
      mutt_message(progress->msg);
    }
    else
    {
      mutt_message("%s (%s)", progress->msg, progress->sizestr);
    }
  }
  else
  {
    /* This progress bar does increment - perform the initial update */
    progress_update(progress, 0, 0);
  }
  return progress;
}
