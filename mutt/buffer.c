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
#include "logging.h"
#include "memory.h"
#include "string2.h"

static size_t BufferPoolCount = 0;
static size_t BufferPoolLen = 0;
static struct Buffer **BufferPool = NULL;

/**
 * mutt_buffer_new - Create and initialise a Buffer
 * @retval ptr New Buffer
 *
 * Call mutt_buffer_free() to release the Buffer.
 */
struct Buffer *mutt_buffer_new(void)
{
  struct Buffer *b = mutt_mem_malloc(sizeof(struct Buffer));

  mutt_buffer_init(b);

  return b;
}

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
 * mutt_buffer_reset - Reset an existing Buffer
 * @param buf Buffer to reset
 *
 * This can be called on a Buffer to reset the pointers,
 * effectively emptying it.
 */
void mutt_buffer_reset(struct Buffer *buf)
{
  if (!buf)
    return;
  memset(buf->data, 0, buf->dsize);
  buf->dptr = buf->data;
}

/**
 * mutt_buffer_from - Create Buffer from an existing string
 * @param seed String to put in the Buffer
 * @retval ptr New Buffer
 */
struct Buffer *mutt_buffer_from(const char *seed)
{
  struct Buffer *b = NULL;

  if (!seed)
    return NULL;

  b = mutt_buffer_new();
  b->data = mutt_str_strdup(seed);
  b->dsize = mutt_str_strlen(seed);
  b->dptr = (char *) b->data + b->dsize;
  return b;
}

/**
 * mutt_buffer_addstr_n - Add a string to a Buffer, expanding it if necessary
 * @param buf Buffer to add to
 * @param s   String to add
 * @param len Length of the string
 * @retval num Bytes written to Buffer
 *
 * Dynamically grow a Buffer to accommodate s, in increments of 128 bytes.
 * Always one byte bigger than necessary for the null terminator, and the
 * buffer is always NUL-terminated
 */
size_t mutt_buffer_addstr_n(struct Buffer *buf, const char *s, size_t len)
{
  if (!buf || !s)
    return 0;

  if ((buf->dptr + len + 1) > (buf->data + buf->dsize))
    mutt_buffer_increase_size(buf, buf->dsize + ((len < 128) ? 128 : len + 1));
  if (!buf->dptr)
    return 0;
  memcpy(buf->dptr, s, len);
  buf->dptr += len;
  *(buf->dptr) = '\0';
  return len;
}

/**
 * mutt_buffer_free - Release a Buffer and its contents
 * @param[out] p Buffer pointer to free and NULL
 */
void mutt_buffer_free(struct Buffer **p)
{
  if (!p || !*p)
    return;

  FREE(&(*p)->data);
  /* dptr is just an offset to data and shouldn't be freed */
  FREE(p);
}

/**
 * buffer_printf - Format a string into a Buffer
 * @param buf Buffer
 * @param fmt printf-style format string
 * @param ap  Arguments to be formatted
 * @retval num Characters written
 */
