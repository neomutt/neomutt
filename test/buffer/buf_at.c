/**
 * @file
 * Test code for buf_at()
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
#include "test_common.h" // IWYU pragma: keep

void test_buf_at(void)
{
  // char buf_at(struct Buffer *buf, ssize_t offset);

  // Degenerate tests - NULL buffer
  {
    TEST_CHECK(buf_at(NULL, 0) == '\0');
    TEST_CHECK(buf_at(NULL, 1) == '\0');
    TEST_CHECK(buf_at(NULL, -1) == '\0');
  }

  // Empty buffer tests
  {
    struct Buffer *buf = buf_new("");

    TEST_CHECK(buf_at(buf, 0) == '\0');
    TEST_CHECK(buf_at(buf, 1) == '\0');
    TEST_CHECK(buf_at(buf, -1) == '\0');
    TEST_CHECK(buf_at(buf, -100) == '\0');

    buf_free(&buf);
  }

  // Positive offset tests with "foo"
  {
    struct Buffer *buf = buf_new("foo");

    TEST_CHECK(buf_at(buf, 0) == 'f');
    TEST_CHECK(buf_at(buf, 1) == 'o');
    TEST_CHECK(buf_at(buf, 2) == 'o');
    TEST_CHECK(buf_at(buf, 3) == '\0');  // null terminator
    TEST_CHECK(buf_at(buf, 4) == '\0');  // out of bounds
    TEST_CHECK(buf_at(buf, 10000) == '\0');  // far out of bounds

    buf_free(&buf);
  }

  // Negative offset tests with "foo"
  {
    struct Buffer *buf = buf_new("foo");

    TEST_CHECK(buf_at(buf, -1) == 'o');  // last char
    TEST_CHECK(buf_at(buf, -2) == 'o');  // second to last
    TEST_CHECK(buf_at(buf, -3) == 'f');  // first char (same as offset 0)
    TEST_CHECK(buf_at(buf, -4) == '\0'); // out of bounds (too negative)
    TEST_CHECK(buf_at(buf, -100) == '\0'); // far out of bounds

    buf_free(&buf);
  }

  // Single character buffer tests
  {
    struct Buffer *buf = buf_new("x");

    TEST_CHECK(buf_at(buf, 0) == 'x');
    TEST_CHECK(buf_at(buf, 1) == '\0');  // null terminator
    TEST_CHECK(buf_at(buf, -1) == 'x');  // last char (same as first)
    TEST_CHECK(buf_at(buf, -2) == '\0'); // out of bounds

    buf_free(&buf);
  }

  // Longer string tests with "hello"
  {
    struct Buffer *buf = buf_new("hello");

    // Positive offsets
    TEST_CHECK(buf_at(buf, 0) == 'h');
    TEST_CHECK(buf_at(buf, 4) == 'o');
    TEST_CHECK(buf_at(buf, 5) == '\0');  // null terminator

    // Negative offsets
    TEST_CHECK(buf_at(buf, -1) == 'o');  // last
    TEST_CHECK(buf_at(buf, -2) == 'l');  // second to last
    TEST_CHECK(buf_at(buf, -5) == 'h');  // first (same as offset 0)
    TEST_CHECK(buf_at(buf, -6) == '\0'); // out of bounds

    buf_free(&buf);
  }
}
