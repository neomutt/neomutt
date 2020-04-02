/**
 * @file
 * Test code for mutt_buffer_add_printf()
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
#include "mutt/lib.h"

void test_mutt_buffer_add_printf(void)
{
  // int mutt_buffer_add_printf(struct Buffer *buf, const char *fmt, ...);

  {
    TEST_CHECK(mutt_buffer_add_printf(NULL, "apple") != 0);
  }

  {
    struct Buffer buf = mutt_buffer_make(0);
    TEST_CHECK(mutt_buffer_add_printf(&buf, NULL) != 0);
  }

  TEST_CASE("printf to an empty Buffer");

  {
    TEST_CASE("Empty");
    struct Buffer buf = mutt_buffer_make(0);
    TEST_CHECK(mutt_buffer_add_printf(&buf, "") == 0);
    TEST_CHECK(strlen(mutt_b2s(&buf)) == 0);
    mutt_buffer_dealloc(&buf);
  }

  {
    TEST_CASE("Static");
    const char *str = "apple";
    struct Buffer buf = mutt_buffer_make(0);
    TEST_CHECK(mutt_buffer_add_printf(&buf, str) == 5);
    TEST_CHECK(strcmp(mutt_b2s(&buf), str) == 0);
    mutt_buffer_dealloc(&buf);
  }

  {
    TEST_CASE("Static big");
    const char *str =
        "apple banana cherry damson elderberry fig guava hawthorn ilama "
        "jackfruit kumquat lemon mango nectarine olive papaya quince raspberry "
        "strawberry tangerine ugli vanilla wolfberry xigua yew ziziphus";
    struct Buffer buf = mutt_buffer_make(0);
    TEST_CHECK(mutt_buffer_add_printf(&buf, str) == 195);
    TEST_CHECK(strcmp(mutt_b2s(&buf), str) == 0);
    mutt_buffer_dealloc(&buf);
  }

  {
    TEST_CASE("Varargs");
    const char *str = "apple";
    const char *result = "app 1234567 3.1416";
    struct Buffer buf = mutt_buffer_make(0);
    TEST_CHECK(mutt_buffer_add_printf(&buf, "%.3s %ld %3.4f", str, 1234567, 3.141592654) == 18);
    TEST_CHECK(strcmp(mutt_b2s(&buf), result) == 0);
    mutt_buffer_dealloc(&buf);
  }

  TEST_CASE("printf to a non-empty Buffer");

  {
    TEST_CASE("Empty");
    const char *str = "test";
    struct Buffer buf = mutt_buffer_make(0);
    mutt_buffer_addstr(&buf, str);
    TEST_CHECK(mutt_buffer_add_printf(&buf, "") == 0);
    TEST_CHECK(strcmp(mutt_b2s(&buf), str) == 0);
    mutt_buffer_dealloc(&buf);
  }

  {
    TEST_CASE("Static");
    const char *str = "apple";
    const char *result = "testapple";
    struct Buffer buf = mutt_buffer_make(0);
    mutt_buffer_addstr(&buf, "test");
    TEST_CHECK(mutt_buffer_add_printf(&buf, str) == 5);
    TEST_CHECK(strcmp(mutt_b2s(&buf), result) == 0);
    mutt_buffer_dealloc(&buf);
  }

  {
    TEST_CASE("Static big");
    const char *str =
        "apple banana cherry damson elderberry fig guava hawthorn ilama "
        "jackfruit kumquat lemon mango nectarine olive papaya quince raspberry "
        "strawberry tangerine ugli vanilla wolfberry xigua yew ziziphus";
    const char *result =
        "testapple banana cherry damson elderberry fig guava hawthorn ilama "
        "jackfruit kumquat lemon mango nectarine olive papaya quince raspberry "
        "strawberry tangerine ugli vanilla wolfberry xigua yew ziziphus";
    struct Buffer buf = mutt_buffer_make(0);
    mutt_buffer_addstr(&buf, "test");
    TEST_CHECK(mutt_buffer_add_printf(&buf, str) == 195);
    TEST_CHECK(strcmp(mutt_b2s(&buf), result) == 0);
    mutt_buffer_dealloc(&buf);
  }

  {
    TEST_CASE("Varargs");
    const char *str = "apple";
    const char *result = "testapp 1234567 3.1416";
    struct Buffer buf = mutt_buffer_make(0);
    mutt_buffer_addstr(&buf, "test");
    TEST_CHECK(mutt_buffer_add_printf(&buf, "%.3s %ld %3.4f", str, 1234567, 3.141592654) == 18);
    TEST_CHECK(strcmp(mutt_b2s(&buf), result) == 0);
    mutt_buffer_dealloc(&buf);
  }
}
