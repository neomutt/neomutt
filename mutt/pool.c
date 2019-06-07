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
 * @page pool A global pool of Buffers
 *
 * A shared pool of Buffers to save lots of allocs/frees.
 */

#include "config.h"
#include <stdio.h>
#include "buffer.h"
#include "logging.h"
#include "memory.h"

static size_t BufferPoolCount = 0;
static size_t BufferPoolLen = 0;
static size_t BufferPoolIncrement = 20;
static size_t BufferPoolInitialBufferSize = 1024;
static struct Buffer **BufferPool = NULL;

/**
 * increase_buffer_pool - Increase the size of the Buffer pool
 */
static void increase_buffer_pool(void)
{
  struct Buffer *newbuf;
  BufferPoolLen += BufferPoolIncrement;
  mutt_debug(LL_DEBUG1, "%zu\n", BufferPoolLen);

  mutt_mem_realloc(&BufferPool, BufferPoolLen * sizeof(struct Buffer *));
  while (BufferPoolCount < BufferPoolIncrement)
  {
    newbuf = mutt_buffer_alloc(BufferPoolInitialBufferSize);
    BufferPool[BufferPoolCount++] = newbuf;
  }
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
  if (buf->dsize > 2 * BufferPoolInitialBufferSize)
  {
    buf->dsize = BufferPoolInitialBufferSize;
    mutt_mem_realloc(&buf->data, buf->dsize);
  }
  mutt_buffer_reset(buf);
  BufferPool[BufferPoolCount++] = buf;

  *pbuf = NULL;
}
