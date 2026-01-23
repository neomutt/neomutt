/**
 * @file
 * Test code for buf_tell()
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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

void test_buf_tell(void)
{
  // size_t buf_tell(const struct Buffer *buf);

  {
    TEST_CASE("buf_tell(NULL)");
    TEST_CHECK_NUM_EQ(buf_tell(NULL), 0);
  }

  {
    TEST_CASE("Empty buffer");
    struct Buffer *buf = buf_pool_get();
    TEST_CHECK_NUM_EQ(buf_tell(buf), 0);
    buf_pool_release(&buf);
  }

  {
    TEST_CASE("Buffer with content at start");
    struct Buffer *buf = buf_pool_get();
    buf_addstr(buf, "test");
    buf_seek(buf, 0);
    TEST_CHECK_NUM_EQ(buf_tell(buf), 0);
    buf_pool_release(&buf);
  }

  {
    TEST_CASE("Buffer with content at end");
    struct Buffer *buf = buf_pool_get();
    buf_addstr(buf, "hello");
    TEST_CHECK_NUM_EQ(buf_tell(buf), 5);
    buf_pool_release(&buf);
  }

  {
    TEST_CASE("Buffer with content at middle");
    struct Buffer *buf = buf_pool_get();
    buf_addstr(buf, "hello");
    buf_seek(buf, 2);
    TEST_CHECK_NUM_EQ(buf_tell(buf), 2);
    buf_pool_release(&buf);
  }

  {
    TEST_CASE("Uninitialized buffer");
    struct Buffer buf = { 0 };
    TEST_CHECK_NUM_EQ(buf_tell(&buf), 0);
  }

  {
    TEST_CASE("Buffer with data but no dptr");
    struct Buffer buf = { 0 };
    buf.data = "test";
    buf.dptr = NULL;
    buf.dsize = 5;
    TEST_CHECK_NUM_EQ(buf_tell(&buf), 0);
  }
}
