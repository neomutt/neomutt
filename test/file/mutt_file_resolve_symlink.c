/**
 * @file
 * Test code for mutt_file_resolve_symlink()
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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

  struct Buffer *first = buf_pool_get();
  struct Buffer *second = buf_pool_get();

  struct Buffer *result = buf_pool_get();
  for (size_t i = 0; i < countof(tests); i++)
  {
    test_gen_path(first, tests[i].first);
    test_gen_path(second, tests[i].second);
    buf_strcpy(result, buf_string(first));

    TEST_CASE(buf_string(first));
    mutt_file_resolve_symlink(result);
    TEST_CHECK_STR_EQ(buf_string(result), buf_string(second));
  }

  buf_pool_release(&first);
  buf_pool_release(&second);
  buf_pool_release(&result);
}
