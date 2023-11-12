/**
 * @file
 * Test code for mutt_b64_buffer_decode()
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
#include <stddef.h>
#include "mutt/lib.h"
#include "test_common.h"

void test_mutt_b64_buffer_decode(void)
{
  // int mutt_b64_buffer_decode(struct Buffer *buf, const char *in);

  {
    TEST_CHECK(mutt_b64_buffer_decode(NULL, "apple") != 0);
  }

  {
    struct Buffer *buf = buf_pool_get();
    TEST_CHECK(mutt_b64_buffer_decode(buf, NULL) != 0);
    buf_pool_release(&buf);
  }

  {
    static const char clear[] = "Hello";
    static const char encoded[] = "SGVsbG8=";

    struct Buffer *buf = buf_pool_get();

    int declen = mutt_b64_buffer_decode(buf, encoded);
    TEST_CHECK(declen == 5);
    TEST_CHECK_STR_EQ(buf_string(buf), clear);
    buf_pool_release(&buf);
  }
}
