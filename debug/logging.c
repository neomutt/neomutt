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
  char buf[LOG_LINE_MAX_LEN] = { 0 };
  size_t buflen = sizeof(buf);
  int err = errno;
  int colour = 0;
  int bytes = 0;

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
      bytes += snprintf(buf + bytes, buflen - bytes, "\033[1;%dm", colour); // Escape
    }
  }

  if (DebugLogTimestamp)
  {
    bytes += log_timestamp(buf + bytes, buflen - bytes, stamp);
  }

  if (DebugLogLevel)
  {
    bytes += log_level(buf + bytes, buflen - bytes, level);
  }

  va_list ap;
  va_start(ap, format);
  bytes += vsnprintf(buf + bytes, buflen - bytes, format, ap);
  va_end(ap);

  if (level == LL_PERROR)
    bytes += snprintf(buf + bytes, buflen - bytes, ": %s", strerror(err));

  if (colour > 0)
  {
    bytes += snprintf(buf + bytes, buflen - bytes, "\033[0m"); // Escape
  }

  if (level < LL_DEBUG1)
    bytes += snprintf(buf + bytes, buflen - bytes, "\n");

  fputs(buf, stdout);
  return bytes;
}
