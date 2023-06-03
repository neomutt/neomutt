/**
 * @file
 * Test code for buf_free()
 *
 * @authors
 * Copyright (C) 2023 Anna Figueiredo Gomes <navi@vlhl.dev>
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

void test_buf_free(void)
{
  // void buf_free(struct Buffer **ptr);

  {
    // Degenerate tests
    buf_free(NULL);
    TEST_CHECK_(1, "buf_free(NULL)");
    struct Buffer *ptr = NULL;
    buf_free(&ptr);
    TEST_CHECK_(1, "buf_free(&ptr)");
  }

  {
    struct Buffer *buf = buf_new("foo");

    buf_free(&buf);
    TEST_CHECK(buf == NULL);
  }
}
