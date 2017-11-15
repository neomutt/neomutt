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

/**
 * @page message Message logging
 *
 * Display informational messages for the user.
 *
 * These library stubs print the messages to stdout/stderr.
 *
 * | Function          | Description
 * | :---------------- | :--------------------------------------------
 * | default_error()   | Display an error message
 * | default_message() | Display an informative message
 * | default_perror()  | Lookup a standard error message (using errno)
 */

#include "config.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "message.h"

/**
 * default_error - Display an error message
 * @param format printf-like formatting string
 * @param ...    Arguments to format
 *
 * This stub function writes to stderr.
 */
static void default_error(const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);
  fputc('\n', stderr);
}

void (*mutt_error)(const char *, ...) = default_error;

/**
 * default_message - Display an informative message
 * @param format printf-like formatting string
 * @param ...    Arguments to format
 *
 * This stub function writes to stdout.
 */
static void default_message(const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  vfprintf(stdout, format, ap);
  va_end(ap);
  fputc('\n', stdout);
}

void (*mutt_message)(const char *, ...) = default_message;

/**
 * default_perror - Lookup a standard error message (using errno)
 * @param message Prefix message to display
 *
 * This stub function writes to stderr.
 */
static void default_perror(const char *message)
{
  char *p = strerror(errno);

  mutt_error("%s: %s (errno = %d)", message, p ? p : _("unknown error"), errno);
}

void (*mutt_perror)(const char *) = default_perror;
