/**
 * @file
 * Test code for buf_addstr_n()
 *
 * @authors
 * Copyright (C) 2019-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
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

void test_buf_addstr_n(void)
{
  // size_t buf_addstr_n(struct Buffer *buf, const char *s, size_t len);

  {
    TEST_CHECK(buf_addstr_n(NULL, "apple", 10) == 0);
  }

  {
    struct Buffer *buf = buf_pool_get();
    TEST_CHECK(buf_addstr_n(buf, NULL, 10) == 0);
    buf_pool_release(&buf);
  }

  TEST_CASE("Adding to an empty Buffer");

  {
    const char *str = "a quick brown fox";
    const size_t len = strlen(str);
    size_t sizes[] = { 0, 5, len };

    for (size_t i = 0; i < mutt_array_size(sizes); i++)
    {
      TEST_CASE_("%ld", sizes[i]);
      struct Buffer *buf = buf_pool_get();
      TEST_CHECK(buf_addstr_n(buf, str, sizes[i]) == sizes[i]);
      TEST_CHECK(strlen(buf_string(buf)) == MIN(len, sizes[i]));
      TEST_CHECK(mutt_strn_equal(buf_string(buf), str, sizes[i]));
      buf_pool_release(&buf);
    }
  }

  TEST_CASE("Adding to a non-empty Buffer");

  {
    const char *base = "test";
    const size_t base_len = strlen(base);
    const char *str = "a quick brown fox";
    const size_t len = strlen(str);
    const char *combined = "testa quick brown fox";
    size_t sizes[] = { 0, 5, len };

    for (size_t i = 0; i < mutt_array_size(sizes); i++)
    {
      TEST_CASE_("%ld", sizes[i]);
      struct Buffer *buf = buf_pool_get();
      buf_addstr(buf, base);
      TEST_CHECK(buf_addstr_n(buf, str, sizes[i]) == sizes[i]);
      TEST_CHECK(strlen(buf_string(buf)) == (base_len + MIN(len, sizes[i])));
      TEST_CHECK(mutt_strn_equal(buf_string(buf), combined, base_len + sizes[i]));
      buf_pool_release(&buf);
    }
  }
}
