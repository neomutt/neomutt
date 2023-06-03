/**
 * @file
 * Test code for buf_coll()
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

void test_buf_coll(void)
{
  // int buf_coll(struct Buffer *a, struct Buffer *b);

  {
    // Degenerate tests
    TEST_CHECK(buf_coll(NULL, NULL) == 0);

    struct Buffer *a = buf_new("apple");
    struct Buffer *b = buf_new("banana");

    TEST_CHECK(buf_coll(a, NULL) > 0);
    TEST_CHECK(buf_coll(NULL, b) < 0);

    buf_free(&a);
    buf_free(&b);
  }

  {
    struct Buffer *a = buf_new("foo");
    struct Buffer *b = buf_new("foo");
    struct Buffer *c = buf_new("FOO");
    struct Buffer *d = buf_new("foo2");

    TEST_CHECK(buf_coll(a, b) == 0);
    TEST_CHECK(buf_coll(a, c) > 0);
    TEST_CHECK(buf_coll(a, d) < 0);

    buf_free(&a);
    buf_free(&b);
    buf_free(&c);
    buf_free(&d);
  }
}
