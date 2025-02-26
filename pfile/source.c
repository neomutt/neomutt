/**
 * @file
 * Source XXX
 *
 * @authors
 * Copyright (C) 2024-2025 Richard Russon <rich@flatcap.org>
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

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "source.h"

/// Maximum size of the in-memory cache (64KiB)
const int CacheMaxSize = 65536;

/// Amount to increase cache size (4KiB)
const int CacheStepSize = 4096;

/**
 * source_free - XXX
 */
void source_free(struct Source **pptr)
{
  if (!pptr || !*pptr)
    return;

  struct Source *src = *pptr;

  if (src->close_fp)
    mutt_file_fclose(&src->fp);

  FREE(src->cache);

  FREE(pptr);
}

/**
 * source_new - XXX
 */
struct Source *source_new(FILE *fp)
{
  struct Source *src = MUTT_MEM_CALLOC(1, struct Source);

  src->fp = fp;
  src->source_size = mutt_file_get_size_fp(fp);

  return src;
}

/**
 * cache_alloc - XXX
 */
bool cache_alloc(struct Source *src, int size)
{
  if (!src)
    return false;

  // Already enough space in the cache
  if ((size - 1) < src->cache_size)
    return true;

  // Adding the text would make the cache too large
  if ((size - 1) >= CacheMaxSize)
    return false;

  mutt_mem_realloc(&src->cache, src->cache_size + CacheStepSize);

  // Zero the new bit
  memset(src->cache + src->cache_size, 0, CacheStepSize);

  src->cache_size += CacheStepSize;
  return true;
}

/**
 * cache_add_text - XXX
 */
long cache_add_text(struct Source *src, const char *text, int bytes)
{
  if (!src || !text || (bytes < 1))
    return -1;

  if (!cache_alloc(src, src->source_size + bytes))
    return -1;

  memcpy(src->cache + src->source_size, text, bytes);

  long offset = src->source_size;
  src->source_size += bytes;

  return offset;
}

/**
 * file_add_text - XXX
 */
long file_add_text(struct Source *src, const char *text, int bytes)
{
  if (!src || !text || (bytes < 1))
    return -1;

  return 0;
}

/**
 * source_add_text - XXX
 */
long source_add_text(struct Source *src, const char *text, int bytes)
{
  if (!src || !text)
    return -1;

  if (bytes < 0)
    bytes = mutt_str_len(text);

  long offset = cache_add_text(src, text, bytes);
  if (offset >= 0)
    return offset;

  return file_add_text(src, text, bytes);
}

/**
 * source_get_text - XXX
 */
const char *source_get_text(struct Source *src, long offset)
{
  if (!src)
    return NULL;

  if (offset < src->cache_size)
    return src->cache + offset;

  return NULL;
}
