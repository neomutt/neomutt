/**
 * @file
 * General purpose object for storing and parsing strings
 *
 * @authors
 * Copyright (C) 2017 Ian Zimmerman <itz@primate.net>
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef _MUTT_BUFFER_H
#define _MUTT_BUFFER_H

#include <stddef.h>
#include <stdbool.h>

/**
 * struct Buffer - String manipulation buffer
 */
struct Buffer
{
  char *data;   /**< pointer to data */
  char *dptr;   /**< current read/write position */
  size_t dsize; /**< length of data */
  int destroy;  /**< destroy 'data' when done? */
};

#define MoreArgs(p) (*p->dptr && (*p->dptr != ';') && (*p->dptr != '#'))

size_t         mutt_buffer_add(struct Buffer *buf, const char *s, size_t len);
size_t         mutt_buffer_addch(struct Buffer *buf, char c);
size_t         mutt_buffer_addstr(struct Buffer *buf, const char *s);
struct Buffer *mutt_buffer_alloc(size_t size);
void           mutt_buffer_free(struct Buffer **p);
struct Buffer *mutt_buffer_from(char *seed);
struct Buffer *mutt_buffer_init(struct Buffer *b);
bool           mutt_buffer_is_empty(const struct Buffer *buf);
struct Buffer *mutt_buffer_new(void);
int            mutt_buffer_printf(struct Buffer *buf, const char *fmt, ...);
void           mutt_buffer_reset(struct Buffer *b);

#endif /* _MUTT_BUFFER_H */
