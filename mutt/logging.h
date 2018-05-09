/**
 * @file
 * Message logging
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef _LOGGING_H
#define _LOGGING_H

#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include "queue.h"

typedef int (*log_dispatcher_t)(time_t stamp, const char *file, int line, const char *function, int level, ...);

extern log_dispatcher_t MuttLogger;

/**
 * enum LogLevel - Names for the Logging Levels
 */
enum LogLevel
{
  LL_PERROR  = -3,
  LL_ERROR   = -2,
  LL_WARNING = -1,
  LL_MESSAGE =  0,
  LL_DEBUG1  =  1,
  LL_DEBUG2  =  2,
  LL_DEBUG3  =  3,
  LL_DEBUG4  =  4,
  LL_DEBUG5  =  5,
};

/**
 * struct LogLine - A Log line
 */
struct LogLine
{
  time_t time;
  const char *file;
  int line;
  const char *function;
  int level;
  char *message;
  STAILQ_ENTRY(LogLine) entries;
};

/**
 * struct LogList - A list of log lines
 *
 * The Log is stored as a STAILQ.
 * This means that insertions are quick at the head and tail of the list.
 */
STAILQ_HEAD(LogList, LogLine);

#define mutt_debug(LEVEL, ...) MuttLogger(0, __FILE__, __LINE__, __func__, LEVEL,      __VA_ARGS__)
#define mutt_warning(...)      MuttLogger(0, __FILE__, __LINE__, __func__, LL_WARNING, __VA_ARGS__)
#define mutt_message(...)      MuttLogger(0, __FILE__, __LINE__, __func__, LL_MESSAGE, __VA_ARGS__)
#define mutt_error(...)        MuttLogger(0, __FILE__, __LINE__, __func__, LL_ERROR,   __VA_ARGS__)
#define mutt_perror(...)       MuttLogger(0, __FILE__, __LINE__, __func__, LL_PERROR,  __VA_ARGS__)

int log_disp_file    (time_t stamp, const char *file, int line, const char *function, int level, ...);
int log_disp_queue   (time_t stamp, const char *file, int line, const char *function, int level, ...);
int log_disp_terminal(time_t stamp, const char *file, int line, const char *function, int level, ...);

int  log_queue_add(struct LogLine *ll);
void log_queue_empty(void);
void log_queue_flush(log_dispatcher_t disp);
int  log_queue_save(FILE *fp);
void log_queue_set_max_size(int size);

void log_file_close(bool verbose);
int  log_file_open(bool verbose);
bool log_file_running(void);
int  log_file_set_filename(const char *file, bool verbose);
int  log_file_set_level(int level, bool verbose);
void log_file_set_version(const char *version);

#endif /* _LOGGING_H */

