/**
 * @file
 * Logging Dispatcher
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
 * @page logging Logging Dispatcher
 *
 * Logging Dispatcher
 */

#include "config.h"
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "logging.h"
#include "date.h"
#include "file.h"
#include "memory.h"
#include "message.h"
#include "queue.h"
#include "string2.h"

const char *LevelAbbr = "PEWM12345N"; ///< Abbreviations of logging level names

/**
 * MuttLogger - The log dispatcher
 *
 * This function pointer controls where log messages are redirected.
 */
log_dispatcher_t MuttLogger = log_disp_terminal;

FILE *LogFileFP = NULL;      ///< Log file handle
char *LogFileName = NULL;    ///< Log file name
int LogFileLevel = 0;        ///< Log file level
char *LogFileVersion = NULL; ///< Program version

/**
 * LogQueue - In-memory list of log lines
 */
static struct LogLineList LogQueue = STAILQ_HEAD_INITIALIZER(LogQueue);

int LogQueueCount = 0; ///< Number of entries currently in the log queue
int LogQueueMax = 0;   ///< Maximum number of entries in the log queue

/**
 * timestamp - Create a YYYY-MM-DD HH:MM:SS timestamp
 * @param stamp Unix time
 * @retval ptr Timestamp string
 *
 * If stamp is 0, then the current time will be used.
 *
 * @note This function returns a pointer to a static buffer.
 *       Do not free it.
 */
