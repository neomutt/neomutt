/**
 * @file
 * Parse Error
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
 * @page parse_error Parse Error
 *
 * Parse Error
 */

#include "config.h"
#include <stdarg.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "perror.h"
#include "pcontext.h"

/**
 * parse_error_new - Create a new ParseError
 * @retval ptr New ParseError
 */
struct ParseError *parse_error_new(void)
{
  struct ParseError *pc = MUTT_MEM_CALLOC(1, struct ParseError);

  pc->message = buf_pool_get();

  return pc;
}

/**
 * parse_error_free - Free a ParseError
 * @param pptr ParseError to free
 */
void parse_error_free(struct ParseError **pptr)
{
  if (!pptr || !*pptr)
    return;

  struct ParseError *pe = *pptr;

  buf_pool_release(&pe->message);

  buf_dealloc(pe->message);
  FREE(&pe->filename);
  pe->lineno = 0;
  pe->origin = CO_CONFIG_FILE;
  pe->result = MUTT_CMD_SUCCESS;

  FREE(pptr);
}

/**
 * parse_error_reset - Clear the contents of a ParseError
 * @param pe ParseError to clear
 */
void parse_error_reset(struct ParseError *pe)
{
  buf_reset(pe->message);
}

/**
 * parse_error_init - Initialize a ParseError
 * @param pe ParseError to initialize
 */
void parse_error_init(struct ParseError *pe)
{
  if (!pe)
    return;

  buf_init(pe->message);
  pe->filename = NULL;
  pe->lineno = 0;
  pe->origin = CO_CONFIG_FILE;
  pe->result = MUTT_CMD_SUCCESS;
}

/**
 * parse_error_set - Set error information
 * @param pe       ParseError to set
 * @param result   Error result code
 * @param filename Filename where error occurred (may be NULL)
 * @param lineno   Line number where error occurred (0 if N/A)
 * @param fmt      Printf-style format string
 * @param ...      Variable arguments
 */
void parse_error_set(struct ParseError *pe, enum CommandResult result,
                            const char *filename, int lineno, const char *fmt, ...)
{
  if (!pe)
    return;

  pe->result = result;
  mutt_str_replace(&pe->filename, filename);
  pe->lineno = lineno;

  if (fmt)
  {
    va_list ap;
    va_start(ap, fmt);
    buf_reset(pe->message);
    /* Use a reasonably large buffer - this is consistent with other
     * logging code in the codebase (e.g., mutt/logging.c) */
    char buf[2048];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    buf_strcpy(pe->message, buf);
    va_end(ap);
  }
}