static int buffer_printf(struct Buffer *buf, const char *fmt, va_list ap)
{
  if (!buf)
    return 0;

  va_list ap_retry;
  int len, blen, doff;

  va_copy(ap_retry, ap);

  if (!buf->dptr)
    buf->dptr = buf->data;

  doff = buf->dptr - buf->data;
  blen = buf->dsize - doff;
  /* solaris 9 vsnprintf barfs when blen is 0 */
  if (blen == 0)
  {
    blen = 128;
    mutt_buffer_increase_size(buf, buf->dsize + blen);
  }
  len = vsnprintf(buf->dptr, blen, fmt, ap);
  if (len >= blen)
  {
    blen = ++len - blen;
    if (blen < 128)
      blen = 128;
    mutt_buffer_increase_size(buf, buf->dsize + blen);
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
 */
int mutt_buffer_printf(struct Buffer *buf, const char *fmt, ...)
{
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

  if (buf->data)
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
 */
int mutt_buffer_add_printf(struct Buffer *buf, const char *fmt, ...)
{
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
  return mutt_buffer_addstr_n(buf, s, mutt_str_strlen(s));
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
  if (!buf)
    return true;

  return buf->data && (buf->data[0] == '\0');
}

/**
 * mutt_buffer_alloc - Create a new Buffer
 * @param size Size of Buffer to create
 * @retval ptr Newly allocated Buffer
 */
struct Buffer *mutt_buffer_alloc(size_t size)
{
  struct Buffer *b = mutt_mem_calloc(1, sizeof(struct Buffer));

  b->data = mutt_mem_calloc(1, size);
  b->dptr = b->data;
  b->dsize = size;

  return b;
}

/**
 * mutt_buffer_strcpy - Copy a string into a Buffer
 * @param buf Buffer to overwrite
 * @param s   String to copy
 *
 * Overwrites any existing content.
 */
void mutt_buffer_strcpy(struct Buffer *buf, const char *s)
{
  mutt_buffer_reset(buf);
  mutt_buffer_addstr(buf, s);
}

/**
 * mutt_buffer_increase_size - Increase the allocated size of a buffer
 * @param buf      Buffer to change
 * @param new_size New size
 */
void mutt_buffer_increase_size(struct Buffer *buf, size_t new_size)
{
  if (!buf)
    return;

  if (new_size <= buf->dsize)
    return;

  size_t offset = buf->dptr - buf->data;
  buf->dsize = new_size;
  mutt_mem_realloc(&buf->data, buf->dsize);
  buf->dptr = buf->data + offset;
  /* This ensures an initially NULL buf->data is now properly terminated. */
  *buf->dptr = '\0';
}

/**
 * increase_buffer_pool - Increase the size of the Buffer pool
 */
static void increase_buffer_pool(void)
{
  struct Buffer *newbuf;

  BufferPoolLen += 5;
  mutt_mem_realloc(&BufferPool, BufferPoolLen * sizeof(struct Buffer *));
  while (BufferPoolCount < 5)
  {
    newbuf = mutt_buffer_alloc(1024);
    BufferPool[BufferPoolCount++] = newbuf;
  }
}

/**
 * mutt_buffer_pool_init - Initialise the Buffer pool
 */
void mutt_buffer_pool_init(void)
{
  increase_buffer_pool();
}

/**
 * mutt_buffer_pool_free - Release the Buffer pool
 */
void mutt_buffer_pool_free(void)
{
  mutt_debug(LL_DEBUG1, "mutt_buffer_pool_free: %zu of %zu returned to pool\n",
             BufferPoolCount, BufferPoolLen);

  while (BufferPoolCount)
    mutt_buffer_free(&BufferPool[--BufferPoolCount]);
  FREE(&BufferPool);
  BufferPoolLen = 0;
}

/**
 * mutt_buffer_pool_get - Get a Buffer from the pool
 * @retval ptr Buffer
 */
struct Buffer *mutt_buffer_pool_get(void)
{
  if (BufferPoolCount == 0)
    increase_buffer_pool();
  return BufferPool[--BufferPoolCount];
}

/**
 * mutt_buffer_pool_release - Free a Buffer from the pool
 * @param[out] pbuf Buffer to free
 */
void mutt_buffer_pool_release(struct Buffer **pbuf)
{
  if (!pbuf || !*pbuf)
    return;

  if (BufferPoolCount >= BufferPoolLen)
  {
    mutt_debug(LL_DEBUG1, "Internal buffer pool error\n");
    mutt_buffer_free(pbuf);
    return;
  }

  struct Buffer *buf = *pbuf;
  if (buf->dsize > 2048)
  {
    buf->dsize = 1024;
    mutt_mem_realloc(&buf->data, buf->dsize);
  }
  mutt_buffer_reset(buf);
  BufferPool[BufferPoolCount++] = buf;

  *pbuf = NULL;
}

/**
 * mutt_buffer_len - Calculate the length of a Buffer
 * @param buf Buffer
 * @retval num Size of buffer
 */
size_t mutt_buffer_len(const struct Buffer *buf)
{
  if (!buf)
    return 0;

  return buf->dptr - buf->data;
}
