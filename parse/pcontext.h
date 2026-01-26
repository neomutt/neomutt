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

#ifndef MUTT_PARSE_PCONTEXT_H
#define MUTT_PARSE_PCONTEXT_H

/**
 * struct ParseContext - Context for config parsing (history/backtrace)
 *
 * This structure maintains the history of nested files being processed,
 * allowing proper error reporting with full file location backtrace
 * and detection of infinite loops in config file sourcing.
 */
struct ParseContext
{
  int dummy;
};

struct ParseContext *parse_context_new(void);
void                 parse_context_free(struct ParseContext **pptr);

#endif /* MUTT_PARSE_PCONTEXT_H */
