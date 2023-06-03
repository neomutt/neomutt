/**
 * @file
 * General purpose object for storing and parsing strings
 *
 * @authors
 * Copyright (C) 2017 Ian Zimmerman <itz@primate.net>
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019-2023 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2023 Anna Figueiredo Gomes <navi@vlhl.dev>
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
 * @page mutt_buffer Helper object for storing and parsing strings
 *
 * The Buffer object make parsing and manipulating strings easier.
 */

#include "config.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "buffer.h"
#include "exit.h"
#include "logging2.h"
#include "memory.h"
#include "message.h"
#include "string2.h"

/// When increasing the size of a Buffer, add this much extra space
static const int BufferStepSize = 128;

/**
 * buf_init - Initialise a new Buffer
 * @param buf Buffer to initialise
 * @retval ptr Initialised Buffer
 *
 * This must not be called on a Buffer that already contains data.
 */
struct Buffer *buf_init(struct Buffer *buf)
{
  if (!buf)
    return NULL;
  memset(buf, 0, sizeof(struct Buffer));
  return buf;
}

/**
 * buf_make - Make a new buffer on the stack
 * @param size Initial size
 * @retval obj Initialized buffer
 *
 * The buffer must be released using buf_dealloc
 */
struct Buffer buf_make(size_t size)
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
 * buf_reset - Reset an existing Buffer
 * @param buf Buffer to reset
 *
 * This can be called on a Buffer to reset the pointers,
 * effectively emptying it.
 */
void buf_reset(struct Buffer *buf)
{
  if (!buf || !buf->data || (buf->dsize == 0))
    return;
  memset(buf->data, 0, buf->dsize);
  buf_seek(buf, 0);
}

/**
 * buf_addstr_n - Add a string to a Buffer, expanding it if necessary
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
size_t buf_addstr_n(struct Buffer *buf, const char *s, size_t len)
{
  if (!buf || !s)
    return 0;

  if (len > (SIZE_MAX - BufferStepSize))
  {
    // LCOV_EXCL_START
    mutt_error(_("Out of memory"));
    mutt_exit(1);
    // LCOV_EXCL_STOP
  }

  if (!buf->data || !buf->dptr || ((buf->dptr + len + 1) > (buf->data + buf->dsize)))
    buf_alloc(buf, buf->dsize + MAX(BufferStepSize, len + 1));

  memcpy(buf->dptr, s, len);
  buf->dptr += len;
  *(buf->dptr) = '\0';
  return len;
}

/**
 * buf_vaprintf - Format a string into a Buffer
 * @param buf Buffer
 * @param fmt printf-style format string
 * @param ap  Arguments to be formatted
 * @retval num Characters written
 * @retval 0   Error
 */
static int buf_vaprintf(struct Buffer *buf, const char *fmt, va_list ap)
{
  if (!buf || !fmt)
    return 0; /* LCOV_EXCL_LINE */

  buf_alloc(buf, 128);

  int doff = buf->dptr - buf->data;
  int blen = buf->dsize - doff;

  va_list ap_retry;
  va_copy(ap_retry, ap);

  int len = vsnprintf(buf->dptr, blen, fmt, ap);
  if (len >= blen)
  {
    buf_alloc(buf, buf->dsize + len - blen + 1);
    len = vsnprintf(buf->dptr, len + 1, fmt, ap_retry);
  }
  if (len > 0)
    buf->dptr += len;

  va_end(ap_retry);

  return len;
}

/**
 * buf_printf - Format a string overwriting a Buffer
 * @param buf Buffer
 * @param fmt printf-style format string
 * @param ... Arguments to be formatted
 * @retval num Characters written
 * @retval -1  Error
 */
int buf_printf(struct Buffer *buf, const char *fmt, ...)
{
  if (!buf || !fmt)
    return -1;

  va_list ap;

  va_start(ap, fmt);
  buf_reset(buf);
  int len = buf_vaprintf(buf, fmt, ap);
  va_end(ap);

  return len;
}

/**
 * buf_fix_dptr - Move the dptr to end of the Buffer
 * @param buf Buffer to alter
 *
 * Ensure buffer->dptr points to the end of the buffer.
 */
