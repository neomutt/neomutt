/**
 * @file
 * Test code for zlib compression
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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

#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "compress/lib.h"
#include "common.h" // IWYU pragma: keep

#define MIN_COMP_LEVEL 1 ///< Minimum compression level for zlib
#define MAX_COMP_LEVEL 9 ///< Maximum compression level for zlib

void test_compress_zlib(void)
{
  // ComprHandle *open(short level);
  // void *compress(ComprHandle *handle, const char *data, size_t dlen, size_t *clen);
  // void *decompress(ComprHandle *handle, const char *cbuf, size_t clen);
  // void close(ComprHandle **ptr);

  const struct ComprOps *compr_ops = compress_get_ops("zlib");
  if (!TEST_CHECK(compr_ops != NULL))
    return;

  {
    // Degenerate tests
    TEST_CHECK(compr_ops->compress(NULL, NULL, 0, NULL) == NULL);
    TEST_CHECK(compr_ops->decompress(NULL, NULL, 0) == NULL);
    ComprHandle *compr_handle = NULL;
    compr_ops->close(NULL);
    TEST_CHECK_(1, "compr_ops->close(NULL)");
    compr_ops->close(&compr_handle);
    TEST_CHECK_(1, "compr_ops->close(&compr_handle)");
  }

  {
    // Temporarily disable logging
    MuttLogger = log_disp_null;

    ComprHandle *compr_handle = compr_ops->open(MIN_COMP_LEVEL - 1);
    TEST_CHECK(compr_handle != NULL);
    compr_ops->close(&compr_handle);

    compr_handle = compr_ops->open(MAX_COMP_LEVEL + 1);
    TEST_CHECK(compr_handle != NULL);
    compr_ops->close(&compr_handle);

    // Restore logging
    MuttLogger = log_disp_terminal;
  }

  {
    // Garbage data
    ComprHandle *compr_handle = compr_ops->open(MIN_COMP_LEVEL);
    TEST_CHECK(compr_handle != NULL);

    const char zeroes[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    void *result = compr_ops->decompress(compr_handle, zeroes, 0);
    TEST_CHECK(result == NULL);

    result = compr_ops->decompress(compr_handle, zeroes, sizeof(zeroes));
    TEST_CHECK(result == NULL);

    const char ones[] = { 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
                          0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 };
    result = compr_ops->decompress(compr_handle, ones, sizeof(ones));
    TEST_CHECK(result == NULL);

    compr_ops->close(&compr_handle);
  }

  compress_data_tests(compr_ops, MIN_COMP_LEVEL, MAX_COMP_LEVEL);
}
