/**
 * @file
 * Test code for buf_str_equal()
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
#include <stdbool.h>
#include <string.h>
#include "mutt/lib.h"
#include "test_common.h" // IWYU pragma: keep

void test_buf_str_equal(void)
{
  // bool buf_str_equal(struct Buffer *a, struct Buffer *b);

  {
    // Degenerate tests
    TEST_CHECK(buf_str_equal(NULL, NULL) == true);

    struct Buffer *a = buf_new("foo");
    struct Buffer *b = buf_new("foo");

    TEST_CHECK(buf_str_equal(a, NULL) == false);
    TEST_CHECK(buf_str_equal(NULL, b) == false);

    buf_free(&a);
    buf_free(&b);
  }

  {
    struct Buffer *a = buf_new("foo");
    struct Buffer *b = buf_new("foo");

    TEST_CHECK(buf_str_equal(a, b) == true);

    buf_free(&a);
    buf_free(&b);
  }

  {
    struct Buffer *a = buf_new("foobar");
    struct Buffer *b = buf_new("foo");

    TEST_CHECK(buf_str_equal(a, b) == false);

    buf_free(&a);
    buf_free(&b);
  }

  {
    struct Buffer *a = buf_new("foo");
    struct Buffer *b = buf_new("foobar");

    TEST_CHECK(buf_str_equal(a, b) == false);

    buf_free(&a);
    buf_free(&b);
  }

  {
    struct Buffer *a = buf_new("foo");
    struct Buffer *b = buf_new("bar");

    TEST_CHECK(buf_str_equal(a, b) == false);

    buf_free(&a);
    buf_free(&b);
  }

  {
    struct Buffer *a = buf_new("foo");
    struct Buffer *b = buf_new("Foo");

    TEST_CHECK(buf_str_equal(a, b) == false);

    buf_free(&a);
    buf_free(&b);
  }
}
