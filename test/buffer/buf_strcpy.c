/**
 * @file
 * Test code for buf_strcpy()
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

void test_buf_strcpy(void)
{
  // void buf_strcpy(struct Buffer *buf, const char *s);

  {
    buf_strcpy(NULL, "apple");
    TEST_CHECK_(1, "buf_strcpy(NULL, \"apple\")");
  }

  {
    struct Buffer buf = buf_make(0);
    buf_strcpy(&buf, NULL);
    TEST_CHECK_(1, "buf_strcpy(&buf, NULL)");
  }

  TEST_CASE("Copy to an empty Buffer");

  {
    TEST_CASE("Empty");
    struct Buffer buf = buf_make(0);
    buf_strcpy(&buf, "");
    TEST_CHECK(mutt_str_equal(buf_string(&buf), ""));
    buf_dealloc(&buf);
  }

  {
    TEST_CASE("String");
    const char *str = "test";
    struct Buffer buf = buf_make(0);
    buf_strcpy(&buf, str);
    TEST_CHECK(mutt_str_equal(buf_string(&buf), str));
    buf_dealloc(&buf);
  }

  TEST_CASE("Overwrite a non-empty Buffer");

  {
    TEST_CASE("Empty");
    struct Buffer buf = buf_make(0);
    buf_addstr(&buf, "test");
    buf_strcpy(&buf, "");
    TEST_CHECK(mutt_str_equal(buf_string(&buf), ""));
    buf_dealloc(&buf);
  }

  {
    TEST_CASE("String");
    const char *str = "apple";
    struct Buffer buf = buf_make(0);
    buf_addstr(&buf, "test");
    buf_strcpy(&buf, str);
    TEST_CHECK(mutt_str_equal(buf_string(&buf), str));
    buf_dealloc(&buf);
  }
}
