/**
 * @file
 * General purpose object for storing and parsing strings
 *
 * @authors
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

#ifndef _MUTT_BUFFER_H
#define _MUTT_BUFFER_H

#include <stddef.h>

/**
 * struct Buffer - String manipulation buffer
 */
struct Buffer
{
  char *data;   /**< pointer to data */
  char *dptr;   /**< current read/write position */
  size_t dsize; /**< length of data */
  int destroy;  /**< destroy `data' when done? */
};

/* flags for mutt_extract_token() */
#define MUTT_TOKEN_EQUAL     (1 << 0) /**< treat '=' as a special */
#define MUTT_TOKEN_CONDENSE  (1 << 1) /**< ^(char) to control chars (macros) */
#define MUTT_TOKEN_SPACE     (1 << 2) /**< don't treat whitespace as a term */
#define MUTT_TOKEN_QUOTE     (1 << 3) /**< don't interpret quotes */
#define MUTT_TOKEN_PATTERN   (1 << 4) /**< !)|~ are terms (for patterns) */
#define MUTT_TOKEN_COMMENT   (1 << 5) /**< don't reap comments */
#define MUTT_TOKEN_SEMICOLON (1 << 6) /**< don't treat ; as special */

struct Buffer *mutt_buffer_new(void);
struct Buffer *mutt_buffer_init(struct Buffer *b);
struct Buffer *mutt_buffer_from(char *seed);
void mutt_buffer_free(struct Buffer **p);
int mutt_buffer_printf(struct Buffer *buf, const char *fmt, ...);
void mutt_buffer_addstr(struct Buffer *buf, const char *s);
void mutt_buffer_addch(struct Buffer *buf, char c);
int mutt_extract_token(struct Buffer *dest, struct Buffer *tok, int flags);

#endif /* _MUTT_BUFFER_H */
