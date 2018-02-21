/**
 * @file
 * Debug messages
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

/**
 * @page debug Debug messages
 *
 * Output debugging messages, suitable for a developer.
 *
 * | Function          | Description
 * | :---------------- | :--------------------------------
 * | mutt_debug_real() | Output some debugging information
 */

#include "config.h"
#include <stdarg.h>
#include <stdio.h>
#include "debug.h"

/**
 * mutt_debug_real - Output some debugging information
 * @param function Function
 * @param file     File
 * @param line     Line number
 * @param level    Debug level
 * @param ...      Arguments to be formatted
 *
 * This stub function ignores the logging level and outputs all information to
 * stderr.
 */
int mutt_debug_real(const char *UNUSED(function), const char *UNUSED(file),
                    int UNUSED(line), int level, ...)
{
  va_list ap;
  va_start(ap, level);
  const char *fmt = va_arg(ap, const char *);
  int ret = vfprintf(stderr, fmt, ap);
  va_end(ap);
  return ret;
}
