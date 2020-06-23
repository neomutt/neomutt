/**
 * @file
 * Test code for mutt_file_resolve_symlink()
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
#include <limits.h>
#include "mutt/lib.h"
#include "common.h"
#include "test_common.h"

void test_mutt_file_resolve_symlink(void)
{
  // void mutt_file_resolve_symlink(struct Buffer *buf);

  // clang-format off
  static struct TestValue tests[] = {
    { NULL,                      "",                        }, // Invalid
    { "",                        "",                        }, // Invalid
    { "%s/file/size",            "%s/file/size",            }, // Real file
    { "%s/file/size_symlink",    "%s/file/size",            }, // Symlink
    { "%s/file/missing_symlink", "%s/file/missing_symlink", }, // Broken symlink
    { "%s/file/missing",         "%s/file/missing",         }, // Missing file
  };
  // clang-format on

  char first[256] = { 0 };
  char second[256] = { 0 };

  struct Buffer result = mutt_buffer_make(256);
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    test_gen_path(first, sizeof(first), tests[i].first);
    test_gen_path(second, sizeof(second), tests[i].second);
    mutt_buffer_strcpy(&result, first);

    TEST_CASE(first);
    mutt_file_resolve_symlink(&result);
    if (!TEST_CHECK(mutt_str_equal(mutt_b2s(&result), second)))
    {
      TEST_MSG("Original: %s", NONULL(tests[i].first));
      TEST_MSG("Expected: %s", second);
      TEST_MSG("Actual:   %s", mutt_b2s(&result));
    }
  }

  mutt_buffer_dealloc(&result);
}
