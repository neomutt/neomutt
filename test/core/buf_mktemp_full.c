/**
 * @file
 * Test code for buf_mktemp_full()
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "test_common.h"

void test_buf_mktemp_full(void)
{
  // void buf_mktemp_full(struct Buffer *buf, const char *prefix, const char *suffix, const char *src, int line);

  {
    struct Buffer *buf = buf_pool_get();
    buf_mktemp_full(buf, NULL, NULL, __FILE__, __LINE__);
    buf_pool_release(&buf);
  }
}
