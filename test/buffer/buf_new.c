/**
 * @file
 * Test code for buf_new()
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

void test_buf_new(void)
{
  // struct Buffer *buf_new(const char *str);

  {
    // Degenerate tests
    struct Buffer *buf = buf_new(NULL);
    TEST_CHECK(buf != NULL);
    TEST_CHECK_STR_EQ(buf_string(buf), "");
    buf_free(&buf);
  }

  {
    struct Buffer *buf = buf_new("apple");
    TEST_CHECK(buf != NULL);
    TEST_CHECK_STR_EQ(buf_string(buf), "apple");
    buf_free(&buf);
  }
}
