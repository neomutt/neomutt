/**
 * @file
 * API for the header cache compression
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
 * @page compress COMPRESS: Compression
 *
 * @subpage compress_compress
 *
 * Compression Backends:
 *
 * | File                | Description            |
 * | :------------------ | :--------------------- |
 * | compress/lz4.c      | @subpage compress_lz4  |
 * | compress/zlib.c     | @subpage compress_zlib |
 * | compress/zstd.c     | @subpage compress_zstd |
 */

#ifndef MUTT_COMPRESS_LIB_H
#define MUTT_COMPRESS_LIB_H

#include <stdlib.h>

/**
 * struct ComprOps - Header Cache Compression API
 */
struct ComprOps
{
  const char *name; ///< Compression name

  /**
   * open - Open a compression context
   * @retval ptr  Success, backend-specific context
   * @retval NULL Otherwise
   */
  void *(*open)(void);

  /**
   * compress - Compress header cache data
   * @param[in]  cctx Compression context
   * @param[in]  data Data to be compressed
   * @param[in]  dlen Length of the uncompressed data
   * @param[out] clen Length of returned compressed data
   * @retval ptr  Success, pointer to compressed data
   * @retval NULL Otherwise
   *
   * @note This function returns a pointer to data, which will be freed by the
   *       close() function.
   */
  void *(*compress)(void *cctx, const char *data, size_t dlen, size_t *clen);

  /**
   * decompress - Decompress header cache data
   * @param[in] cctx Compression context
   * @param[in] cbuf Data to be decompressed
   * @param[in] clen Length of the compressed input data
   * @retval ptr  Success, pointer to decompressed data
   * @retval NULL Otherwise
   *
   * @note This function returns a pointer to data, which will be freed by the
   *       close() function.
   */
  void *(*decompress)(void *cctx, const char *cbuf, size_t clen);

  /**
   * close - Close a compression context
   * @param[out] cctx Backend-specific context retrieved via open()
   *
   * @note This function will free all allocated resources, which were
   *       allocated by open(), compress() or decompress()
   */
  void (*close)(void **cctx);
};

extern const struct ComprOps compr_lz4_ops;
extern const struct ComprOps compr_zlib_ops;
extern const struct ComprOps compr_zstd_ops;

const struct ComprOps *compress_get_ops(const char *compr);
const char *           compress_list   (void);

#endif /* MUTT_COMPRESS_LIB_H */
