/**
 * @file
 * Test code for buf_pool_release()
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

void test_buf_pool_release(void)
{
  // void buf_pool_release(struct Buffer **pbuf);

  {
    buf_pool_release(NULL);
    TEST_CHECK_(1, "buf_pool_release(NULL)");
  }

  {
    struct Buffer *buf = NULL;
    buf_pool_release(&buf);
    TEST_CHECK_(1, "buf_pool_release(&buf)");
  }

  {
    static const char *large = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    struct Buffer *buf = buf_pool_get();
    TEST_CHECK(buf != NULL);

    // Make the buffer HUGE
    for (int i = 0; i < 256; i++)
      buf_addstr(buf, large);

    buf_pool_release(&buf);
    TEST_CHECK_(1, "buf_pool_release(&buf)");
  }
}
