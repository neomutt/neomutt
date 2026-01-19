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

#ifndef MUTT_PARSE_PCONTEXT_H
#define MUTT_PARSE_PCONTEXT_H

#include "config.h"
#include "mutt/lib.h"
#include "core/lib.h"

/**
 * struct FileLocation - Represents one config file being processed
 *
 * This structure tracks a single file location during config parsing,
 * containing the filename and the current line number being processed.
 */
struct FileLocation
{
  char *filename;  ///< Full path to the config file
  int lineno;      ///< Line number being processed (1-based)
};
ARRAY_HEAD(FileLocationArray, struct FileLocation);

/**
 * enum CommandOrigin - Origin of a config command
 *
 * Identifies where a configuration command originated from.
 */
enum CommandOrigin
{
  CO_CONFIG_FILE = 0, ///< Command from a config file
  CO_USER,            ///< User manually entered the command
  CO_HOOK,            ///< Hook triggered by an event
  CO_LUA,             ///< Lua script executing the command
};

/**
 * struct ParseContext - Context for config parsing (history/backtrace)
 *
 * This structure maintains the history of nested files being processed,
 * allowing proper error reporting with full file location backtrace
 * and detection of infinite loops in config file sourcing.
 */
struct ParseContext
{
  struct FileLocationArray locations; ///< LIFO stack of file locations
  enum CommandOrigin origin;          ///< Origin of the command
  enum CommandId hook_id;             ///< Hook ID if origin is CO_HOOK
};

/**
 * struct ConfigParseError - Detailed error information from config parsing
 *
 * This structure provides comprehensive error information when
 * a config parsing error occurs, including the error message,
 * location information, and the result code.
 */
struct ConfigParseError
{
  struct Buffer message;        ///< Error message
  char *filename;               ///< File where error occurred (may be NULL)
  int lineno;                   ///< Line number where error occurred (0 if N/A)
  enum CommandOrigin origin;    ///< Origin of the command
  enum CommandResult result;    ///< Error code result
};

// FileLocation functions
void                  file_location_init  (struct FileLocation *fl, const char *filename, int lineno);
void                  file_location_free  (struct FileLocation *fl);

// ParseContext functions
void                  parse_context_init    (struct ParseContext *pctx, enum CommandOrigin origin);
void                  parse_context_free    (struct ParseContext *pctx);
void                  parse_context_push    (struct ParseContext *pctx, const char *filename, int lineno);
void                  parse_context_pop     (struct ParseContext *pctx);
struct FileLocation  *parse_context_current (struct ParseContext *pctx);
bool                  parse_context_contains(struct ParseContext *pctx, const char *filename);
const char           *parse_context_cwd     (struct ParseContext *pctx);

// ConfigParseError functions
void                  config_parse_error_init(struct ConfigParseError *err);
void                  config_parse_error_free(struct ConfigParseError *err);
void                  config_parse_error_set (struct ConfigParseError *err, enum CommandResult result,
                                              const char *filename, int lineno, const char *fmt, ...)
                                              __attribute__((__format__(__printf__, 5, 6)));

#endif /* MUTT_PARSE_PCONTEXT_H */
