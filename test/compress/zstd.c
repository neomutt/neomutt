/**
 * @file
 * Test code for zstd compression
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

#define MIN_COMP_LEVEL 1  ///< Minimum compression level for zstd
#define MAX_COMP_LEVEL 22 ///< Maximum compression level for zstd

void test_compress_zstd(void)
{
  // void *open(short level);
  // void *compress(void *cctx, const char *data, size_t dlen, size_t *clen);
  // void *decompress(void *cctx, const char *cbuf, size_t clen);
  // void  close(void **cctx);

  const struct ComprOps *cops = compress_get_ops("zstd");
  if (!TEST_CHECK(cops != NULL))
    return;

  {
    // Degenerate tests
    TEST_CHECK(cops->compress(NULL, NULL, 0, NULL) == NULL);
    TEST_CHECK(cops->decompress(NULL, NULL, 0) == NULL);
    void *cctx = NULL;
    cops->close(NULL);
    TEST_CHECK_(1, "cops->close(NULL)");
    cops->close(&cctx);
    TEST_CHECK_(1, "cops->close(&cctx)");
  }

  {
    // Temporarily disable logging
    MuttLogger = log_disp_null;

    void *cctx = cops->open(MIN_COMP_LEVEL - 1);
    TEST_CHECK(cctx != NULL);
    cops->close(&cctx);

    cctx = cops->open(MAX_COMP_LEVEL + 1);
    TEST_CHECK(cctx != NULL);
    cops->close(&cctx);

    // Restore logging
    MuttLogger = log_disp_terminal;
  }

  {
    // Garbage data
    void *cctx = cops->open(MIN_COMP_LEVEL);
    TEST_CHECK(cctx != NULL);

    const char zeroes[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    void *result = cops->decompress(cctx, zeroes, sizeof(zeroes));
    TEST_CHECK(result == NULL);

    cops->close(&cctx);
  }

  compress_data_tests(cops, MIN_COMP_LEVEL, MAX_COMP_LEVEL);
}
