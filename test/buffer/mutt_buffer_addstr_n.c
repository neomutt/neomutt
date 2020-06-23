/**
 * @file
 * Test code for mutt_buffer_addstr_n()
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

void test_mutt_buffer_addstr_n(void)
{
  // size_t mutt_buffer_addstr_n(struct Buffer *buf, const char *s, size_t len);

  {
    TEST_CHECK(mutt_buffer_addstr_n(NULL, "apple", 10) == 0);
  }

  {
    struct Buffer buf = mutt_buffer_make(0);
    TEST_CHECK(mutt_buffer_addstr_n(&buf, NULL, 10) == 0);
  }

  TEST_CASE("Adding to an empty Buffer");

  {
    const char *str = "a quick brown fox";
    const size_t len = strlen(str);
    size_t sizes[] = { 0, 5, len };

    for (size_t i = 0; i < mutt_array_size(sizes); i++)
    {
      TEST_CASE_("%ld", sizes[i]);
      struct Buffer buf = mutt_buffer_make(0);
      TEST_CHECK(mutt_buffer_addstr_n(&buf, str, sizes[i]) == sizes[i]);
      TEST_CHECK(strlen(mutt_b2s(&buf)) == MIN(len, sizes[i]));
      TEST_CHECK(mutt_strn_equal(mutt_b2s(&buf), str, sizes[i]));
      mutt_buffer_dealloc(&buf);
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
      struct Buffer buf = mutt_buffer_make(0);
      mutt_buffer_addstr(&buf, base);
      TEST_CHECK(mutt_buffer_addstr_n(&buf, str, sizes[i]) == sizes[i]);
      TEST_CHECK(strlen(mutt_b2s(&buf)) == (base_len + MIN(len, sizes[i])));
      TEST_CHECK(mutt_strn_equal(mutt_b2s(&buf), combined, base_len + sizes[i]));
      mutt_buffer_dealloc(&buf);
    }
  }
}
