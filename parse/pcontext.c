/**
 * @file
 * Config parse context and error structures
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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
 * @page parse_pcontext Config parse context
 *
 * Config parse context and error structures for improved error reporting.
 */

#include "config.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "mutt/lib.h"
#include "pcontext.h"

/**
 * file_location_init - Initialize a FileLocation
 * @param fl       FileLocation to initialize
 * @param filename Path to the file
 * @param lineno   Line number (1-based)
 */
void file_location_init(struct FileLocation *fl, const char *filename, int lineno)
{
  if (!fl)
    return;

  fl->filename = mutt_str_dup(filename);
  fl->lineno = lineno;
}

/**
 * file_location_free - Free a FileLocation's contents
 * @param fl FileLocation to free
 */
void file_location_free(struct FileLocation *fl)
{
  if (!fl)
    return;

  FREE(&fl->filename);
  fl->lineno = 0;
}

/**
 * parse_context_init - Initialize a ParseContext
 * @param pctx   ParseContext to initialize
 * @param origin Origin of the command
 */
void parse_context_init(struct ParseContext *pctx, enum CommandOrigin origin)
{
  if (!pctx)
    return;

  ARRAY_INIT(&pctx->locations);
  pctx->origin = origin;
  pctx->hook_id = CMD_NONE;
}

/**
 * parse_context_free - Free a ParseContext's contents
 * @param pctx ParseContext to free
 */
void parse_context_free(struct ParseContext *pctx)
{
  if (!pctx)
    return;

  struct FileLocation *fl = NULL;
  ARRAY_FOREACH(fl, &pctx->locations)
  {
    file_location_free(fl);
  }
  ARRAY_FREE(&pctx->locations);
  pctx->origin = CO_CONFIG_FILE;
  pctx->hook_id = CMD_NONE;
}

/**
 * parse_context_push - Push a file location onto the context stack
 * @param pctx     ParseContext
 * @param filename Path to the file
 * @param lineno   Line number (1-based)
 */
void parse_context_push(struct ParseContext *pctx, const char *filename, int lineno)
{
  if (!pctx)
    return;

  struct FileLocation fl = { 0 };
  file_location_init(&fl, filename, lineno);
  ARRAY_ADD(&pctx->locations, fl);
}

/**
 * parse_context_pop - Pop a file location from the context stack
 * @param pctx ParseContext
 */
void parse_context_pop(struct ParseContext *pctx)
{
  if (!pctx || ARRAY_EMPTY(&pctx->locations))
    return;

  struct FileLocation *fl = ARRAY_LAST(&pctx->locations);
  if (fl)
    file_location_free(fl);
  ARRAY_SHRINK(&pctx->locations, 1);
}

/**
 * parse_context_current - Get the current (top) file location
 * @param pctx ParseContext
 * @retval ptr  Current FileLocation
 * @retval NULL Stack is empty
 */
struct FileLocation *parse_context_current(struct ParseContext *pctx)
{
  if (!pctx || ARRAY_EMPTY(&pctx->locations))
    return NULL;

  return ARRAY_LAST(&pctx->locations);
}

/**
 * parse_context_contains - Check if a filename is already in the stack
 * @param pctx     ParseContext
 * @param filename Filename to check
 * @retval true  Filename is in the stack (cyclic sourcing)
 * @retval false Filename is not in the stack
 */
bool parse_context_contains(struct ParseContext *pctx, const char *filename)
{
  if (!pctx || !filename)
    return false;

  struct FileLocation *fl = NULL;
  ARRAY_FOREACH(fl, &pctx->locations)
  {
    if (mutt_str_equal(fl->filename, filename))
      return true;
  }

  return false;
}

/**
 * parse_context_cwd - Get the current working directory from context
 * @param pctx ParseContext
 * @retval ptr Path of the current file being parsed, or NULL if stack is empty
 *
 * This returns the directory containing the current file, which can be
 * used for resolving relative paths in 'source' commands.
 */
const char *parse_context_cwd(struct ParseContext *pctx)
{
  struct FileLocation *fl = parse_context_current(pctx);
  if (fl && fl->filename)
    return fl->filename;
  return NULL;
}

/**
 * config_parse_error_init - Initialize a ConfigParseError
 * @param err ConfigParseError to initialize
 */
void config_parse_error_init(struct ConfigParseError *err)
{
  if (!err)
    return;

  buf_init(&err->message);
  err->filename = NULL;
  err->lineno = 0;
  err->origin = CO_CONFIG_FILE;
  err->result = MUTT_CMD_SUCCESS;
}

/**
 * config_parse_error_free - Free a ConfigParseError's contents
 * @param err ConfigParseError to free
 */
void config_parse_error_free(struct ConfigParseError *err)
{
  if (!err)
    return;

  buf_dealloc(&err->message);
  FREE(&err->filename);
  err->lineno = 0;
  err->origin = CO_CONFIG_FILE;
  err->result = MUTT_CMD_SUCCESS;
}

/**
 * config_parse_error_set - Set error information
 * @param err      ConfigParseError to set
 * @param result   Error result code
 * @param filename Filename where error occurred (may be NULL)
 * @param lineno   Line number where error occurred (0 if N/A)
 * @param fmt      Printf-style format string
 * @param ...      Variable arguments
 */
void config_parse_error_set(struct ConfigParseError *err, enum CommandResult result,
                            const char *filename, int lineno, const char *fmt, ...)
{
  if (!err)
    return;

  err->result = result;
  mutt_str_replace(&err->filename, filename);
  err->lineno = lineno;

  if (fmt)
  {
    va_list ap;
    va_start(ap, fmt);
    buf_reset(&err->message);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    buf_strcpy(&err->message, buf);
    va_end(ap);
  }
}
