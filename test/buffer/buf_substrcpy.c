/**
 * @file
 * Test code for buf_substrcpy()
 *
 * @authors
 * Copyright (C) 2020-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
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
#include "test_common.h"

void test_buf_substrcpy(void)
{
  // size_t buf_substrcpy(struct Buffer *buf, const char *beg, const char *end);

  {
    TEST_CHECK(buf_substrcpy(NULL, NULL, NULL) == 0);
  }

  {
    struct Buffer *buf = buf_pool_get();
    const char *str = "apple banana";
    TEST_CHECK(buf_substrcpy(buf, str, NULL) == 0);
    buf_pool_release(&buf);
  }

  {
    struct Buffer *buf = buf_pool_get();
    const char *str = "apple banana";
    TEST_CHECK(buf_substrcpy(buf, NULL, str) == 0);
    buf_pool_release(&buf);
  }

  {
    struct Buffer *buf = buf_pool_get();
    const char *str = "apple banana";
    TEST_CHECK(buf_substrcpy(buf, str + 8, str + 2) == 0);
    buf_pool_release(&buf);
  }

  {
    char *src = "abcdefghijklmnopqrstuvwxyz";
    char *result = "jklmnopqr";

    struct Buffer *buf = buf_pool_get();

    size_t len = buf_substrcpy(buf, src + 9, src + 18);

    TEST_CHECK(len == 9);
    TEST_CHECK_STR_EQ(buf_string(buf), result);

    buf_pool_release(&buf);
  }
}
