/**
 * @file
 * Test code for mutt_file_get_size()
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
#include "acutest.h"
#include "config.h"
#include "mutt/lib.h"
#include "common.h"

void test_mutt_file_get_size(void)
{
  // long mutt_file_get_size(const char *path);

  // clang-format off
  static struct TestValue tests[] = {
    { NULL,                      "Invalid",        0,    },
    { "%s/file/size",            "Real path",      1234, },
    { "%s/file/size_symlink",    "Symlink",        1234, },
    { "%s/file/missing_symlink", "Broken symlink", 0,    },
    { "%s/file/missing",         "Missing",        0,    },
  };
  // clang-format on

  char first[256] = { 0 };

  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    test_gen_path(first, sizeof(first), tests[i].first);

    TEST_CASE(tests[i].second);
    rc = mutt_file_get_size(first);
    TEST_CHECK(rc == tests[i].retval);
  }
}
