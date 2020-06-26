/**
 * @file
 * General purpose object for storing and parsing strings
 *
 * @authors
 * Copyright (C) 2017 Ian Zimmerman <itz@primate.net>
 * Copyright (C) 2017-2019 Richard Russon <rich@flatcap.org>
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
 * @page buffer General purpose object for storing and parsing strings
 *
 * The Buffer object make parsing and manipulating strings easier.
 */

#include "config.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "buffer.h"
#include "memory.h"
#include "string2.h"

/**
 * mutt_buffer_init - Initialise a new Buffer
 * @param buf Buffer to initialise
 * @retval ptr Initialised Buffer
 *
 * This must not be called on a Buffer that already contains data.
 */
struct Buffer *mutt_buffer_init(struct Buffer *buf)
{
  if (!buf)
    return NULL;
  memset(buf, 0, sizeof(struct Buffer));
  return buf;
}

/**
 * mutt_buffer_make - Make a new buffer on the stack
 * @param size Initial size
 * @retval buf Initialized buffer
 *
 * The buffer must be released using mutt_buffer_dealloc
 */
struct Buffer mutt_buffer_make(size_t size)
{
  struct Buffer buf = { 0 };
  if (size != 0)
  {
    buf.dptr = buf.data = mutt_mem_calloc(1, size);
    buf.dsize = size;
  }
  return buf;
}

/**
 * mutt_buffer_reset - Reset an existing Buffer
 * @param buf Buffer to reset
 *
 * This can be called on a Buffer to reset the pointers,
 * effectively emptying it.
 */
void mutt_buffer_reset(struct Buffer *buf)
{
  if (!buf || !buf->data || (buf->dsize == 0))
    return;
  memset(buf->data, 0, buf->dsize);
  buf->dptr = buf->data;
}

/**
 * mutt_buffer_addstr_n - Add a string to a Buffer, expanding it if necessary
 * @param buf Buffer to add to
 * @param s   String to add
 * @param len Length of the string
 * @retval num Bytes written to Buffer
 * @retval 0   Error
 *
 * Dynamically grow a Buffer to accommodate s, in increments of 128 bytes.
 * Always one byte bigger than necessary for the null terminator, and the
 * buffer is always NUL-terminated
 */
size_t mutt_buffer_addstr_n(struct Buffer *buf, const char *s, size_t len)
{
  if (!buf || !s)
    return 0;

  if (!buf->data || !buf->dptr || ((buf->dptr + len + 1) > (buf->data + buf->dsize)))
    mutt_buffer_alloc(buf, buf->dsize + MAX(128, len + 1));

  memcpy(buf->dptr, s, len);
  buf->dptr += len;
  *(buf->dptr) = '\0';
  return len;
}

/**
 * buffer_printf - Format a string into a Buffer
 * @param buf Buffer
 * @param fmt printf-style format string
 * @param ap  Arguments to be formatted
 * @retval num Characters written
 * @retval 0   Error
 */
static int buffer_printf(struct Buffer *buf, const char *fmt, va_list ap)
{
  if (!buf || !fmt)
    return 0; /* LCOV_EXCL_LINE */

  if (!buf->data || !buf->dptr || (buf->dsize < 128))
    mutt_buffer_alloc(buf, 128);

  int doff = buf->dptr - buf->data;
  int blen = buf->dsize - doff;

  va_list ap_retry;
  va_copy(ap_retry, ap);

  int len = vsnprintf(buf->dptr, blen, fmt, ap);
  if (len >= blen)
  {
    blen = ++len - blen;
    if (blen < 128)
      blen = 128;
    mutt_buffer_alloc(buf, buf->dsize + blen);
    len = vsnprintf(buf->dptr, len, fmt, ap_retry);
  }
  if (len > 0)
    buf->dptr += len;

  va_end(ap_retry);

  return len;
}

/**
 * mutt_buffer_printf - Format a string overwriting a Buffer
 * @param buf Buffer
 * @param fmt printf-style format string
 * @param ... Arguments to be formatted
 * @retval num Characters written
 * @retval -1  Error
 */
