/**
 * @file
 * Test code for buf_find_string()
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

void test_buf_find_string(void)
{
  // const char *buf_find_string(struct Buffer *buf, const char *s);

  {
    // Degenerate tests
    struct Buffer *buf = buf_new("foo bar");

    TEST_CHECK(buf_find_string(NULL, "foo") == NULL);
    TEST_CHECK(buf_find_string(buf, NULL) == NULL);

    buf_free(&buf);
  }

  {
    struct Buffer *buf = buf_new("foo bar");

    TEST_CHECK_STR_EQ(buf_find_string(buf, "foo"), "foo bar");
    TEST_CHECK_STR_EQ(buf_find_string(buf, "bar"), "bar");
    TEST_CHECK_STR_EQ(buf_find_string(buf, "baz"), NULL);

    buf_free(&buf);
  }
}
