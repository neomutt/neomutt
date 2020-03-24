/**
 * @file
 * Test code for mutt_file_check_empty()
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

void test_mutt_file_check_empty(void)
{
  // int mutt_file_check_empty(const char *path);

  {
    TEST_CHECK(mutt_file_check_empty(NULL) != 0);
  }

  // clang-format off
  static struct TestValue tests[] = {
    { NULL,                      NULL, -1 }, // Invalid
    { "",                        NULL, -1 }, // Invalid
    { "%s/file/empty",           NULL,  1 }, // Empty file
    { "%s/file/empty_symlink",   NULL,  1 }, // Symlink
    { "%s/file/size",            NULL,  0 }, // Non-empty file
    { "%s/file/missing_symlink", NULL, -1 }, // Broken symlink
    { "%s/file/missing",         NULL, -1 }, // Missing file
  };
  // clang-format on

  char first[256] = { 0 };
  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    test_gen_path(first, sizeof(first), tests[i].first);

    TEST_CASE(first);
    rc = mutt_file_check_empty(first);
    TEST_CHECK(rc == tests[i].retval);
  }
}