void buf_fix_dptr(struct Buffer *buf)
{
  if (!buf)
    return;

  buf_seek(buf, 0);

  if (buf->data && (buf->dsize > 0))
  {
    buf->data[buf->dsize - 1] = '\0';
    buf->dptr = strchr(buf->data, '\0');
  }
}

/**
 * buf_add_printf - Format a string appending a Buffer
 * @param buf Buffer
 * @param fmt printf-style format string
 * @param ... Arguments to be formatted
 * @retval num Characters written
 * @retval -1  Error
 */
int buf_add_printf(struct Buffer *buf, const char *fmt, ...)
{
  if (!buf || !fmt)
    return -1;

  va_list ap;

  va_start(ap, fmt);
  int len = buf_vaprintf(buf, fmt, ap);
  va_end(ap);

  return len;
}

/**
 * buf_addstr - Add a string to a Buffer
 * @param buf Buffer to add to
 * @param s   String to add
 * @retval num Bytes written to Buffer
 *
 * If necessary, the Buffer will be expanded.
 */
size_t buf_addstr(struct Buffer *buf, const char *s)
{
  if (!buf || !s)
    return 0;
  return buf_addstr_n(buf, s, mutt_str_len(s));
}

/**
 * buf_addch - Add a single character to a Buffer
 * @param buf Buffer to add to
 * @param c   Character to add
 * @retval num Bytes written to Buffer
 *
 * If necessary, the Buffer will be expanded.
 */
size_t buf_addch(struct Buffer *buf, char c)
{
  if (!buf)
    return 0;
  return buf_addstr_n(buf, &c, 1);
}

/**
 * buf_insert - Add a string in the middle of a buffer
 * @param buf    Buffer
 * @param offset Position for the insertion
 * @param s      String to insert
 * @retval num Characters written
 * @retval -1  Error
 */
size_t buf_insert(struct Buffer *buf, size_t offset, const char *s)
{
  if (!buf || !s || (*s == '\0'))
  {
    return -1;
  }

  const size_t slen = mutt_str_len(s);
  const size_t curlen = buf_len(buf);
  buf_alloc(buf, curlen + slen + 1);

  if (offset > curlen)
  {
    for (size_t i = curlen; i < offset; ++i)
    {
      buf_addch(buf, ' ');
    }
    buf_addstr(buf, s);
  }
  else
  {
    memmove(buf->data + offset + slen, buf->data + offset, curlen - offset);
    memcpy(buf->data + offset, s, slen);
    buf->data[curlen + slen] = '\0';
    buf->dptr = buf->data + curlen + slen;
  }

  return buf_len(buf) - curlen;
}

/**
 * buf_is_empty - Is the Buffer empty?
 * @param buf Buffer to inspect
 * @retval true Buffer is empty
 */
bool buf_is_empty(const struct Buffer *buf)
{
  if (!buf || !buf->data)
    return true;

  return (buf->data[0] == '\0');
}

/**
 * buf_new - Allocate a new Buffer
 * @param str String to initialise the buffer with, can be NULL
 * @retval ptr Pointer to new buffer
 */
struct Buffer *buf_new(const char *str)
{
  struct Buffer *buf = mutt_mem_calloc(1, sizeof(struct Buffer));

  if (str)
    buf_addstr(buf, str);
  return buf;
}

/**
 * buf_free - Deallocates a buffer
 * @param ptr Buffer to free
 */
void buf_free(struct Buffer **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Buffer *buf = *ptr;
  buf_dealloc(buf);

  FREE(ptr);
}

/**
 * buf_alloc - Make sure a buffer can store at least new_size bytes
 * @param buf      Buffer to change
 * @param new_size New size
 *
 * @note new_size will be rounded up to #BufferStepSize
 */
