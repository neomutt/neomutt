/**
 * @file
 * Test code for buf_reset()
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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

void test_buf_reset(void)
{
  // void buf_reset(struct Buffer *buf);

  {
    buf_reset(NULL);
    TEST_CHECK_(1, "buf_reset(NULL)");
  }

  {
    struct Buffer *buf = buf_pool_get();
    buf_reset(buf);
    TEST_CHECK_(1, "buf_reset(buf)");
    buf_pool_release(&buf);
  }

  {
    struct Buffer *buf = buf_pool_get();
    buf_addstr(buf, "test");
    buf_reset(buf);
    TEST_CHECK_(1, "buf_reset(buf)");
    buf_pool_release(&buf);
  }
}
