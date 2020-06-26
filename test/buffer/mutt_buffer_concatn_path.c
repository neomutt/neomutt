/**
 * @file
 * Test code for mutt_buffer_concatn_path()
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

void test_mutt_buffer_concatn_path(void)
{
  // size_t mutt_buffer_concatn_path(struct Buffer *buf, const char *dir, size_t dirlen, const char *fname, size_t fnamelen);

  {
    TEST_CHECK(mutt_buffer_concatn_path(NULL, NULL, 0, NULL, 0) == 0);
  }

  {
    struct Buffer buf = mutt_buffer_make(0);

    const char *dir = "/home/jim/work";
    const char *file = "file.txt";
    const char *result = "/home/jim/file";

    size_t len = mutt_buffer_concatn_path(&buf, dir, 9, file, 4);

    TEST_CHECK(len == 14);
    TEST_CHECK(mutt_str_equal(mutt_b2s(&buf), result));

    mutt_buffer_dealloc(&buf);
  }
}
