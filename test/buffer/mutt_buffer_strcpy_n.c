/**
 * @file
 * Test code for mutt_buffer_strcpy_n()
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

void test_mutt_buffer_strcpy_n(void)
{
  // void mutt_buffer_strcpy_n(struct Buffer *buf, const char *s, size_t len);

  {
    mutt_buffer_strcpy_n(NULL, "apple", 3);
    TEST_CHECK_(1, "mutt_buffer_strcpy_n(NULL, \"apple\", 3)");
  }

  {
    struct Buffer buf = mutt_buffer_make(0);
    mutt_buffer_strcpy_n(&buf, NULL, 3);
    TEST_CHECK_(1, "mutt_buffer_strcpy_n(&buf, NULL, 3)");
  }

  TEST_CASE("Copy to an empty Buffer");

  {
    const char *str = "a quick brown fox";
    const size_t len = strlen(str);
    size_t sizes[] = { 0, 5, len };

    for (size_t i = 0; i < mutt_array_size(sizes); i++)
    {
      TEST_CASE_("%ld", sizes[i]);
      struct Buffer buf = mutt_buffer_make(0);
      mutt_buffer_strcpy_n(&buf, str, sizes[i]);
      TEST_CHECK(strlen(mutt_b2s(&buf)) == MIN(len, sizes[i]));
      TEST_CHECK(mutt_strn_equal(mutt_b2s(&buf), str, sizes[i]));
      mutt_buffer_dealloc(&buf);
    }
  }

  TEST_CASE("Copy to a non-empty Buffer");

  {
    const char *base = "test";
    const char *str = "a quick brown fox";
    const size_t len = strlen(str);
    size_t sizes[] = { 0, 5, len };

    for (size_t i = 0; i < mutt_array_size(sizes); i++)
    {
      TEST_CASE_("%ld", sizes[i]);
      struct Buffer buf = mutt_buffer_make(0);
      mutt_buffer_addstr(&buf, base);
      mutt_buffer_strcpy_n(&buf, str, sizes[i]);
      TEST_CHECK(strlen(mutt_b2s(&buf)) == MIN(len, sizes[i]));
      TEST_CHECK(mutt_strn_equal(mutt_b2s(&buf), str, sizes[i]));
      mutt_buffer_dealloc(&buf);
    }
  }
}
