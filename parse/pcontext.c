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
#include "mutt/lib.h"
#include "pcontext.h"

/**
 * parse_context_new - Create a new ParseContext
 * @retval ptr New ParseContext
 */
struct ParseContext *parse_context_new(void)
{
  struct ParseContext *pc = MUTT_MEM_CALLOC(1, struct ParseContext);

  return pc;
}

/**
 * parse_context_free - Free a ParseContext
 * @param pptr ParseContext to free
 */
void parse_context_free(struct ParseContext **pptr)
{
  if (!pptr || !*pptr)
    return;

  FREE(pptr);
}
