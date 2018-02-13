/**
 * @file
 * Mutt Logging
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

/**
 * @page mutt_logging Mutt Logging
 *
 * Mutt Logging
 *
 * | File                 | Description
 * | :------------------- | :-----------------------------------------------
 * | mutt_clear_error()   | Clear the message line (bottom line of screen)
 */

#include "config.h"
#include <errno.h>
#include <limits.h>
#include <sys/time.h>
#include <time.h>
#include "mutt/mutt.h"
#include "globals.h"
#include "mutt_curses.h"

struct timeval LastError = { 0 };

/**
 * micro_elapsed - Number of microseconds between two timevals
 * @param begin Begin time
 * @param end   End time
 * @retval num      Microseconds elapsed
 * @retval LONG_MAX Begin time was zero
 */
static long micro_elapsed(const struct timeval *begin, const struct timeval *end)
{
  if ((begin->tv_sec == 0) && (end->tv_sec != 0))
    return LONG_MAX;

  return ((end->tv_sec - begin->tv_sec) * 1000000) + (end->tv_usec - begin->tv_usec);
}

/**
 * error_pause - Wait for an error message to be read
 *
 * If a second hasn't elapsed since LastError, then wait.
 */
void error_pause(void)
{
  struct timeval now = { 0 };

  if (gettimeofday(&now, NULL) < 0)
  {
    mutt_debug(1, "gettimeofday failed: %d\n", errno);
    return;
  }

  long micro = micro_elapsed(&LastError, &now);
  if (micro >= 1000000)
    return;

  struct timespec wait = { 0 };
  wait.tv_nsec = 1000000000 - (micro * 10);

  mutt_refresh();
  nanosleep(&wait, NULL);
}

/**
 * mutt_clear_error - Clear the message line (bottom line of screen)
 */
void mutt_clear_error(void)
{
  /* Make sure the error message has had time to be read */
  if (OPT_MSG_ERR)
    error_pause();

  ErrorBuf[0] = 0;
  if (!OPT_NO_CURSES)
    mutt_window_clearline(MuttMessageWindow, 0);
}

