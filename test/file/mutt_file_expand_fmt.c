/**
 * @file
 * Test code for mutt_file_expand_fmt()
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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
#include <stddef.h>
#include "mutt/lib.h"

void test_mutt_file_expand_fmt(void)
{
  // void mutt_file_expand_fmt(struct Buffer *dest, const char *fmt, const char *src);

  {
    mutt_file_expand_fmt(NULL, "apple", "banana");
    TEST_CHECK_(1, "mutt_file_expand_fmt(NULL, \"apple\", \"banana\")");
  }

  {
    struct Buffer *buf = buf_pool_get();
    mutt_file_expand_fmt(buf, NULL, "banana");
    TEST_CHECK_(1, "mutt_file_expand_fmt(&buf, NULL, \"banana\")");
    buf_pool_release(&buf);
  }

  {
    struct Buffer *buf = buf_pool_get();
    mutt_file_expand_fmt(buf, "apple", NULL);
    TEST_CHECK_(1, "mutt_file_expand_fmt(&buf, \"apple\", NULL)");
    buf_pool_release(&buf);
  }
}
