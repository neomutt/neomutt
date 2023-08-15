/**
 * @file
 * Test code for buf_startswith()
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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

void test_buf_startswith(void)
{
  // size_t buf_startswith(struct Buffer *buf, const char *prefix);

  {
    // Degenerate tests
    TEST_CHECK(buf_startswith(NULL, NULL) == 0);

    struct Buffer *a = buf_new("apple");
    const char *b = "apple";

    TEST_CHECK(buf_startswith(a, NULL) == 0);
    TEST_CHECK(buf_startswith(NULL, b) == 0);

    buf_free(&a);
  }

  {
    struct Buffer *a = buf_new("");
    const char *b = "apple";

    TEST_CHECK(buf_startswith(a, b) == 0);

    buf_free(&a);
  }

  {
    struct Buffer *a = buf_new("apple");
    const char *b = "";

    TEST_CHECK(buf_startswith(a, b) == 0);

    buf_free(&a);
  }

  {
    struct Buffer *a = buf_new("applebanana");
    const char *b = "apple";

    TEST_CHECK(buf_startswith(a, b) == 5);

    buf_free(&a);
  }

  {
    struct Buffer *a = buf_new("APPLEbanana");
    const char *b = "apple";

    TEST_CHECK(buf_startswith(a, b) == 0);

    buf_free(&a);
  }
}
