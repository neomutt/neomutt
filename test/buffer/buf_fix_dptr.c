/**
 * @file
 * Test code for buf_fix_dptr()
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

void test_buf_fix_dptr(void)
{
  // void buf_fix_dptr(struct Buffer *buf);

  {
    buf_fix_dptr(NULL);
    TEST_CHECK_(1, "buf_fix_dptr(NULL)");
  }

  {
    struct Buffer *buf = buf_pool_get();
    buf_fix_dptr(buf);
    TEST_CHECK(buf_is_empty(buf));
    buf_pool_release(&buf);
  }

  {
    const char *str = "a quick brown fox";
    struct Buffer *buf = buf_pool_get();
    buf_addstr(buf, str);
    buf_fix_dptr(buf);
    TEST_CHECK(buf_len(buf) == (strlen(str)));
    buf_pool_release(&buf);
  }
}
