/**
 * @file
 * LZ4 compression
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
 * @page compress_lz4 LZ4 compression
 *
 * LZ4 compression.
 * https://github.com/lz4/lz4
 */

#include "config.h"
#include <stddef.h>
#include <limits.h>
#include <lz4.h>
#include "private.h"
#include "mutt/lib.h"
#include "lib.h"

#define MIN_COMP_LEVEL 1  ///< Minimum compression level for lz4
#define MAX_COMP_LEVEL 12 ///< Maximum compression level for lz4

/**
 * struct ComprLz4Ctx - Private Lz4 Compression Context
 */
struct ComprLz4Ctx
{
  void *buf;   ///< Temporary buffer
  short level; ///< Compression Level to be used
};

/**
 * lz4_cdata_free - Free Lz4 Compression Data
 * @param ptr Lz4 Compression Data to free
 */
static void lz4_cdata_free(struct ComprLz4Ctx **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct ComprLz4Ctx *cdata = *ptr;
  FREE(&cdata->buf);

  FREE(ptr);
}

/**
 * lz4_cdata_new - Create new Lz4 Compression Data
 * @retval ptr New Lz4 Compression Data
 */
static struct ComprLz4Ctx *lz4_cdata_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct ComprLz4Ctx));
}

/**
 * compr_lz4_open - Implements ComprOps::open() - @ingroup compress_open
 */
static void *compr_lz4_open(short level)
{
  struct ComprLz4Ctx *ctx = lz4_cdata_new();

  ctx->buf = mutt_mem_malloc(LZ4_compressBound(1024 * 32));

  if ((level < MIN_COMP_LEVEL) || (level > MAX_COMP_LEVEL))
  {
    mutt_debug(LL_DEBUG1, "The compression level for %s should be between %d and %d",
               compr_lz4_ops.name, MIN_COMP_LEVEL, MAX_COMP_LEVEL);
    level = MIN_COMP_LEVEL;
  }

  ctx->level = level;

  return ctx;
}

/**
 * compr_lz4_compress - Implements ComprOps::compress() - @ingroup compress_compress
 */
static void *compr_lz4_compress(void *cctx, const char *data, size_t dlen, size_t *clen)
{
  if (!cctx || (dlen > INT_MAX))
    return NULL;

  struct ComprLz4Ctx *ctx = cctx;

  int datalen = dlen;
  int len = LZ4_compressBound(dlen);
  if (len > (INT_MAX - 4))
    return NULL; // LCOV_EXCL_LINE
  mutt_mem_realloc(&ctx->buf, len + 4);
  char *cbuf = ctx->buf;

  len = LZ4_compress_fast(data, cbuf + 4, datalen, len, ctx->level);
  if (len == 0)
    return NULL; // LCOV_EXCL_LINE
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
 * compr_lz4_decompress - Implements ComprOps::decompress() - @ingroup compress_decompress
 */
static void *compr_lz4_decompress(void *cctx, const char *cbuf, size_t clen)
{
  if (!cctx)
    return NULL;

  struct ComprLz4Ctx *ctx = cctx;

  /* first 4 bytes store the size */
  const unsigned char *cs = (const unsigned char *) cbuf;
  if (clen < 4)
    return NULL;
  size_t ulen = cs[0] + (cs[1] << 8) + (cs[2] << 16) + ((size_t) cs[3] << 24);
  if (ulen > INT_MAX)
    return NULL; // LCOV_EXCL_LINE
  if (ulen == 0)
    return (void *) cbuf;

  mutt_mem_realloc(&ctx->buf, ulen);
  void *ubuf = ctx->buf;
  const char *data = cbuf;
  int rc = LZ4_decompress_safe(data + 4, ubuf, clen - 4, ulen);
  if (rc < 0)
    return NULL;

  return ubuf;
}

/**
 * compr_lz4_close - Implements ComprOps::close() - @ingroup compress_close
 */
static void compr_lz4_close(void **cctx)
{
  if (!cctx || !*cctx)
    return;

  lz4_cdata_free((struct ComprLz4Ctx **) cctx);
}

COMPRESS_OPS(lz4, MIN_COMP_LEVEL, MAX_COMP_LEVEL)
