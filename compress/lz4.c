/**
 * @file
 * LZ4 compression
 *
 * @authors
 * Copyright (C) 2019-2020 Tino Reichardt <milky-neomutt@mcmilk.de>
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020-2023 Richard Russon <rich@flatcap.org>
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
 * @page compress_lz4 LZ4 compression
 *
 * LZ4 compression.
 * https://github.com/lz4/lz4
 */

#include "config.h"
#include <limits.h>
#include <lz4.h>
#include <stddef.h>
#include "private.h"
#include "mutt/lib.h"
#include "lib.h"

#define MIN_COMP_LEVEL 1  ///< Minimum compression level for lz4
#define MAX_COMP_LEVEL 12 ///< Maximum compression level for lz4

/**
 * struct Lz4ComprData - Private Lz4 Compression Data
 */
struct Lz4ComprData
{
  void *buf;   ///< Temporary buffer
  short level; ///< Compression Level to be used
};

/**
 * lz4_cdata_free - Free Lz4 Compression Data
 * @param ptr Lz4 Compression Data to free
 */
void lz4_cdata_free(struct Lz4ComprData **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Lz4ComprData *cdata = *ptr;
  FREE(&cdata->buf);

  FREE(ptr);
}

/**
 * lz4_cdata_new - Create new Lz4 Compression Data
 * @retval ptr New Lz4 Compression Data
 */
static struct Lz4ComprData *lz4_cdata_new(void)
{
  return MUTT_MEM_CALLOC(1, struct Lz4ComprData);
}

/**
 * compr_lz4_open - Open a compression context - Implements ComprOps::open() - @ingroup compress_open
 */
static ComprHandle *compr_lz4_open(short level)
{
  struct Lz4ComprData *cdata = lz4_cdata_new();

  cdata->buf = mutt_mem_calloc(LZ4_compressBound(1024 * 32), 1);

  if ((level < MIN_COMP_LEVEL) || (level > MAX_COMP_LEVEL))
  {
    mutt_debug(LL_DEBUG1, "The compression level for %s should be between %d and %d",
               compr_lz4_ops.name, MIN_COMP_LEVEL, MAX_COMP_LEVEL);
    level = MIN_COMP_LEVEL;
  }

  cdata->level = level;

  // Return an opaque pointer
  return (ComprHandle *) cdata;
}

/**
 * compr_lz4_compress - Compress header cache data - Implements ComprOps::compress() - @ingroup compress_compress
 */
static void *compr_lz4_compress(ComprHandle *handle, const char *data,
                                size_t dlen, size_t *clen)
{
  if (!handle || (dlen > INT_MAX))
    return NULL;

  // Decloak an opaque pointer
  struct Lz4ComprData *cdata = handle;

  int datalen = dlen;
  int len = LZ4_compressBound(dlen);
  if (len > (INT_MAX - 4))
    return NULL; // LCOV_EXCL_LINE
  mutt_mem_realloc(&cdata->buf, len + 4);
  char *cbuf = cdata->buf;

  len = LZ4_compress_fast(data, cbuf + 4, datalen, len, cdata->level);
  if (len == 0)
    return NULL; // LCOV_EXCL_LINE
  *clen = len + 4;

  /* save ulen to first 4 bytes */
  unsigned char *cs = cdata->buf;
  cs[0] = dlen & 0xff;
  dlen >>= 8;
  cs[1] = dlen & 0xff;
  dlen >>= 8;
  cs[2] = dlen & 0xff;
  dlen >>= 8;
  cs[3] = dlen & 0xff;

  return cdata->buf;
}

/**
 * compr_lz4_decompress - Decompress header cache data - Implements ComprOps::decompress() - @ingroup compress_decompress
 */
static void *compr_lz4_decompress(ComprHandle *handle, const char *cbuf, size_t clen)
{
  if (!handle)
    return NULL;

  // Decloak an opaque pointer
  struct Lz4ComprData *cdata = handle;

  /* first 4 bytes store the size */
  const unsigned char *cs = (const unsigned char *) cbuf;
  if (clen < 4)
    return NULL;
  size_t ulen = cs[0] + (cs[1] << 8) + (cs[2] << 16) + ((size_t) cs[3] << 24);
  if (ulen > INT_MAX)
    return NULL; // LCOV_EXCL_LINE
  if (ulen == 0)
    return (void *) cbuf;

  mutt_mem_realloc(&cdata->buf, ulen);
  void *ubuf = cdata->buf;
  const char *data = cbuf;
  int rc = LZ4_decompress_safe(data + 4, ubuf, clen - 4, ulen);
  if (rc < 0)
    return NULL;

  return ubuf;
}

/**
 * compr_lz4_close - Close a compression context - Implements ComprOps::close() - @ingroup compress_close
 */
static void compr_lz4_close(ComprHandle **ptr)
{
  if (!ptr || !*ptr)
    return;

  // Decloak an opaque pointer
  lz4_cdata_free((struct Lz4ComprData **) ptr);
}

COMPRESS_OPS(lz4, MIN_COMP_LEVEL, MAX_COMP_LEVEL)
