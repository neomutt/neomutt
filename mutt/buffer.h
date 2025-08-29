/**
 * @file
 * General purpose object for storing and parsing strings
 *
 * @authors
 * Copyright (C) 2017 Ian Zimmerman <itz@primate.net>
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Anna Figueiredo Gomes <navi@vlhl.dev>
 * Copyright (C) 2023 Simon Reichel <simonreichel@giese-optik.de>
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

#ifndef MUTT_MUTT_BUFFER_H
#define MUTT_MUTT_BUFFER_H

#include <stdbool.h>
#include <stddef.h>

/**
 * struct Buffer - String manipulation buffer
 */
struct Buffer
{
  char *data;   ///< Pointer to data
  char *dptr;   ///< Current read/write position
  size_t dsize; ///< Length of data
};

struct Buffer *buf_new             (const char *str);
void           buf_free            (struct Buffer **ptr);
void           buf_alloc           (struct Buffer *buf, size_t size);
void           buf_dealloc         (struct Buffer *buf);
void           buf_fix_dptr        (struct Buffer *buf);
struct Buffer *buf_init            (struct Buffer *buf);
bool           buf_is_empty        (const struct Buffer *buf);
size_t         buf_len             (const struct Buffer *buf);
void           buf_reset           (struct Buffer *buf);
char *         buf_strdup          (const struct Buffer *buf);
struct Buffer *buf_dup             (const struct Buffer *buf);
void           buf_seek            (struct Buffer *buf, size_t offset);
const char*    buf_find_string     (const struct Buffer *buf, const char *s);
const char*    buf_find_char       (const struct Buffer *buf, const char c);
char           buf_at              (const struct Buffer *buf, size_t offset);
bool           buf_str_equal       (const struct Buffer *a, const struct Buffer *b);
bool           buf_istr_equal      (const struct Buffer *a, const struct Buffer *b);
int            buf_coll            (const struct Buffer *a, const struct Buffer *b);
size_t         buf_startswith      (const struct Buffer *buf, const char *prefix);
const char    *buf_rfind           (const struct Buffer *buf, const char *str);

// Functions that APPEND to a Buffer
size_t         buf_addch           (struct Buffer *buf, char c);
size_t         buf_addstr          (struct Buffer *buf, const char *s);
size_t         buf_addstr_n        (struct Buffer *buf, const char *s, size_t len);
int            buf_add_printf      (struct Buffer *buf, const char *fmt, ...)
                                    __attribute__((__format__(__printf__, 2, 3)));
void           buf_join_str        (struct Buffer *str, const char *item, char sep);

// Functions that INSERT into a Buffer
size_t         buf_insert          (struct Buffer *buf, size_t offset, const char *s);

// Functions that OVERWRITE a Buffer
size_t         buf_concat_path     (struct Buffer *buf, const char *dir, const char *fname);
size_t         buf_concatn_path    (struct Buffer *dst, const char *dir, size_t dirlen, const char *fname, size_t fnamelen);
size_t         buf_copy            (struct Buffer *dst, const struct Buffer *src);
int            buf_printf          (struct Buffer *buf, const char *fmt, ...)
                                    __attribute__((__format__(__printf__, 2, 3)));
size_t         buf_strcpy          (struct Buffer *buf, const char *s);
size_t         buf_strcpy_n        (struct Buffer *buf, const char *s, size_t len);
size_t         buf_substrcpy       (struct Buffer *buf, const char *beg, const char *end);
void           buf_dequote_comment (struct Buffer *buf);
void           buf_lower           (struct Buffer *buf);
void           buf_inline_replace  (struct Buffer *buf, size_t pos, size_t len, const char *str);

/**
 * buf_string - Convert a buffer to a const char * "string"
 * @param buf Buffer to that is to be converted
 * @retval ptr String inside the Buffer
 *
 * This method exposes Buffer's underlying data field
 *
 * @note Returns an empty string if Buffer isn't initialised
 */
static inline const char *buf_string(const struct Buffer *buf)
{
  if (!buf || !buf->data)
    return "";

  return buf->data;
}

#endif /* MUTT_MUTT_BUFFER_H */
