/**
 * @file
 * LZ4 header cache compression
 *
 * @authors
 * Copyright (C) 2019 Tino Reichardt <milky-neomutt@mcmilk.de>
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
 * Use LZ4 header cache compression for database backends.
 */

#include "config.h"
#include <stddef.h>
#include <lz4.h>
#include "compress_private.h"
#include "mutt/lib.h"
#include "lib.h"
#include "hcache/lib.h"

/**
 * struct ComprLz4Ctx - Private Lz4 Compression Context
 */
struct ComprLz4Ctx
{
  void *buf; ///< Temporary buffer
};

/**
 * compr_lz4_open - Implements ComprOps::open()
 */
static void *compr_lz4_open(void)
{
  struct ComprLz4Ctx *ctx = mutt_mem_malloc(sizeof(struct ComprLz4Ctx));

  ctx->buf = mutt_mem_malloc(LZ4_compressBound(1024 * 32));

  return ctx;
}

/**
 * compr_lz4_compress - Implements ComprOps::compress()
 */
static void *compr_lz4_compress(void *cctx, const char *data, size_t dlen, size_t *clen)
{
  if (!cctx)
    return NULL;

  struct ComprLz4Ctx *ctx = cctx;
  int datalen = dlen;
  int len = LZ4_compressBound(dlen);
  mutt_mem_realloc(&ctx->buf, len + 4);
  char *cbuf = ctx->buf;

  len = LZ4_compress_fast(data, cbuf + 4, datalen, len, C_HeaderCacheCompressLevel);
  if (len == 0)
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
 * compr_lz4_decompress - Implements ComprOps::decompress()
 */
static void *compr_lz4_decompress(void *cctx, const char *cbuf, size_t clen)
{
  if (!cctx)
    return NULL;

  struct ComprLz4Ctx *ctx = cctx;

  /* first 4 bytes store the size */
  const unsigned char *cs = (const unsigned char *) cbuf;
  size_t ulen = cs[0] + (cs[1] << 8) + (cs[2] << 16) + (cs[3] << 24);
  if (ulen == 0)
    return (void *) cbuf;

  mutt_mem_realloc(&ctx->buf, ulen);
  void *ubuf = ctx->buf;
  const char *data = cbuf;
  int ret = LZ4_decompress_safe(data + 4, ubuf, clen - 4, ulen);
  if (ret < 0)
    return NULL;

  return ubuf;
}

/**
 * compr_lz4_close - Implements ComprOps::close()
 */
static void compr_lz4_close(void **cctx)
{
  if (!cctx || !*cctx)
    return;

  struct ComprLz4Ctx *ctx = *cctx;

  FREE(&ctx->buf);
  FREE(cctx);
}

COMPRESS_OPS(lz4)
