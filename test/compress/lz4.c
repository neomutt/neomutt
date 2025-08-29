/**
 * @file
 * Test code for lz4 compression
 *
 * @authors
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

#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "compress/lib.h"
#include "test/compress/common.h" // IWYU pragma: keep
#include "test_common.h"

#define MIN_COMP_LEVEL 1  ///< Minimum compression level for lz4
#define MAX_COMP_LEVEL 12 ///< Maximum compression level for lz4

struct Lz4ComprData;
void lz4_cdata_free(struct Lz4ComprData **ptr);

void test_compress_lz4(void)
{
  // ComprHandle *open(short level);
  // void *compress(ComprHandle *handle, const char *data, size_t dlen, size_t *clen);
  // void *decompress(ComprHandle *handle, const char *cbuf, size_t clen);
  // void close(ComprHandle **ptr);

  const struct ComprOps *compr_ops = compress_get_ops("lz4");
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

    struct Lz4ComprData *ptr = NULL;
    lz4_cdata_free(NULL);
    lz4_cdata_free(&ptr);
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
    TEST_CHECK(result == zeroes);

    const char ones[] = { 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
                          0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 };
    result = compr_ops->decompress(compr_handle, ones, sizeof(ones));
    TEST_CHECK(result == NULL);

    compr_ops->close(&compr_handle);
  }

  compress_data_tests(compr_ops, MIN_COMP_LEVEL, MAX_COMP_LEVEL);
}
