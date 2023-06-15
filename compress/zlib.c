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
#include "private.h"
#include "mutt/lib.h"
#include "lib.h"

#define MIN_COMP_LEVEL 1 ///< Minimum compression level for zlib
#define MAX_COMP_LEVEL 9 ///< Maximum compression level for zlib

/**
 * struct ZlibComprData - Private Zlib Compression Data
 */
struct ZlibComprData
{
  void *buf;   ///< Temporary buffer
  short level; ///< Compression Level to be used
};

/**
 * zlib_cdata_free - Free Zlib Compression Data
 * @param ptr Zlib Compression Data to free
 */
static void zlib_cdata_free(struct ZlibComprData **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct ZlibComprData *cdata = *ptr;
  FREE(&cdata->buf);

  FREE(ptr);
}

/**
 * zlib_cdata_new - Create new Zlib Compression Data
 * @retval ptr New Zlib Compression Data
 */
static struct ZlibComprData *zlib_cdata_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct ZlibComprData));
}

/**
 * compr_zlib_open - Implements ComprOps::open() - @ingroup compress_open
 */
static ComprHandle *compr_zlib_open(short level)
{
  struct ZlibComprData *cdata = zlib_cdata_new();

  cdata->buf = mutt_mem_calloc(1, compressBound(1024 * 32));

  if ((level < MIN_COMP_LEVEL) || (level > MAX_COMP_LEVEL))
  {
    mutt_debug(LL_DEBUG1, "The compression level for %s should be between %d and %d",
               compr_zlib_ops.name, MIN_COMP_LEVEL, MAX_COMP_LEVEL);
    level = MIN_COMP_LEVEL;
  }

  cdata->level = level;

  // Return an opaque pointer
  return (ComprHandle *) cdata;
}

/**
 * compr_zlib_compress - Implements ComprOps::compress() - @ingroup compress_compress
 */
static void *compr_zlib_compress(ComprHandle *handle, const char *data,
                                 size_t dlen, size_t *clen)
{
  if (!handle)
    return NULL;

  // Decloak an opaque pointer
  struct ZlibComprData *cdata = handle;

  uLong len = compressBound(dlen);
  mutt_mem_realloc(&cdata->buf, len + 4);
  Bytef *cbuf = (unsigned char *) cdata->buf + 4;
  const void *ubuf = data;
  int rc = compress2(cbuf, &len, ubuf, dlen, cdata->level);
  if (rc != Z_OK)
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
 * compr_zlib_decompress - Implements ComprOps::decompress() - @ingroup compress_decompress
 */
static void *compr_zlib_decompress(ComprHandle *handle, const char *cbuf, size_t clen)
{
  if (!handle)
    return NULL;

  // Decloak an opaque pointer
  struct ZlibComprData *cdata = handle;

  /* first 4 bytes store the size */
  const unsigned char *cs = (const unsigned char *) cbuf;
  if (clen < 4)
    return NULL;
  uLong ulen = cs[0] + (cs[1] << 8) + (cs[2] << 16) + ((uLong) cs[3] << 24);
  if (ulen == 0)
    return NULL;

  mutt_mem_realloc(&cdata->buf, ulen);
  Bytef *ubuf = cdata->buf;
  cs = (const unsigned char *) cbuf;
  int rc = uncompress(ubuf, &ulen, cs + 4, clen - 4);
  if (rc != Z_OK)
    return NULL;

  return ubuf;
}

/**
 * compr_zlib_close - Implements ComprOps::close() - @ingroup compress_close
 */
static void compr_zlib_close(ComprHandle **ptr)
{
  if (!ptr || !*ptr)
    return;

  // Decloak an opaque pointer
  zlib_cdata_free((struct ZlibComprData **) ptr);
}

COMPRESS_OPS(zlib, MIN_COMP_LEVEL, MAX_COMP_LEVEL)
