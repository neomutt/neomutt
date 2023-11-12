/**
 * @file
 * Test code for buf_concatn_path()
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
#include "test_common.h"

void test_buf_concatn_path(void)
{
  // size_t buf_concatn_path(struct Buffer *buf, const char *dir, size_t dirlen, const char *fname, size_t fnamelen);

  {
    TEST_CHECK(buf_concatn_path(NULL, NULL, 0, NULL, 0) == 0);
  }

  {
    struct Buffer *buf = buf_pool_get();

    const char *dir = "/home/jim/work";
    const char *file = "file.txt";
    const char *result = "/home/jim/file";

    size_t len = buf_concatn_path(buf, dir, 9, file, 4);

    TEST_CHECK(len == 14);
    TEST_CHECK_STR_EQ(buf_string(buf), result);

    buf_pool_release(&buf);
  }
}
