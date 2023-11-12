/**
 * @file
 * Test code for buf_add_printf()
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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

void test_buf_add_printf(void)
{
  // int buf_add_printf(struct Buffer *buf, const char *fmt, ...);

  {
    TEST_CHECK(buf_add_printf(NULL, "apple") != 0);
  }

  {
    struct Buffer *buf = buf_pool_get();
    TEST_CHECK(buf_add_printf(buf, NULL) != 0);
    buf_pool_release(&buf);
  }

  TEST_CASE("printf to an empty Buffer");

  {
    TEST_CASE("Varargs");
    const char *str = "apple";
    const char *result = "app 1234567 3.1416";
    struct Buffer *buf = buf_pool_get();
    TEST_CHECK(buf_add_printf(buf, "%.3s %d %3.4f", str, 1234567, 3.141592654) == 18);
    TEST_CHECK_STR_EQ(buf_string(buf), result);
    buf_pool_release(&buf);
  }

  TEST_CASE("printf to a non-empty Buffer");

  {
    TEST_CASE("Static very big");
    const char *str = "%d apple banana cherry damson elderberry fig guava hawthorn ilama jackfruit kumquat lemon mango nectarine olive papaya quince raspberry strawberry tangerine ugli vanilla wolfberry xigua yew ziziphus";
    struct Buffer *buf = buf_pool_get();
    buf_addstr(buf, "test");

    for (int i = 0; i < 50; i++)
      TEST_CHECK(buf_add_printf(buf, str, 1) != -1);

    buf_pool_release(&buf);
  }

  {
    TEST_CASE("Varargs");
    const char *str = "apple";
    const char *result = "testapp 1234567 3.1416";
    struct Buffer *buf = buf_pool_get();
    buf_addstr(buf, "test");
    TEST_CHECK(buf_add_printf(buf, "%.3s %d %3.4f", str, 1234567, 3.141592654) == 18);
    TEST_CHECK_STR_EQ(buf_string(buf), result);
    buf_pool_release(&buf);
  }
}
