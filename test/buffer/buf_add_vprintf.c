/**
 * @file
 * Test code for buf_add_vprintf()
 *
 * @authors
 * Copyright (C) 2026 Syed Mohammed Nayyar <jmestwa@gmail.com>
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
#include <stdarg.h>
#include <string.h>
#include "mutt/lib.h"
#include "test_common.h"

static int add_vprintf(struct Buffer *buf, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int len = buf_add_vprintf(buf, fmt, ap);
  va_end(ap);
  return len;
}

void test_buf_add_vprintf(void)
{
  // int buf_add_vprintf(struct Buffer *buf, const char *fmt, va_list ap);

  {
    TEST_CHECK(add_vprintf(NULL, "apple") == -1);
  }

  {
    struct Buffer *buf = buf_pool_get();
    TEST_CHECK(add_vprintf(buf, NULL) == -1);
    buf_pool_release(&buf);
  }

  {
    TEST_CASE("printf to an empty Buffer");
    struct Buffer *buf = buf_pool_get();
    TEST_CHECK(add_vprintf(buf, "%.3s %d %3.4f", "apple", 1234567, 3.141592654) == 18);
    TEST_CHECK_STR_EQ(buf_string(buf), "app 1234567 3.1416");
    buf_pool_release(&buf);
  }

  {
    TEST_CASE("printf to a non-empty Buffer");
    struct Buffer *buf = buf_pool_get();
    buf_addstr(buf, "test");
    TEST_CHECK(add_vprintf(buf, "%.3s %d %3.4f", "apple", 1234567, 3.141592654) == 18);
    TEST_CHECK_STR_EQ(buf_string(buf), "testapp 1234567 3.1416");
    buf_pool_release(&buf);
  }

  {
    TEST_CASE("Grows past the initial allocation");
    char big[2048];
    memset(big, 'x', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    struct Buffer *buf = buf_pool_get();
    buf_addstr(buf, "test");
    TEST_CHECK(add_vprintf(buf, "%s!", big) == (int) sizeof(big));
    TEST_CHECK(buf_len(buf) == sizeof(big) + 4);
    TEST_CHECK(buf_string(buf)[buf_len(buf) - 1] == '!');
    buf_pool_release(&buf);
  }
}