static const char *timestamp(time_t stamp)
{
  static char buf[23] = { 0 };
  static time_t last = 0;

  if (stamp == 0)
    stamp = mutt_date_epoch();

  if (stamp != last)
  {
    mutt_date_localtime_format(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", stamp);
    last = stamp;
  }

  return buf;
}

/**
 * log_file_close - Close the log file
 * @param verbose If true, then log the event
 */
void log_file_close(bool verbose)
{
  if (!LogFileFP)
    return;

  fprintf(LogFileFP, "[%s] Closing log.\n", timestamp(0));
  fprintf(LogFileFP, "# vim: syntax=neomuttlog\n");
  mutt_file_fclose(&LogFileFP);
  if (verbose)
    mutt_message(_("Closed log file: %s"), LogFileName);
}

/**
 * log_file_open - Start logging to a file
 * @param verbose If true, then log the event
 * @retval  0 Success
 * @retval -1 Error, see errno
 *
 * Before opening a log file, call log_file_set_version(), log_file_set_level()
 * and log_file_set_filename().
 */
int log_file_open(bool verbose)
{
  if (!LogFileName)
    return -1;

  if (LogFileFP)
    log_file_close(false);

  if (LogFileLevel < LL_DEBUG1)
    return -1;

  LogFileFP = mutt_file_fopen(LogFileName, "a+");
  if (!LogFileFP)
    return -1;
  setvbuf(LogFileFP, NULL, _IOLBF, 0);

  fprintf(LogFileFP, "[%s] NeoMutt%s debugging at level %d\n", timestamp(0),
          NONULL(LogFileVersion), LogFileLevel);
  if (verbose)
    mutt_message(_("Debugging at level %d to file '%s'"), LogFileLevel, LogFileName);
  return 0;
}

/**
 * log_file_set_filename - Set the filename for the log
 * @param file Name to use
 * @param verbose If true, then log the event
 * @retval  0 Success, file opened
 * @retval -1 Error, see errno
 */
int log_file_set_filename(const char *file, bool verbose)
{
  if (!file)
    return -1;

  /* also handles both being NULL */
  if (mutt_str_equal(LogFileName, file))
    return 0;

  mutt_str_replace(&LogFileName, file);

  if (!LogFileName)
    log_file_close(verbose);

  return log_file_open(verbose);
}

/**
 * log_file_set_level - Set the logging level
 * @param level Logging level
 * @param verbose If true, then log the event
 * @retval  0 Success
 * @retval -1 Error, level is out of range
 *
 * The level should be: LL_MESSAGE <= level < LL_MAX.
 */
int log_file_set_level(enum LogLevel level, bool verbose)
{
  if ((level < LL_MESSAGE) || (level >= LL_MAX))
    return -1;

  if (level == LogFileLevel)
    return 0;

  LogFileLevel = level;

  if (level == LL_MESSAGE)
  {
    log_file_close(verbose);
  }
  else if (LogFileFP)
  {
    if (verbose)
      mutt_message(_("Logging at level %d to file '%s'"), LogFileLevel, LogFileName);
    fprintf(LogFileFP, "[%s] NeoMutt%s debugging at level %d\n", timestamp(0),
            NONULL(LogFileVersion), LogFileLevel);
  }
  else
  {
    log_file_open(verbose);
  }

  if (LogFileLevel >= LL_DEBUG5)
  {
    fprintf(LogFileFP,
            "\n"
            "WARNING:\n"
            "    Logging at this level can reveal personal information.\n"
            "    Review the log carefully before posting in bug reports.\n"
            "\n");
  }

  return 0;
}

/**
 * log_file_set_version - Set the program's version number
 * @param version Version number
 *
 * The string will be appended directly to 'NeoMutt', so it should begin with a
 * hyphen.
 */
void log_file_set_version(const char *version)
{
  mutt_str_replace(&LogFileVersion, version);
}

/**
 * log_file_running - Is the log file running?
 * @retval true If the log file is running
 */
bool log_file_running(void)
{
  return LogFileFP;
}

/**
 * log_disp_file - Save a log line to a file
 * @param stamp    Unix time (optional)
 * @param file     Source file (UNUSED)
 * @param line     Source line (UNUSED)
 * @param function Source function
 * @param level    Logging level, e.g. #LL_WARNING
 * @param ...      Format string and parameters, like printf()
 * @retval -1 Error
 * @retval  0 Success, filtered
 * @retval >0 Success, number of characters written
 *
 * This log dispatcher saves a line of text to a file.  The format is:
 * * `[TIMESTAMP]<LEVEL> FUNCTION() FORMATTED-MESSAGE`
 *
 * The caller must first set #LogFileName and #LogFileLevel, then call
 * log_file_open().  Any logging above #LogFileLevel will be ignored.
 *
 * If stamp is 0, then the current time will be used.
 */
int log_disp_file(time_t stamp, const char *file, int line,
                  const char *function, enum LogLevel level, ...)
{
  if (!LogFileFP || (level < LL_PERROR) || (level > LogFileLevel))
    return 0;

  int ret = 0;
  int err = errno;

  if (!function)
    function = "UNKNOWN";

  ret += fprintf(LogFileFP, "[%s]<%c> %s() ", timestamp(stamp),
                 LevelAbbr[level + 3], function);

  va_list ap;
  va_start(ap, level);
  const char *fmt = va_arg(ap, const char *);
  ret += vfprintf(LogFileFP, fmt, ap);
  va_end(ap);

  if (level == LL_PERROR)
  {
    fprintf(LogFileFP, ": %s\n", strerror(err));
  }
  else if (level <= LL_MESSAGE)
  {
    fputs("\n", LogFileFP);
    ret++;
  }

  return ret;
}

/**
 * log_queue_add - Add a LogLine to the queue
 * @param ll LogLine to add
 * @retval num Entries in the queue
 *
 * If #LogQueueMax is non-zero, the queue will be limited to this many items.
 */
int log_queue_add(struct LogLine *ll)
{
  if (!ll)
    return -1;

  STAILQ_INSERT_TAIL(&LogQueue, ll, entries);

  if ((LogQueueMax > 0) && (LogQueueCount >= LogQueueMax))
  {
    ll = STAILQ_FIRST(&LogQueue);
    STAILQ_REMOVE_HEAD(&LogQueue, entries);
    FREE(&ll->message);
    FREE(&ll);
  }
  else
  {
    LogQueueCount++;
  }
  return LogQueueCount;
}

/**
 * log_queue_set_max_size - Set a upper limit for the queue length
 * @param size New maximum queue length
 *
 * @note size of 0 means unlimited
 */
void log_queue_set_max_size(int size)
{
  if (size < 0)
    size = 0;
  LogQueueMax = size;
}

/**
 * log_queue_empty - Free the contents of the queue
 *
 * Free any log lines in the queue.
 */
void log_queue_empty(void)
{
  struct LogLine *ll = NULL;
  struct LogLine *tmp = NULL;

  STAILQ_FOREACH_SAFE(ll, &LogQueue, entries, tmp)
  {
    STAILQ_REMOVE(&LogQueue, ll, LogLine, entries);
    FREE(&ll->message);
    FREE(&ll);
  }

  LogQueueCount = 0;
}

/**
 * log_queue_flush - Replay the log queue
 * @param disp Log dispatcher - Implements ::log_dispatcher_t
 *
 * Pass all of the log entries in the queue to the log dispatcher provided.
 * The queue will be emptied afterwards.
 */
void log_queue_flush(log_dispatcher_t disp)
{
  struct LogLine *ll = NULL;
  STAILQ_FOREACH(ll, &LogQueue, entries)
  {
    disp(ll->time, ll->file, ll->line, ll->function, ll->level, "%s", ll->message);
  }

  log_queue_empty();
}

/**
 * log_queue_save - Save the contents of the queue to a temporary file
 * @param fp Open file handle
 * @retval num Lines written to the file
 *
 * The queue is written to a temporary file.  The format is:
 * * `[HH:MM:SS]<LEVEL> FORMATTED-MESSAGE`
 *
 * @note The caller should free the returned string and delete the file.
 */
int log_queue_save(FILE *fp)
{
  if (!fp)
    return 0;

  char buf[32];
  int count = 0;
  struct LogLine *ll = NULL;
  STAILQ_FOREACH(ll, &LogQueue, entries)
  {
    mutt_date_localtime_format(buf, sizeof(buf), "%H:%M:%S", ll->time);
    fprintf(fp, "[%s]<%c> %s", buf, LevelAbbr[ll->level + 3], ll->message);
    if (ll->level <= LL_MESSAGE)
      fputs("\n", fp);
    count++;
  }

  return count;
}

/**
 * log_disp_queue - Save a log line to an internal queue - Implements ::log_dispatcher_t
 *
 * This log dispatcher saves a line of text to a queue.
 * The format string and parameters are expanded and the other parameters are
 * stored as they are.
 *
 * @sa log_queue_set_max_size(), log_queue_flush(), log_queue_empty()
 *
 * @warning Log lines are limited to 1024 bytes.
 */
int log_disp_queue(time_t stamp, const char *file, int line,
                   const char *function, enum LogLevel level, ...)
{
  char buf[1024] = { 0 };
  int err = errno;

  va_list ap;
  va_start(ap, level);
  const char *fmt = va_arg(ap, const char *);
  int ret = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  if (level == LL_PERROR)
  {
    ret += snprintf(buf + ret, sizeof(buf) - ret, ": %s", strerror(err));
    level = LL_ERROR;
  }

  struct LogLine *ll = mutt_mem_calloc(1, sizeof(*ll));
  ll->time = (stamp != 0) ? stamp : mutt_date_epoch();
  ll->file = file;
  ll->line = line;
  ll->function = function;
  ll->level = level;
  ll->message = mutt_str_dup(buf);

  log_queue_add(ll);

  return ret;
}

/**
 * log_disp_terminal - Save a log line to the terminal - Implements ::log_dispatcher_t
 *
 * This log dispatcher saves a line of text to the terminal.
 * The format is:
 * * `[TIMESTAMP]<LEVEL> FUNCTION() FORMATTED-MESSAGE`
 *
 * @note The output will be coloured using ANSI escape sequences,
 *       unless the output is redirected.
 */
int log_disp_terminal(time_t stamp, const char *file, int line,
                      const char *function, enum LogLevel level, ...)
{
  if ((level < LL_PERROR) || (level > LL_MESSAGE))
    return 0;

  char buf[1024];

  va_list ap;
  va_start(ap, level);
  const char *fmt = va_arg(ap, const char *);
  int ret = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  log_disp_file(stamp, file, line, function, level, "%s", buf);

  FILE *fp = (level < LL_MESSAGE) ? stderr : stdout;
  int err = errno;
  int colour = 0;
  bool tty = (isatty(fileno(fp)) == 1);

  if (tty)
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
        // colour = 36;
        break;
      case LL_DEBUG1:
      case LL_DEBUG2:
      case LL_DEBUG3:
      case LL_DEBUG4:
      case LL_DEBUG5:
      case LL_NOTIFY:
      default:
        break;
    }
  }

  if (colour > 0)
    ret += fprintf(fp, "\033[1;%dm", colour); // Escape

  fputs(buf, fp);

  if (level == LL_PERROR)
    ret += fprintf(fp, ": %s", strerror(err));

  if (colour > 0)
    ret += fprintf(fp, "\033[0m"); // Escape

  ret += fprintf(fp, "\n");

  return ret;
}

/**
 * log_disp_null - Discard log lines - Implements ::log_dispatcher_t
 */
int log_disp_null(time_t stamp, const char *file, int line,
                  const char *function, enum LogLevel level, ...)
{
  return 0;
}
