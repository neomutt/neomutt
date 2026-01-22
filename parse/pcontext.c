/**
 * @file
 * Parse Context
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
 * @page parse_context Parse Context
 *
 * Parse Context
 */

#include "config.h"
#include <stdio.h>
#include "mutt/lib.h"
#include "pcontext.h"
#include "fileloc.h"

/**
 * parse_context_init - Initialize a ParseContext
 * @param pc   ParseContext to initialize
 * @param origin Origin of the command
 */
void parse_context_init(struct ParseContext *pc, enum CommandOrigin origin)
{
  if (!pc)
    return;

  ARRAY_INIT(&pc->locations);
  pc->origin = origin;
  pc->hook_id = CMD_NONE;
}

/**
 * parse_context_free - Free a ParseContext
 * @param pptr ParseContext to free
 */
void parse_context_free(struct ParseContext **pptr)
{
  if (!pptr || !*pptr)
    return;

  struct ParseContext *pc = *pptr;

  struct FileLocation *fl = NULL;
  ARRAY_FOREACH(fl, &pc->locations)
  {
    file_location_clear(fl);
  }
  ARRAY_FREE(&pc->locations);
  pc->origin = CO_CONFIG_FILE;
  pc->hook_id = CMD_NONE;

  FREE(pptr);
}

/**
 * parse_context_push - Push a file location onto the context stack
 * @param pc     ParseContext
 * @param filename Path to the file
 * @param lineno   Line number (1-based)
 */
void parse_context_push(struct ParseContext *pc, const char *filename, int lineno)
{
  if (!pc)
    return;

  struct FileLocation fl = { 0 };
  file_location_init(&fl, filename, lineno);
  ARRAY_ADD(&pc->locations, fl);
}

/**
 * parse_context_pop - Pop a file location from the context stack
 * @param pc ParseContext
 */
void parse_context_pop(struct ParseContext *pc)
{
  if (!pc || ARRAY_EMPTY(&pc->locations))
    return;

  struct FileLocation *fl = ARRAY_LAST(&pc->locations);
  if (fl)
    file_location_clear(fl);
  ARRAY_SHRINK(&pc->locations, 1);
}

/**
 * parse_context_current - Get the current (top) file location
 * @param pc ParseContext
 * @retval ptr  Current FileLocation
 * @retval NULL Stack is empty
 */
struct FileLocation *parse_context_current(struct ParseContext *pc)
{
  if (!pc || ARRAY_EMPTY(&pc->locations))
    return NULL;

  return ARRAY_LAST(&pc->locations);
}

/**
 * parse_context_contains - Check if a filename is already in the stack
 * @param pc     ParseContext
 * @param filename Filename to check
 * @retval true  Filename is in the stack (cyclic sourcing)
 * @retval false Filename is not in the stack
 */
bool parse_context_contains(struct ParseContext *pc, const char *filename)
{
  if (!pc || !filename)
    return false;

  struct FileLocation *fl = NULL;
  ARRAY_FOREACH(fl, &pc->locations)
  {
    if (mutt_str_equal(fl->filename, filename))
      return true;
  }

  return false;
}

/**
 * parse_context_cwd - Get the current working directory from context
 * @param pc ParseContext
 * @retval ptr Path of the current file being parsed, or NULL if stack is empty
 *
 * This returns the directory containing the current file, which can be
 * used for resolving relative paths in 'source' commands.
 */
const char *parse_context_cwd(struct ParseContext *pc)
{
  struct FileLocation *fl = parse_context_current(pc);
  if (fl && fl->filename)
    return fl->filename;
  return NULL;
}

/**
 * parse_context_new - Create a new ParseContext
 * @retval ptr New ParseContext
 */
struct ParseContext *parse_context_new(void)
{
  struct ParseContext *pc = MUTT_MEM_CALLOC(1, struct ParseContext);

  return pc;
}
