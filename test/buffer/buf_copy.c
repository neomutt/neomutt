/**
 * @file
 * Test code for buf_copy()
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
#include <stdbool.h>
#include "mutt/lib.h"
#include "test_common.h"

void test_buf_copy(void)
{
  // size_t buf_copy(struct Buffer *dst, const struct Buffer *src);

  {
    TEST_CHECK(buf_copy(NULL, NULL) == 0);
  }

  {
    struct Buffer *buf1 = buf_pool_get();
    struct Buffer *buf2 = buf_pool_get();

    size_t len = buf_copy(buf2, buf1);

    TEST_CHECK(len == 0);
    TEST_CHECK(buf_is_empty(buf2) == true);

    buf_pool_release(&buf1);
    buf_pool_release(&buf2);
  }

  {
    char *src = "abcdefghij";

    struct Buffer *buf1 = buf_pool_get();
    struct Buffer *buf2 = buf_pool_get();

    buf_strcpy(buf1, src);

    size_t len = buf_copy(buf2, buf1);

    TEST_CHECK(len == 10);
    TEST_CHECK_STR_EQ(buf_string(buf2), buf_string(buf1));

    buf_pool_release(&buf1);
    buf_pool_release(&buf2);
  }
}
