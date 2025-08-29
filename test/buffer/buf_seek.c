/**
 * @file
 * Test code for buf_seek()
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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

void test_buf_seek(void)
{
  // void buf_seek(struct Buffer *buf, size_t offset);

  {
    TEST_CASE("buf_seek(NULL, 0);");
    buf_seek(NULL, 0);
  }

  {
    struct Buffer *buf = buf_pool_get();
    TEST_CASE("buf_seek(buf, 0);");
    buf_seek(buf, 0);
    buf_pool_release(&buf);
  }

  {
    struct Buffer *buf = buf_pool_get();
    const char *str = "apple";
    buf_addstr(buf, str);
    TEST_CASE("buf_seek(buf, 0);");
    TEST_CHECK(buf->dptr != buf->data);
    buf_seek(buf, 0);
    TEST_CHECK(buf->dptr == buf->data);
    buf_pool_release(&buf);
  }
}
