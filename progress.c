/**
 * @file
 * Progress bar
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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

#include "config.h"
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>
#include "mutt/mutt.h"
#include "mutt.h"
#include "progress.h"
#include "globals.h"
#include "mutt_curses.h"
#include "options.h"
#include "protos.h"

/**
 * message_bar - Draw a colourful progress bar
 * @param percent %age complete
 * @param fmt     printf(1)-like formatting string
 * @param ...     Arguments to formatting string
 */
static void message_bar(int percent, const char *fmt, ...)
{
  va_list ap;
  char buf[STRING], buf2[STRING];
  int w = percent * COLS / 100;
  size_t l;

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  l = mutt_strwidth(buf);
  va_end(ap);

  mutt_simple_format(buf2, sizeof(buf2), 0, COLS - 2, FMT_LEFT, 0, buf, sizeof(buf), 0);

  move(LINES - 1, 0);

  if (ColorDefs[MT_COLOR_PROGRESS] == 0)
  {
    addstr(buf2);
  }
  else
  {
    if (l < w)
    {
      /* The string fits within the colour bar */
      SETCOLOR(MT_COLOR_PROGRESS);
      addstr(buf2);
      w -= l;
      while (w-- > 0)
      {
        addch(' ');
      }
      NORMAL_COLOR;
    }
    else
    {
      /* The string is too long for the colour bar */
      char ch;
      int off = mutt_wstr_trunc(buf2, sizeof(buf2), w, NULL);

      ch = buf2[off];
      buf2[off] = '\0';
      SETCOLOR(MT_COLOR_PROGRESS);
      addstr(buf2);
      buf2[off] = ch;
      NORMAL_COLOR;
      addstr(&buf2[off]);
    }
  }

  clrtoeol();
  mutt_refresh();
}

/**
 * mutt_progress_init - Set up a progress bar
 * @param progress Progress bar
 * @param msg      Message to display
 * @param flags    Flags, e.g. #MUTT_PROGRESS_SIZE
 * @param inc      Increments to display (0 disables updates)
 * @param size     Total size of expected file / traffic
 */
void mutt_progress_init(struct Progress *progress, const char *msg,
                        unsigned short flags, unsigned short inc, size_t size)
{
  struct timeval tv = { 0, 0 };

  if (!progress)
    return;
  if (OPT_NO_CURSES)
    return;

  memset(progress, 0, sizeof(struct Progress));
  progress->inc = inc;
  progress->flags = flags;
  progress->msg = msg;
  progress->size = size;
  if (progress->size != 0)
  {
    if (progress->flags & MUTT_PROGRESS_SIZE)
      mutt_str_pretty_size(progress->sizestr, sizeof(progress->sizestr),
                           progress->size);
    else
      snprintf(progress->sizestr, sizeof(progress->sizestr), "%zu", progress->size);
  }
  if (inc == 0)
  {
    if (size != 0)
      mutt_message("%s (%s)", msg, progress->sizestr);
    else
      mutt_message(msg);
    return;
  }
  if (gettimeofday(&tv, NULL) < 0)
    mutt_debug(1, "gettimeofday failed: %d\n", errno);
  /* if timestamp is 0 no time-based suppression is done */
  if (TimeInc != 0)
    progress->timestamp =
        ((unsigned int) tv.tv_sec * 1000) + (unsigned int) (tv.tv_usec / 1000);
  mutt_progress_update(progress, 0, 0);
}

/**
 * mutt_progress_update - Update the state of the progress bar
 * @param progress Progress bar
 * @param pos      Position, or count
 * @param percent  Percentage complete
 *
 * If percent is -1, then the percentage will be calculated using pos and the
 * size in progress.
 */
void mutt_progress_update(struct Progress *progress, long pos, int percent)
{
  char posstr[SHORT_STRING];
  bool update = false;
  struct timeval tv = { 0, 0 };
  unsigned int now = 0;

  if (OPT_NO_CURSES)
    return;

  if (progress->inc == 0)
    goto out;

  /* refresh if size > inc */
  if ((progress->flags & MUTT_PROGRESS_SIZE) && (pos >= (progress->pos + (progress->inc << 10))))
    update = true;
  else if (pos >= (progress->pos + progress->inc))
    update = true;

  /* skip refresh if not enough time has passed */
  if (update && progress->timestamp && (gettimeofday(&tv, NULL) == 0))
  {
    now = ((unsigned int) tv.tv_sec * 1000) + (unsigned int) (tv.tv_usec / 1000);
    if (now && ((now - progress->timestamp) < TimeInc))
      update = false;
  }

  /* always show the first update */
  if (pos == 0)
    update = true;

  if (update)
  {
    if (progress->flags & MUTT_PROGRESS_SIZE)
    {
      pos = pos / (progress->inc << 10) * (progress->inc << 10);
      mutt_str_pretty_size(posstr, sizeof(posstr), pos);
    }
    else
      snprintf(posstr, sizeof(posstr), "%ld", pos);

    mutt_debug(5, "updating progress: %s\n", posstr);

    progress->pos = pos;
    if (now)
      progress->timestamp = now;

    if (progress->size > 0)
    {
      message_bar(
          (percent > 0) ? percent :
                          (int) (100.0 * (double) progress->pos / progress->size),
          "%s %s/%s (%d%%)", progress->msg, posstr, progress->sizestr,
          (percent > 0) ? percent :
                          (int) (100.0 * (double) progress->pos / progress->size));
    }
    else
    {
      if (percent > 0)
        message_bar(percent, "%s %s (%d%%)", progress->msg, posstr, percent);
      else
        mutt_message("%s %s", progress->msg, posstr);
    }
  }

out:
  if (pos >= progress->size)
    mutt_clear_error();
}

