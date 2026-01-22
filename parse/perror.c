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
#include "mutt/lib.h"
#include "perror.h"

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
