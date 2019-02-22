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

#ifndef MUTT_LIB_BUFFER_H
#define MUTT_LIB_BUFFER_H

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

/* Convert a buffer to a const char * "string" */
#define mutt_b2s(buf) (buf->data ? (const char *) buf->data : "")

#define MoreArgs(buf) (*buf->dptr && (*buf->dptr != ';') && (*buf->dptr != '#'))

size_t         mutt_buffer_addch        (struct Buffer *buf, char c);
size_t         mutt_buffer_addstr       (struct Buffer *buf, const char *s);
size_t         mutt_buffer_addstr_n     (struct Buffer *buf, const char *s, size_t len);
int            mutt_buffer_add_printf   (struct Buffer *buf, const char *fmt, ...);
struct Buffer *mutt_buffer_alloc        (size_t size);
void           mutt_buffer_fix_dptr     (struct Buffer *buf);
void           mutt_buffer_free         (struct Buffer **p);
struct Buffer *mutt_buffer_from         (const char *seed);
void           mutt_buffer_increase_size(struct Buffer *buf, size_t new_size);
struct Buffer *mutt_buffer_init         (struct Buffer *buf);
bool           mutt_buffer_is_empty     (const struct Buffer *buf);
size_t         mutt_buffer_len          (const struct Buffer *buf);
struct Buffer *mutt_buffer_new          (void);
int            mutt_buffer_printf       (struct Buffer *buf, const char *fmt, ...);
void           mutt_buffer_reset        (struct Buffer *buf);
void           mutt_buffer_strcpy       (struct Buffer *buf, const char *s);

void           mutt_buffer_pool_free    (void);
struct Buffer *mutt_buffer_pool_get     (void);
void           mutt_buffer_pool_init    (void);
void           mutt_buffer_pool_release (struct Buffer **pbuf);

#endif /* MUTT_LIB_BUFFER_H */
