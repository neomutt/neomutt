/**
 * @file
 * A global pool of Buffers
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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
 * @page mutt_pool A global pool of Buffers
 *
 * A shared pool of Buffers to save lots of allocs/frees.
 */

#include "config.h"
#include <stdio.h>
#include "pool.h"
#include "buffer.h"
#include "logging2.h"
#include "memory.h"

/// Number of buffers in the pool
static size_t BufferPoolCount = 0;
/// Total size of the pool
static size_t BufferPoolLen = 0;
/// Amount to increase the size of the pool
static const size_t BufferPoolIncrement = 20;
/// Minimum size for a buffer
static const size_t BufferPoolInitialBufferSize = 1024;
/// A pool of buffers
static struct Buffer **BufferPool = NULL;

/**
 * pool_increase_size - Increase the size of the Buffer pool
 */
static void pool_increase_size(void)
{
  BufferPoolLen += BufferPoolIncrement;
  mutt_debug(LL_DEBUG1, "%zu\n", BufferPoolLen);

  mutt_mem_realloc(&BufferPool, BufferPoolLen * sizeof(struct Buffer *));
  while (BufferPoolCount < BufferPoolIncrement)
  {
    struct Buffer *newbuf = buf_new(NULL);
    buf_alloc(newbuf, BufferPoolInitialBufferSize);
    BufferPool[BufferPoolCount++] = newbuf;
  }
}

/**
 * buf_pool_free - Release the Buffer pool
 */
void buf_pool_free(void)
{
  mutt_debug(LL_DEBUG1, "%zu of %zu returned to pool\n", BufferPoolCount, BufferPoolLen);

  while (BufferPoolCount)
    buf_free(&BufferPool[--BufferPoolCount]);
  FREE(&BufferPool);
  BufferPoolLen = 0;
}

/**
 * buf_pool_get - Get a Buffer from the pool
 * @retval ptr Buffer
 */
struct Buffer *buf_pool_get(void)
{
  if (BufferPoolCount == 0)
    pool_increase_size();
  return BufferPool[--BufferPoolCount];
}

/**
 * buf_pool_release - Return a Buffer to the pool
 * @param[out] ptr Buffer to release
 *
 * @note The pointer will be NULL'd
 */
void buf_pool_release(struct Buffer **ptr)
{
  if (!ptr || !*ptr)
    return;

  if (BufferPoolCount >= BufferPoolLen)
  {
    // LCOV_EXCL_START
    mutt_debug(LL_DEBUG1, "Internal buffer pool error\n");
    buf_free(ptr);
    return;
    // LCOV_EXCL_STOP
  }

  // Reset the size if it's too big or too small
  struct Buffer *buf = *ptr;
  if ((buf->dsize > (2 * BufferPoolInitialBufferSize)) ||
      (buf->dsize < BufferPoolInitialBufferSize))
  {
    buf->dsize = BufferPoolInitialBufferSize;
    mutt_mem_realloc(&buf->data, buf->dsize);
  }
  buf_reset(buf);
  BufferPool[BufferPoolCount++] = buf;

  *ptr = NULL;
}
