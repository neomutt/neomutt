/**
 * @file
 * Test code for mutt_b64_buffer_encode()
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

void test_mutt_b64_buffer_encode(void)
{
  // size_t mutt_b64_buffer_encode(struct Buffer *buf, const char *in, size_t len);

  {
    TEST_CHECK(mutt_b64_buffer_encode(NULL, "apple", 10) == 0);
  }

  {
    struct Buffer buf = mutt_buffer_make(0);
    TEST_CHECK(mutt_b64_buffer_encode(&buf, NULL, 10) == 0);
    mutt_buffer_dealloc(&buf);
  }
}