int mutt_buffer_printf(struct Buffer *buf, const char *fmt, ...)
{
  if (!buf || !fmt)
    return -1;

  va_list ap;

  va_start(ap, fmt);
  mutt_buffer_reset(buf);
  int len = buffer_printf(buf, fmt, ap);
  va_end(ap);

  return len;
}

/**
 * mutt_buffer_fix_dptr - Move the dptr to end of the Buffer
 * @param buf Buffer to alter
 *
 * Ensure buffer->dptr points to the end of the buffer.
 */
void mutt_buffer_fix_dptr(struct Buffer *buf)
{
  if (!buf)
    return;

  buf->dptr = buf->data;

  if (buf->data && (buf->dsize > 0))
  {
    buf->data[buf->dsize - 1] = '\0';
    buf->dptr = strchr(buf->data, '\0');
  }
}

/**
 * mutt_buffer_add_printf - Format a string appending a Buffer
 * @param buf Buffer
 * @param fmt printf-style format string
 * @param ... Arguments to be formatted
 * @retval num Characters written
 * @retval -1  Error
 */
int mutt_buffer_add_printf(struct Buffer *buf, const char *fmt, ...)
{
  if (!buf || !fmt)
    return -1;

  va_list ap;

  va_start(ap, fmt);
  int len = buffer_printf(buf, fmt, ap);
  va_end(ap);

  return len;
}

/**
 * mutt_buffer_addstr - Add a string to a Buffer
 * @param buf Buffer to add to
 * @param s   String to add
 * @retval num Bytes written to Buffer
 *
 * If necessary, the Buffer will be expanded.
 */
size_t mutt_buffer_addstr(struct Buffer *buf, const char *s)
{
  if (!buf || !s)
    return 0;
  return mutt_buffer_addstr_n(buf, s, mutt_str_len(s));
}

/**
 * mutt_buffer_addch - Add a single character to a Buffer
 * @param buf Buffer to add to
 * @param c   Character to add
 * @retval num Bytes written to Buffer
 *
 * If necessary, the Buffer will be expanded.
 */
size_t mutt_buffer_addch(struct Buffer *buf, char c)
{
  if (!buf)
    return 0;
  return mutt_buffer_addstr_n(buf, &c, 1);
}

/**
 * mutt_buffer_is_empty - Is the Buffer empty?
 * @param buf Buffer to inspect
 * @retval true Buffer is empty
 */
bool mutt_buffer_is_empty(const struct Buffer *buf)
{
  if (!buf || !buf->data)
    return true;

  return (buf->data[0] == '\0');
}

/**
 * mutt_buffer_alloc - Make sure a buffer can store at least new_size bytes
 * @param buf      Buffer to change
 * @param new_size New size
 */
void mutt_buffer_alloc(struct Buffer *buf, size_t new_size)
{
  if (!buf)
  {
    return;
  }

  if (!buf->dptr)
  {
    buf->dptr = buf->data;
  }

  if ((new_size > buf->dsize) || !buf->data)
  {
    size_t offset = (buf->dptr && buf->data) ? buf->dptr - buf->data : 0;

    buf->dsize = new_size;
    mutt_mem_realloc(&buf->data, buf->dsize);
    buf->dptr = buf->data + offset;
    /* This ensures an initially NULL buf->data is now properly terminated. */
    if (buf->dptr)
      *buf->dptr = '\0';
  }
}

/**
 * mutt_buffer_dealloc - Release the memory allocated by a buffer
 * @param buf Buffer to change
 */
void mutt_buffer_dealloc(struct Buffer *buf)
{
  if (!buf || !buf->data)
    return;

  buf->dptr = NULL;
  buf->dsize = 0;
  FREE(&buf->data);
}

/**
 * mutt_buffer_strcpy - Copy a string into a Buffer
 * @param buf Buffer to overwrite
 * @param s   String to copy
 * @retval num Bytes written to Buffer
 *
 * Overwrites any existing content.
 */
size_t mutt_buffer_strcpy(struct Buffer *buf, const char *s)
{
  mutt_buffer_reset(buf);
  return mutt_buffer_addstr(buf, s);
}

