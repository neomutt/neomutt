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
#include "compress_private.h"
#include "mutt/lib.h"
#include "lib.h"
#include "hcache/lib.h"

#define MIN_COMP_LEVEL 1  ///< Minimum compression level for zstd
#define MAX_COMP_LEVEL 22 ///< Maximum compression level for zstd

/**
 * struct ComprZstdCtx - Private Zstandard Compression Context
 */
struct ComprZstdCtx
{
  void *buf;   ///< Temporary buffer
  short level; ///< Compression Level to be used

  ZSTD_CCtx *cctx; ///< Compression context
  ZSTD_DCtx *dctx; ///< Decompression context
};

/**
 * compr_zstd_open - Implements ComprOps::open()
 */
static void *compr_zstd_open(short level)
{
  struct ComprZstdCtx *ctx = mutt_mem_malloc(sizeof(struct ComprZstdCtx));

  ctx->buf = mutt_mem_malloc(ZSTD_compressBound(1024 * 128));
  ctx->cctx = ZSTD_createCCtx();
  ctx->dctx = ZSTD_createDCtx();

  if (!ctx->cctx || !ctx->dctx)
  {
    if (ctx->cctx)
      ZSTD_freeCCtx(ctx->cctx);
    if (ctx->dctx)
      ZSTD_freeDCtx(ctx->dctx);
    FREE(&ctx);
    return NULL;
  }

  if ((level < MIN_COMP_LEVEL) || (level > MAX_COMP_LEVEL))
  {
    mutt_warning(_("The compression level for %s should be between %d and %d"),
                 compr_zstd_ops.name, MIN_COMP_LEVEL, MAX_COMP_LEVEL);
    level = MIN_COMP_LEVEL;
  }

  ctx->level = level;

  return ctx;
}

/**
 * compr_zstd_compress - Implements ComprOps::compress()
 */
static void *compr_zstd_compress(void *cctx, const char *data, size_t dlen, size_t *clen)
{
  if (!cctx)
    return NULL;

  struct ComprZstdCtx *ctx = cctx;

  size_t len = ZSTD_compressBound(dlen);
  mutt_mem_realloc(&ctx->buf, len);

  size_t ret = ZSTD_compressCCtx(ctx->cctx, ctx->buf, len, data, dlen, ctx->level);
  if (ZSTD_isError(ret))
    return NULL;

  *clen = ret;

  return ctx->buf;
}

/**
 * compr_zstd_decompress - Implements ComprOps::decompress()
 */
static void *compr_zstd_decompress(void *cctx, const char *cbuf, size_t clen)
{
  struct ComprZstdCtx *ctx = cctx;

  if (!cctx)
    return NULL;

  unsigned long long len = ZSTD_getFrameContentSize(cbuf, clen);
  if (len == ZSTD_CONTENTSIZE_UNKNOWN)
    return NULL;
  else if (len == ZSTD_CONTENTSIZE_ERROR)
    return NULL;
  else if (len == 0)
    return NULL;
  mutt_mem_realloc(&ctx->buf, len);

  size_t ret = ZSTD_decompressDCtx(ctx->dctx, ctx->buf, len, cbuf, clen);
  if (ZSTD_isError(ret))
    return NULL;

  return ctx->buf;
}

/**
 * compr_zstd_close - Implements ComprOps::close()
 */
static void compr_zstd_close(void **cctx)
{
  if (!cctx || !*cctx)
    return;

  struct ComprZstdCtx *ctx = *cctx;

  if (ctx->cctx)
    ZSTD_freeCCtx(ctx->cctx);

  if (ctx->dctx)
    ZSTD_freeDCtx(ctx->dctx);

  FREE(&ctx->buf);
  FREE(cctx);
}

COMPRESS_OPS(zstd)
