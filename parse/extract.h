/**
 * @file
 * Text parser
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Rayford Shireman
 * Copyright (C) 2025 Thomas Klausner <wiz@gatalith.at>
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

#ifndef MUTT_PARSE_EXTRACT_H
#define MUTT_PARSE_EXTRACT_H

#include <stdint.h>
#include "mutt/lib.h" // IWYU pragma: keep

#define MoreArgs(buf) (*(buf)->dptr && (*(buf)->dptr != ';') && (*(buf)->dptr != '#'))

/* The same conditions as in mutt_extract_token() */
#define MoreArgsF(buf, flags) (*(buf)->dptr && \
    (!mutt_isspace(*(buf)->dptr) || ((flags) & TOKEN_SPACE)) && \
    ((*(buf)->dptr != '#') ||  ((flags) & TOKEN_COMMENT)) && \
    ((*(buf)->dptr != '+') || !((flags) & TOKEN_PLUS)) && \
    ((*(buf)->dptr != '-') || !((flags) & TOKEN_MINUS)) && \
    ((*(buf)->dptr != '=') || !((flags) & TOKEN_EQUAL)) && \
    ((*(buf)->dptr != '?') || !((flags) & TOKEN_QUESTION)) && \
    ((*(buf)->dptr != ';') || ((flags) & TOKEN_SEMICOLON)) && \
    (!((flags) & TOKEN_PATTERN) || strchr("~%=!|", *(buf)->dptr)))

/**
 * enum TokenFlag - Flags for parse_extract_token(), e.g. #TOKEN_EQUAL
 */
enum TokenFlag
{
  TOKEN_NONE          =        0,  ///< No flags are set
  TOKEN_EQUAL         = 1U <<  0,  ///< Treat '=' as a special
  TOKEN_CONDENSE      = 1U <<  1,  ///< ^(char) to control chars (macros)
  TOKEN_SPACE         = 1U <<  2,  ///< Don't treat whitespace as a term
  TOKEN_QUOTE         = 1U <<  3,  ///< Don't interpret quotes
  TOKEN_PATTERN       = 1U <<  4,  ///< ~%=!| are terms (for patterns)
  TOKEN_COMMENT       = 1U <<  5,  ///< Don't reap comments
  TOKEN_SEMICOLON     = 1U <<  6,  ///< Don't treat ; as special
  TOKEN_NOSHELL       = 1U <<  7,  ///< Don't expand environment variables
  TOKEN_QUESTION      = 1U <<  8,  ///< Treat '?' as a special
  TOKEN_PLUS          = 1U <<  9,  ///< Treat '+' as a special
  TOKEN_MINUS         = 1U << 10,  ///< Treat '-' as a special
};
typedef uint16_t TokenFlags;

int parse_extract_token(struct Buffer *dest, struct Buffer *line, TokenFlags flags);

#endif /* MUTT_PARSE_EXTRACT_H */
