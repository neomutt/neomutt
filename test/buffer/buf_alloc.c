/**
 * @file
 * Test code for buf_alloc()
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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

void test_buf_alloc(void)
{
  // void buf_alloc(struct Buffer *buf, size_t new_size);

  {
    buf_alloc(NULL, 10);
    TEST_CHECK_(1, "buf_alloc(NULL, 10)");
  }

  {
    struct Buffer buf = buf_make(0);
    buf_alloc(&buf, 10);
    TEST_CHECK_(1, "buf_alloc(buf, 10)");
    buf_dealloc(&buf);
  }

  {
    const int orig_size = 64;
    static int sizes[][2] = {
      { 0, 128 },
      { 32, 128 },
      { 128, 128 },
      { 129, 256 },
    };

    for (size_t i = 0; i < mutt_array_size(sizes); i++)
    {
      struct Buffer buf = buf_make(0);
      buf_alloc(&buf, orig_size);
      TEST_CASE_("%d", sizes[i][0]);
      buf_alloc(&buf, sizes[i][0]);
      if (!TEST_CHECK(buf.dsize == sizes[i][1]))
      {
        TEST_MSG("Expected: %ld", sizes[i][1]);
        TEST_MSG("Actual  : %ld", buf.dsize);
      }
      buf_dealloc(&buf);
    }
  }
}
