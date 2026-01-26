/**
 * @file
 * Parse Errors
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

#ifndef MUTT_PARSE_PERROR_H
#define MUTT_PARSE_PERROR_H

/**
 * struct ParseError - Detailed error information from config parsing
 *
 * This structure provides comprehensive error information when
 * a config parsing error occurs, including the error message,
 * location information, and the result code.
 */
struct ParseError
{
  struct Buffer *message;        ///< Error message
};

struct ParseError *parse_error_new(void);
void               parse_error_free(struct ParseError **pptr);

void parse_error_reset(struct ParseError *pe);

#endif /* MUTT_PARSE_PERROR_H */