void buf_alloc(struct Buffer *buf, size_t new_size)
{
  if (!buf)
    return;

  if (buf->data && (new_size <= buf->dsize))
    return;

  if (new_size > (SIZE_MAX - BufferStepSize))
  {
    // LCOV_EXCL_START
    mutt_error(_("Out of memory"));
    mutt_exit(1);
    // LCOV_EXCL_STOP
  }

  const bool was_empty = (buf->dptr == NULL);
  const size_t offset = (buf->dptr && buf->data) ? (buf->dptr - buf->data) : 0;

  buf->dsize = ROUND_UP(new_size + 1, BufferStepSize);

  mutt_mem_realloc(&buf->data, buf->dsize);
  buf_seek(buf, offset);

  // Ensures that initially NULL buf->data is properly terminated
  if (was_empty)
  {
    buf->dptr = buf->data;
    *buf->dptr = '\0';
  }
}

/**
 * buf_dealloc - Release the memory allocated by a buffer
 * @param buf Buffer to change
 */
void buf_dealloc(struct Buffer *buf)
{
  if (!buf || !buf->data)
    return;

  buf->dptr = NULL;
  buf->dsize = 0;
  FREE(&buf->data);
}

/**
 * buf_strcpy - Copy a string into a Buffer
 * @param buf Buffer to overwrite
 * @param s   String to copy
 * @retval num Bytes written to Buffer
 *
 * Overwrites any existing content.
 */
size_t buf_strcpy(struct Buffer *buf, const char *s)
{
  buf_reset(buf);
  return buf_addstr(buf, s);
}

/**
 * buf_strcpy_n - Copy a string into a Buffer
 * @param buf Buffer to overwrite
 * @param s   String to copy
 * @param len Length of string to copy
 * @retval num Bytes written to Buffer
 *
 * Overwrites any existing content.
 */
size_t buf_strcpy_n(struct Buffer *buf, const char *s, size_t len)
{
  buf_reset(buf);
  return buf_addstr_n(buf, s, len);
}

/**
 * buf_dequote_comment - Un-escape characters in an email address comment
 * @param buf Buffer to be un-escaped
 *
 * @note the buffer is modified
 */
void buf_dequote_comment(struct Buffer *buf)
{
  if (!buf)
    return;

  char *tmp = buf_strdup(buf);

  mutt_str_dequote_comment(tmp);

  buf_strcpy(buf, tmp);
  FREE(&tmp);
}

/**
 * buf_substrcpy - Copy a partial string into a Buffer
 * @param buf Buffer to overwrite
 * @param beg Start of string to copy
 * @param end End of string to copy
 * @retval num Bytes written to Buffer
 *
 * Overwrites any existing content.
 */
size_t buf_substrcpy(struct Buffer *buf, const char *beg, const char *end)
{
  buf_reset(buf);
  if (end <= beg)
    return 0;

  return buf_strcpy_n(buf, beg, end - beg);
}

/**
 * buf_len - Calculate the length of a Buffer
 * @param buf Buffer
 * @retval num Size of buffer
 */
size_t buf_len(const struct Buffer *buf)
{
  if (!buf || !buf->data || !buf->dptr)
    return 0;

  return buf->dptr - buf->data;
}

/**
 * buf_concat_path - Join a directory name and a filename
 * @param buf   Buffer to add to
 * @param dir   Directory name
 * @param fname File name
 * @retval num Bytes written to Buffer
 *
 * If both dir and fname are supplied, they are separated with '/'.
 * If either is missing, then the other will be copied exactly.
 */
size_t buf_concat_path(struct Buffer *buf, const char *dir, const char *fname)
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

  return buf_printf(buf, fmt, dir, fname);
}

/**
 * buf_concatn_path - Join a directory name and a filename
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
size_t buf_concatn_path(struct Buffer *buf, const char *dir, size_t dirlen,
                        const char *fname, size_t fnamelen)
{
  size_t len = 0;
  buf_reset(buf);
  if (dirlen != 0)
    len += buf_addstr_n(buf, dir, dirlen);
  if ((dirlen != 0) && (fnamelen != 0))
    len += buf_addch(buf, '/');
  if (fnamelen != 0)
    len += buf_addstr_n(buf, fname, fnamelen);
  return len;
}

/**
 * buf_strdup - Copy a Buffer's string
 * @param buf Buffer to copy
 * @retval ptr Copy of string
 *
 * @note Caller must free the returned string
 */
char *buf_strdup(const struct Buffer *buf)
{
  if (!buf)
    return NULL;

  return mutt_str_dup(buf->data);
}

