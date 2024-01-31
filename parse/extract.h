/**
 * @file
 * Text parser
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Rayford Shireman
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

#include <ctype.h>
#include <stdint.h>

struct Buffer;

#define MoreArgs(buf) (*(buf)->dptr && (*(buf)->dptr != ';') && (*(buf)->dptr != '#'))

typedef uint16_t TokenFlags;          ///< Flags for parse_extract_token(), e.g. #TOKEN_EQUAL
#define TOKEN_NO_FLAGS            0   ///< No flags are set
#define TOKEN_EQUAL         (1 << 0)  ///< Treat '=' as a special
#define TOKEN_CONDENSE      (1 << 1)  ///< ^(char) to control chars (macros)
#define TOKEN_SPACE         (1 << 2)  ///< Don't treat whitespace as a term
#define TOKEN_QUOTE         (1 << 3)  ///< Don't interpret quotes
#define TOKEN_PATTERN       (1 << 4)  ///< ~%=!| are terms (for patterns)
#define TOKEN_COMMENT       (1 << 5)  ///< Don't reap comments
#define TOKEN_SEMICOLON     (1 << 6)  ///< Don't treat ; as special
#define TOKEN_BACKTICK_VARS (1 << 7)  ///< Expand variables within backticks
#define TOKEN_NOSHELL       (1 << 8)  ///< Don't expand environment variables
#define TOKEN_QUESTION      (1 << 9)  ///< Treat '?' as a special
#define TOKEN_PLUS          (1 << 10) ///< Treat '+' as a special
#define TOKEN_MINUS         (1 << 11) ///< Treat '-' as a special

int parse_extract_token(struct Buffer *dest, struct Buffer *tok, TokenFlags flags);

#endif /* MUTT_PARSE_EXTRACT_H */
