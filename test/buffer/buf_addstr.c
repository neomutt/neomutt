/**
 * @file
 * Test code for buf_addstr()
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
#include <string.h>
#include "mutt/lib.h"
#include "test_common.h"

void test_buf_addstr(void)
{
  // size_t buf_addstr(struct Buffer *buf, const char *s);

  {
    TEST_CHECK(buf_addstr(NULL, "apple") == 0);
  }

  {
    struct Buffer *buf = buf_pool_get();
    TEST_CHECK(buf_addstr(buf, NULL) == 0);
    buf_pool_release(&buf);
  }

  {
    struct Buffer *buf = buf_pool_get();
    TEST_CHECK(buf_addstr(buf, "apple") == 5);
    TEST_CHECK_STR_EQ(buf_string(buf), "apple");
    buf_pool_release(&buf);
  }

  {
    struct Buffer *buf = buf_pool_get();
    buf_addstr(buf, "test");
    TEST_CHECK(buf_addstr(buf, "apple") == 5);
    TEST_CHECK_STR_EQ(buf_string(buf), "testapple");
    buf_pool_release(&buf);
  }
}
