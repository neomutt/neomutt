/**
 * @file
 * Test code for buf_quote_filename()
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
#include <stdbool.h>
#include "mutt/lib.h"
#include "common.h"
#include "test_common.h"

void test_buf_quote_filename(void)
{
  // void buf_quote_filename(struct Buffer *buf, const char *filename, bool add_outer)

  // clang-format off
  static struct TestValue tests[] = {
    { "plain",  "plain",        },
    { "plain",  "'plain'",      },
    { "ba`ck",  "ba'\\`'ck",    },
    { "ba`ck",  "'ba'\\`'ck'",  },
    { "qu'ote", "qu'\\''ote",   },
    { "qu'ote", "'qu'\\''ote'", },
  };
  // clang-format on

  {
    buf_quote_filename(NULL, NULL, false);
    TEST_CHECK_(1, "buf_quote_filename(NULL, NULL, false)");
  }

  struct Buffer *result = buf_pool_get();
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    TEST_CASE(tests[i].first);
    buf_quote_filename(result, tests[i].first, (i % 2));
    TEST_CHECK_STR_EQ(buf_string(result), tests[i].second);
  }

  buf_pool_release(&result);
}