/**
 * mutt_buffer_strcpy_n - Copy a string into a Buffer
 * @param buf Buffer to overwrite
 * @param s   String to copy
 * @param len Length of string to copy
 * @retval num Bytes written to Buffer
 *
 * Overwrites any existing content.
 */
size_t mutt_buffer_strcpy_n(struct Buffer *buf, const char *s, size_t len)
{
  mutt_buffer_reset(buf);
  return mutt_buffer_addstr_n(buf, s, len);
}

/**
 * mutt_buffer_substrcpy - Copy a partial string into a Buffer
 * @param buf Buffer to overwrite
 * @param beg Start of string to copy
 * @param end End of string to copy
 * @retval num Bytes written to Buffer
 *
 * Overwrites any existing content.
 */
size_t mutt_buffer_substrcpy(struct Buffer *buf, const char *beg, const char *end)
{
  mutt_buffer_reset(buf);
  if (end <= beg)
    return 0;

  return mutt_buffer_strcpy_n(buf, beg, end - beg);
}

/**
 * mutt_buffer_len - Calculate the length of a Buffer
 * @param buf Buffer
 * @retval num Size of buffer
 */
size_t mutt_buffer_len(const struct Buffer *buf)
{
  if (!buf || !buf->data || !buf->dptr)
    return 0;

  return buf->dptr - buf->data;
}

/**
 * mutt_buffer_concat_path - Join a directory name and a filename
 * @param buf   Buffer to add to
 * @param dir   Directory name
 * @param fname File name
 * @retval num Bytes written to Buffer
 *
 * If both dir and fname are supplied, they are separated with '/'.
 * If either is missing, then the other will be copied exactly.
 */
size_t mutt_buffer_concat_path(struct Buffer *buf, const char *dir, const char *fname)
{
  if (!buf)
    return 0;

  if (!dir)
    dir = "";
  if (!fname)
    fname = "";

  const bool d_set = (dir[0] != '\0');
  const bool f_set = (fname[0] != '\0');
  if (!d_set && !f_set)
    return 0;

  const int d_len = strlen(dir);
  const bool slash = d_set && (dir[d_len - 1] == '/');

  const char *fmt = "%s/%s";
  if (!f_set || !d_set || slash)
    fmt = "%s%s";

  return mutt_buffer_printf(buf, fmt, dir, fname);
}

/**
 * mutt_buffer_concatn_path - Join a directory name and a filename
 * @param buf      Buffer for the result
 * @param dir      Directory name
 * @param dirlen   Directory name
 * @param fname    File name
 * @param fnamelen File name
 * @retval num Size of buffer
 *
 * If both dir and fname are supplied, they are separated with '/'.
 * If either is missing, then the other will be copied exactly.
 */
size_t mutt_buffer_concatn_path(struct Buffer *buf, const char *dir,
                                size_t dirlen, const char *fname, size_t fnamelen)
{
  size_t len = 0;
  mutt_buffer_reset(buf);
  if (dirlen != 0)
    len += mutt_buffer_addstr_n(buf, dir, dirlen);
  if ((dirlen != 0) && (fnamelen != 0))
    len += mutt_buffer_addch(buf, '/');
  if (fnamelen != 0)
    len += mutt_buffer_addstr_n(buf, fname, fnamelen);
  return len;
}

/**
 * mutt_buffer_strdup - Copy a Buffer's string
 * @param buf Buffer to copy
 * @retval ptr Copy of string
 *
 * @note Caller must free the returned string
 */
char *mutt_buffer_strdup(struct Buffer *buf)
{
  if (!buf)
    return NULL;

  return mutt_str_dup(buf->data);
}

/**
 * mutt_buffer_copy - Copy a Buffer's contents to another Buffer
 * @param dst Buffer for result
 * @param src Buffer to copy
 */
size_t mutt_buffer_copy(struct Buffer *dst, const struct Buffer *src)
{
  if (!dst)
    return 0;

  mutt_buffer_reset(dst);
  if (!src || !src->data)
    return 0;

  return mutt_buffer_addstr_n(dst, src->data, mutt_buffer_len(src));
}