/**
 * buf_dup - Copy a Buffer into a new allocated buffer
 * @param buf Buffer to copy
 * @retval buf New allocated copy of buffer
 *
 * @note Caller must free the returned buffer
 */
struct Buffer *buf_dup(const struct Buffer *buf)
{
  if (!buf)
    return NULL;

  return buf_new(buf_string(buf));
}

/**
 * buf_copy - Copy a Buffer's contents to another Buffer
 * @param dst Buffer for result
 * @param src Buffer to copy
 * @retval num Bytes written to Buffer
 * @retval 0   Error
 */
size_t buf_copy(struct Buffer *dst, const struct Buffer *src)
{
  if (!dst)
    return 0;

  buf_reset(dst);
  if (!src || !src->data)
    return 0;

  return buf_addstr_n(dst, src->data, buf_len(src));
}

/**
 * buf_seek - Set current read/write position to offset from beginning
 * @param buf    Buffer to use
 * @param offset Distance from the beginning
 *
 * This is used for cases where the buffer is read from
 * A value is placed in the buffer, and then b->dptr is set back to the
 * beginning as a read marker instead of write marker.
 */
void buf_seek(struct Buffer *buf, size_t offset)
{
  if (buf && (offset < buf_len(buf)))
  {
    buf->dptr = buf->data ? buf->data + offset : NULL;
  }
}

/**
 * buf_find_string - Return a pointer to a substring found in the buffer
 * @param buf    Buffer to search
 * @param s      Substring to find
 * @retval NULL substring not found
 * @retval n    Pointer to the beginning of the found substring
 */
const char *buf_find_string(struct Buffer *buf, const char *s)
{
  if (!buf || !s)
    return NULL;

  return strstr(buf->data, s);
}

/**
 * buf_find_char - Return a pointer to a char found in the buffer
 * @param buf    Buffer to search
 * @param c      Char to find
 * @retval NULL char not found
 * @retval ptr  Pointer to the found char
 */
const char *buf_find_char(struct Buffer *buf, const char c)
{
  if (!buf)
    return NULL;

  return strchr(buf->data, c);
}

/**
 * buf_at - Return the character at the given offset
 * @param buf    Buffer to search
 * @param offset Offset to get
 * @retval NUL Offset out of bounds
 * @return n   The char at the offset
 */
char buf_at(struct Buffer *buf, size_t offset)
{
  if (!buf || (offset > buf_len(buf)))
    return '\0';

  return buf->data[offset];
}

/**
 * buf_str_equal - Return if two buffers are equal
 * @param a - Buffer to compare
 * @param b - Buffer to compare
 * @retval true  Strings are equal
 * @retval false String are not equal
 */
bool buf_str_equal(struct Buffer *a, struct Buffer *b)
{
  return mutt_str_equal(buf_string(a), buf_string(b));
}

/**
 * buf_istr_equal - Return if two buffers are equal, case insentive
 * @param a - First buffer to compare
 * @param b - Second buffer to compare
 * @retval true  Strings are equal
 * @retval false String are not equal
 */
bool buf_istr_equal(struct Buffer *a, struct Buffer *b)
{
  return mutt_istr_equal(buf_string(a), buf_string(b));
}

/**
 * buf_coll - Collate two strings (compare using locale)
 * @param a First buffer to compare
 * @param b Second buffer to compare
 * @retval <0 a precedes b
 * @retval  0 a and b are identical
 * @retval >0 b precedes a
 */
int buf_coll(struct Buffer *a, struct Buffer *b)
{
  return mutt_str_coll(buf_string(a), buf_string(b));
}

/**
 * buf_lower - Sets a buffer to lowercase
 * @param[out] buf Buffer to transform to lowercase
 *
 * @note Modifies the buffer
 */
void buf_lower(struct Buffer *buf)
{
  if (!buf)
    return;

  mutt_str_lower(buf->data);
}

/**
 * buf_upper - Sets a buffer to  uppercase
 * @param[out] buf Buffer to transform to uppercase
 *
 * @note Modifies the buffer
 */
void buf_upper(struct Buffer *buf)
{
  if (!buf)
    return;

  mutt_str_upper(buf->data);
}
