/**
 * @file
 * Test code for mutt_buffer_copy()
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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

void test_mutt_buffer_copy(void)
{
  // size_t mutt_buffer_copy(struct Buffer *dst, const struct Buffer *src);

  {
    TEST_CHECK(mutt_buffer_copy(NULL, NULL) == 0);
  }

  {
    struct Buffer buf1 = mutt_buffer_make(0);
    struct Buffer buf2 = mutt_buffer_make(0);

    size_t len = mutt_buffer_copy(&buf2, &buf1);

    TEST_CHECK(len == 0);
    TEST_CHECK(mutt_buffer_is_empty(&buf2) == true);

    mutt_buffer_dealloc(&buf1);
    mutt_buffer_dealloc(&buf2);
  }

  {
    char *src = "abcdefghij";

    struct Buffer buf1 = mutt_buffer_make(32);
    struct Buffer buf2 = mutt_buffer_make(0);

    mutt_buffer_strcpy(&buf1, src);

    size_t len = mutt_buffer_copy(&buf2, &buf1);

    TEST_CHECK(len == 10);
    TEST_CHECK(mutt_str_equal(mutt_b2s(&buf1), mutt_b2s(&buf2)));

    mutt_buffer_dealloc(&buf1);
    mutt_buffer_dealloc(&buf2);
  }
}
