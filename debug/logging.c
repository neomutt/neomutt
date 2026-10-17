/**
 * @file
 * Log everything to the terminal
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
 * @page debug_logging Log everything to the terminal
 *
 * Log everything to the terminal
 */

#include "config.h"
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "mutt/lib.h"
#include "lib.h"

bool DebugLogColor = false;     ///< Output ANSI colours
bool DebugLogLevel = false;     ///< Prefix log level, e.g. [E]
bool DebugLogTimestamp = false; ///< Show the timestamp

extern const char *LevelAbbr;

/**
 * log_timestamp - Write a timestamp to a buffer
 * @param buf    Buffer for the result
 * @param buflen Length of the buffer
 * @param time   Timestamp
 * @retval num Bytes written to buffer
 */
static int log_timestamp(char *buf, size_t buflen, time_t time)
{
  return mutt_date_localtime_format(buf, buflen, "[%H:%M:%S]", time);
}

/**
 * log_level - Write a log level to a buffer
 * @param buf    Buffer for the result
 * @param buflen Length of the buffer
 * @param level  Log level
 * @retval num Bytes written to buffer
 *
 * Write the log level, e.g. [E]
 */
static int log_level(char *buf, size_t buflen, enum LogLevel level)
{
  return snprintf(buf, buflen, "<%c>", LevelAbbr[level + 3]);
}

/**
 * log_disp_debug - Display a log line on screen - Implements ::log_dispatcher_t - @ingroup logging_api
 */
int log_disp_debug(time_t stamp, const char *file, int line, const char *function,
                   enum LogLevel level, const char *format, ...)
{
  struct Buffer *buf = buf_pool_get();
  int err = errno;
  int colour = 0;

  if (DebugLogColor)
  {
    switch (level)
    {
      case LL_PERROR:
      case LL_ERROR:
        colour = 31;
        break;
      case LL_WARNING:
        colour = 33;
        break;
      case LL_MESSAGE:
      default:
        break;
    }

    if (colour > 0)
    {
      buf_add_printf(buf, "\033[1;%dm", colour); // Escape
    }
  }

  if (DebugLogTimestamp)
  {
    char ts[32] = { 0 };
    log_timestamp(ts, sizeof(ts), stamp);
    buf_addstr(buf, ts);
  }

  if (DebugLogLevel)
  {
    char lvl[8] = { 0 };
    log_level(lvl, sizeof(lvl), level);
    buf_addstr(buf, lvl);
  }

  va_list ap;
  va_start(ap, format);
  buf_vaprintf(buf, format, ap);
  va_end(ap);

  if (level == LL_PERROR)
    buf_add_printf(buf, ": %s", strerror(err));

  if (colour > 0)
  {
    buf_addstr(buf, "\033[0m"); // Escape
  }

  if (level < LL_DEBUG1)
    buf_addch(buf, '\n');

  fputs(buf_string(buf), stdout);

  int bytes = buf_len(buf);
  buf_pool_release(&buf);
  return bytes;
}
