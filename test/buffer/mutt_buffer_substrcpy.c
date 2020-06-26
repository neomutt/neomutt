/**
 * @file
 * Test code for mutt_buffer_substrcpy()
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

void test_mutt_buffer_substrcpy(void)
{
  // size_t mutt_buffer_substrcpy(struct Buffer *buf, const char *beg, const char *end);

  {
    TEST_CHECK(mutt_buffer_substrcpy(NULL, NULL, NULL) == 0);
  }

  {
    char *src = "abcdefghijklmnopqrstuvwxyz";
    char *result = "jklmnopqr";

    struct Buffer buf = mutt_buffer_make(32);

    size_t len = mutt_buffer_substrcpy(&buf, src + 9, src + 18);

    TEST_CHECK(len == 9);
    TEST_CHECK(mutt_str_equal(mutt_b2s(&buf), result));

    mutt_buffer_dealloc(&buf);
  }
}
