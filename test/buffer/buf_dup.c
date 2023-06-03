/**
 * @file
 * Test code for buf_dup()
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

void test_buf_dup(void)
{
  // struct Buffer *buf_dup(const struct Buffer *buf);

  {
    // Degenerate tests
    TEST_CHECK(buf_dup(NULL) == NULL);
  }

  {
    struct Buffer *a = buf_new("foo");
    struct Buffer *b = buf_dup(a);

    TEST_CHECK(b != NULL);
    TEST_CHECK_STR_EQ(buf_string(a), buf_string(b));

    buf_free(&a);
    buf_free(&b);
  }
}
