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
 */

#include "config.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "mutt/mutt.h"
#include "globals.h"
#include "mutt_curses.h"
#include "mutt_window.h"
#include "protos.h"

struct timeval LastError = { 0 };

short DebugLevel = 0;    /**< Log file logging level */
char *DebugFile = NULL;  /**< Log file name */
const int NumOfLogs = 5; /**< How many log files to rotate */
bool LogAllowDebugSet = false;

#define S_TO_NS 1000000000UL
#define S_TO_US 1000000UL
#define US_TO_NS 1000UL

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

  return ((end->tv_sec - begin->tv_sec) * S_TO_US) + (end->tv_usec - begin->tv_usec);
}

/**
 * error_pause - Wait for an error message to be read
 *
 * If '$sleep_time' seconds hasn't elapsed since LastError, then wait
 */
static void error_pause(void)
{
  struct timeval now = { 0 };

  if (gettimeofday(&now, NULL) < 0)
  {
    mutt_debug(1, "gettimeofday failed: %d\n", errno);
    return;
  }

  unsigned long sleep = SleepTime * S_TO_NS;
  long micro = micro_elapsed(&LastError, &now);
  if ((micro * US_TO_NS) >= sleep)
    return;

  sleep -= (micro * US_TO_NS);

  struct timespec wait = {
    .tv_sec = (sleep / S_TO_NS),
    .tv_nsec = (sleep % S_TO_NS),
  };

  mutt_refresh();
  nanosleep(&wait, NULL);
}

/**
 * rotate_logs - Rotate a set of numbered files
 * @param file  Template filename
 * @param count Maximum number of files
 * @retval ptr Name of the 0'th file
 *
 * Given a template 'temp', rename files numbered 0 to (count-1).
 *
 * Rename:
 * - ...
 * - temp1 -> temp2
 * - temp0 -> temp1
 */
static const char *rotate_logs(const char *file, int count)
{
  if (!file)
    return NULL;

  char old[PATH_MAX];
  char new[PATH_MAX];

  /* rotate the old debug logs */
  for (count -= 2; count >= 0; count--)
  {
    snprintf(old, sizeof(old), "%s%d", file, count);
    snprintf(new, sizeof(new), "%s%d", file, count + 1);

    mutt_expand_path(old, sizeof(old));
    mutt_expand_path(new, sizeof(new));
    rename(old, new);
  }

  return mutt_str_strdup(old);
}

/**
 * mutt_clear_error - Clear the message line (bottom line of screen)
 */
void mutt_clear_error(void)
{
  /* Make sure the error message has had time to be read */
  if (OPT_MSG_ERR)
    error_pause();

  ErrorBufMessage = false;
  if (!OPT_NO_CURSES)
    mutt_window_clearline(MuttMessageWindow, 0);
}

/**
 * log_disp_curses - Display a log line in the message line
 * @param stamp    Unix time
 * @param file     Source file
 * @param line     Source line
 * @param function Source function
 * @param level    Logging level, e.g. #LL_WARNING
 * @param ...      Format string and parameters, like printf()
 * @retval >0 Success, number of characters written
 */
int log_disp_curses(time_t stamp, const char *file, int line,
                    const char *function, int level, ...)
{
  if (level > DebugLevel)
    return 0;

  char buf[LONG_STRING];

  va_list ap;
  va_start(ap, level);
  const char *fmt = va_arg(ap, const char *);
  int ret = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  if (level == LL_PERROR)
  {
    char *buf2 = buf + ret;
    int len = sizeof(buf) - ret;
    char *p = strerror(errno);
    if (!p)
      p = _("unknown error");

    ret += snprintf(buf2, len, ": %s (errno = %d)", p, errno);
  }

  bool dupe = (strcmp(buf, ErrorBuf) == 0);
  if (!dupe)
  {
    /* Only log unique messages */
    log_disp_file(stamp, file, line, function, level, "%s", buf);
    if (stamp == 0)
      log_disp_queue(stamp, file, line, function, level, "%s", buf);
  }

  /* Don't display debugging message on screen */
  if (level > LL_MESSAGE)
    return 0;

  /* Only pause if this is a message following an error */
  if ((level > LL_ERROR) && OPT_MSG_ERR && !dupe)
    error_pause();

  mutt_simple_format(ErrorBuf, sizeof(ErrorBuf), 0, MuttMessageWindow->cols,
                     FMT_LEFT, 0, buf, sizeof(buf), 0);
  ErrorBufMessage = true;

  if (!OPT_KEEP_QUIET)
  {
    if (level == LL_ERROR)
      BEEP();
    SETCOLOR((level == LL_ERROR) ? MT_COLOR_ERROR : MT_COLOR_MESSAGE);
    mutt_window_mvaddstr(MuttMessageWindow, 0, 0, ErrorBuf);
    NORMAL_COLOR;
    mutt_window_clrtoeol(MuttMessageWindow);
    mutt_refresh();
  }

  if ((level <= LL_ERROR) && !dupe)
  {
    OPT_MSG_ERR = true;
    if (gettimeofday(&LastError, NULL) < 0)
      mutt_debug(1, "gettimeofday failed: %d\n", errno);
  }
  else
  {
    OPT_MSG_ERR = false;
    LastError.tv_sec = 0;
  }

  return ret;
}

/**
 * mutt_log_start - Enable file logging
 *
 * This also handles file rotation.
 */
int mutt_log_start(void)
{
  if (DebugLevel < 1)
    return 0;

  if (log_file_running())
    return 0;

  char ver[64];
  snprintf(ver, sizeof(ver), "-%s%s", PACKAGE_VERSION, GitVer);
  log_file_set_version(ver);

  const char *name = rotate_logs(DebugFile, NumOfLogs);
  if (!name)
    return -1;

  log_file_set_filename(name, false);
  FREE(&name);

  /* This will trigger the file creation */
  if (log_file_set_level(DebugLevel, true) < 1)
    return -1;

  return 0;
}

/**
 * mutt_log_stop - Close the log file
 */
void mutt_log_stop(void)
{
  log_file_close(false);
}

/**
 * mutt_log_set_level - Change the logging level
 * @param level Logging level
 * @param verbose If true, then log the event
 * @retval  0 Success
 * @retval -1 Error, level is out of range
 */
int mutt_log_set_level(int level, bool verbose)
{
  if (log_file_set_level(level, verbose) != 0)
    return -1;

  if (!LogAllowDebugSet)
    return 0;

  DebugLevel = level;
  return 0;
}

/**
 * mutt_log_set_file - Change the logging file
 * @param file Name to use
 * @param verbose If true, then log the event
 * @retval  0 Success, file opened
 * @retval -1 Error, see errno
 *
 * Close the old log, rotate the new logs and open the new log.
 */
int mutt_log_set_file(const char *file, bool verbose)
{
  if (mutt_str_strcmp(DebugFile, file) == 0)
    return 0;

  if (!LogAllowDebugSet)
    return 0;

  const char *name = rotate_logs(file, NumOfLogs);
  if (!name)
    return 0;

  log_file_set_filename(name, verbose);
  FREE(&name);
  mutt_str_replace(&DebugFile, file);

  return 0;
}
