/**
 * @file
 * Zstandard (zstd) compression
 *
 * @authors
 * Copyright (C) 2019-2020 Tino Reichardt <milky-neomutt@mcmilk.de>
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
 * @page compress_zstd Zstandard (zstd) compression
 *
 * Zstandard (zstd) compression.
 * https://www.zstd.net
 */

#include "config.h"
#include <stdio.h>
#include <zstd.h>
#include "private.h"
#include "mutt/lib.h"
#include "lib.h"

#define MIN_COMP_LEVEL 1  ///< Minimum compression level for zstd
#define MAX_COMP_LEVEL 22 ///< Maximum compression level for zstd

/**
 * struct ZstdComprData - Private Zstandard Compression Data
 */
struct ZstdComprData
{
  void *buf;   ///< Temporary buffer
  short level; ///< Compression Level to be used

  ZSTD_CCtx *cctx; ///< Compression context
  ZSTD_DCtx *dctx; ///< Decompression context
};

/**
 * zstd_cdata_free - Free Zstandard Compression Data
 * @param ptr Zstandard Compression Data to free
 */
static void zstd_cdata_free(struct ZstdComprData **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct ZstdComprData *cdata = *ptr;
  FREE(&cdata->buf);

  FREE(ptr);
}

/**
 * zstd_cdata_new - Create new Zstandard Compression Data
 * @retval ptr New Zstandard Compression Data
 */
static struct ZstdComprData *zstd_cdata_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct ZstdComprData));
}

/**
 * compr_zstd_open - Implements ComprOps::open() - @ingroup compress_open
 */
static ComprHandle *compr_zstd_open(short level)
{
  struct ZstdComprData *cdata = zstd_cdata_new();

  cdata->buf = mutt_mem_calloc(1, ZSTD_compressBound(1024 * 128));
  cdata->cctx = ZSTD_createCCtx();
  cdata->dctx = ZSTD_createDCtx();

  if (!cdata->cctx || !cdata->dctx)
  {
    // LCOV_EXCL_START
    ZSTD_freeCCtx(cdata->cctx);
    ZSTD_freeDCtx(cdata->dctx);
    zstd_cdata_free(&cdata);
    return NULL;
    // LCOV_EXCL_STOP
  }

  if ((level < MIN_COMP_LEVEL) || (level > MAX_COMP_LEVEL))
  {
    mutt_debug(LL_DEBUG1, "The compression level for %s should be between %d and %d",
               compr_zstd_ops.name, MIN_COMP_LEVEL, MAX_COMP_LEVEL);
    level = MIN_COMP_LEVEL;
  }

  cdata->level = level;

  // Return an opaque pointer
  return (ComprHandle *) cdata;
}

/**
 * compr_zstd_compress - Implements ComprOps::compress() - @ingroup compress_compress
 */
static void *compr_zstd_compress(ComprHandle *handle, const char *data,
                                 size_t dlen, size_t *clen)
{
  if (!handle)
    return NULL;

  // Decloak an opaque pointer
  struct ZstdComprData *cdata = handle;

  size_t len = ZSTD_compressBound(dlen);
  mutt_mem_realloc(&cdata->buf, len);

  size_t rc = ZSTD_compressCCtx(cdata->cctx, cdata->buf, len, data, dlen, cdata->level);
  if (ZSTD_isError(rc))
    return NULL; // LCOV_EXCL_LINE

  *clen = rc;

  return cdata->buf;
}

/**
 * compr_zstd_decompress - Implements ComprOps::decompress() - @ingroup compress_decompress
 */
static void *compr_zstd_decompress(ComprHandle *handle, const char *cbuf, size_t clen)
{
  if (!handle)
    return NULL;

  // Decloak an opaque pointer
  struct ZstdComprData *cdata = handle;

  unsigned long long len = ZSTD_getFrameContentSize(cbuf, clen);
  if (len == ZSTD_CONTENTSIZE_UNKNOWN)
    return NULL; // LCOV_EXCL_LINE
  else if (len == ZSTD_CONTENTSIZE_ERROR)
    return NULL;
  else if (len == 0)
    return NULL; // LCOV_EXCL_LINE
  mutt_mem_realloc(&cdata->buf, len);

  size_t rc = ZSTD_decompressDCtx(cdata->dctx, cdata->buf, len, cbuf, clen);
  if (ZSTD_isError(rc))
    return NULL; // LCOV_EXCL_LINE

  return cdata->buf;
}

/**
 * compr_zstd_close - Implements ComprOps::close() - @ingroup compress_close
 */
static void compr_zstd_close(ComprHandle **ptr)
{
  if (!ptr || !*ptr)
    return;

  // Decloak an opaque pointer
  struct ZstdComprData *cdata = *ptr;

  if (cdata->cctx)
    ZSTD_freeCCtx(cdata->cctx);

  if (cdata->dctx)
    ZSTD_freeDCtx(cdata->dctx);

  zstd_cdata_free((struct ZstdComprData **) ptr);
}

COMPRESS_OPS(zstd, MIN_COMP_LEVEL, MAX_COMP_LEVEL)
