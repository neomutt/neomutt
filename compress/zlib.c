/**
 * @file
 * ZLIB compression
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
 * @page compress_zlib ZLIB compression
 *
 * ZLIB compression.
 * https://www.zlib.net/
 */

#include "config.h"
#include <stddef.h>
#include <zconf.h>
#include <zlib.h>
#include "compress_private.h"
#include "mutt/lib.h"
#include "lib.h"
#include "hcache/lib.h"

#define MIN_COMP_LEVEL 1 ///< Minimum compression level for zlib
#define MAX_COMP_LEVEL 9 ///< Maximum compression level for zlib

/**
 * struct ComprZlibCtx - Private Zlib Compression Context
 */
struct ComprZlibCtx
{
  void *buf;   ///< Temporary buffer
  short level; ///< Compression Level to be used
};

/**
 * compr_zlib_open - Implements ComprOps::open()
 */
static void *compr_zlib_open(short level)
{
  struct ComprZlibCtx *ctx = mutt_mem_malloc(sizeof(struct ComprZlibCtx));

  ctx->buf = mutt_mem_malloc(compressBound(1024 * 32));

  if ((level < MIN_COMP_LEVEL) || (level > MAX_COMP_LEVEL))
  {
    mutt_warning(_("The compression level for %s should be between %d and %d"),
                 compr_zlib_ops.name, MIN_COMP_LEVEL, MAX_COMP_LEVEL);
    level = MIN_COMP_LEVEL;
  }

  ctx->level = level;

  return ctx;
}

/**
 * compr_zlib_compress - Implements ComprOps::compress()
 */
static void *compr_zlib_compress(void *cctx, const char *data, size_t dlen, size_t *clen)
{
  if (!cctx)
    return NULL;

  struct ComprZlibCtx *ctx = cctx;

  uLong len = compressBound(dlen);
  mutt_mem_realloc(&ctx->buf, len + 4);
  Bytef *cbuf = (unsigned char *) ctx->buf + 4;
  const void *ubuf = data;
  int rc = compress2(cbuf, &len, ubuf, dlen, ctx->level);
  if (rc != Z_OK)
    return NULL;
  *clen = len + 4;

  /* save ulen to first 4 bytes */
  unsigned char *cs = ctx->buf;
  cs[0] = dlen & 0xff;
  dlen >>= 8;
  cs[1] = dlen & 0xff;
  dlen >>= 8;
  cs[2] = dlen & 0xff;
  dlen >>= 8;
  cs[3] = dlen & 0xff;

  return ctx->buf;
}

/**
 * compr_zlib_decompress - Implements ComprOps::decompress()
 */
static void *compr_zlib_decompress(void *cctx, const char *cbuf, size_t clen)
{
  if (!cctx)
    return NULL;

  struct ComprZlibCtx *ctx = cctx;

  /* first 4 bytes store the size */
  const unsigned char *cs = (const unsigned char *) cbuf;
  uLong ulen = cs[0] + (cs[1] << 8) + (cs[2] << 16) + ((uLong) cs[3] << 24);
  if (ulen == 0)
    return NULL;

  mutt_mem_realloc(&ctx->buf, ulen);
  Bytef *ubuf = ctx->buf;
  cs = (const unsigned char *) cbuf;
  int ret = uncompress(ubuf, &ulen, cs + 4, clen - 4);
  if (ret != Z_OK)
    return NULL;

  return ubuf;
}

/**
 * compr_zlib_close - Implements ComprOps::close()
 */
static void compr_zlib_close(void **cctx)
{
  if (!cctx || !*cctx)
    return;

  struct ComprZlibCtx *ctx = *cctx;

  FREE(&ctx->buf);
  FREE(cctx);
}

COMPRESS_OPS(zlib)
