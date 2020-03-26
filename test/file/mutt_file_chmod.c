/**
 * @file
 * Test code for mutt_file_chmod()
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
#include <fcntl.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "common.h"

void test_mutt_file_chmod(void)
{
  // int mutt_file_chmod(const char *path, mode_t mode);

  // clang-format off
  static struct TestValue tests_fail[] = {
    { NULL,                      }, // Invalid
    { "",                        }, // Invalid
    { "%s/file/missing",         }, // Missing file
    { "%s/file/missing_symlink", }, // Broken symlink
  };
  // clang-format on

  char first[256] = { 0 };
  int rc;

  for (size_t i = 0; i < mutt_array_size(tests_fail); i++)
  {
    test_gen_path(first, sizeof(first), tests_fail[i].first);

    TEST_CASE(first);
    rc = mutt_file_chmod(first, S_IRUSR | S_IWUSR);
    TEST_CHECK(rc == -1);
  }

  // clang-format off
  static struct TestValue tests_succeed[] = {
    { "%s/file/chmod",         NULL, 0640 }, // Real file
    { "%s/file/chmod_symlink", NULL, 0640 }, // Symlink
  };
  // clang-format on

  struct stat st;
  for (size_t i = 0; i < mutt_array_size(tests_succeed); i++)
  {
    test_gen_path(first, sizeof(first), tests_succeed[i].first);

    TEST_CASE(first);

    TEST_CHECK(chmod(first, 0000) == 0);
    rc = mutt_file_chmod(first, tests_succeed[i].retval);
    TEST_CHECK(rc == 0);
    TEST_CHECK(stat(first, &st) == 0);
    if (!TEST_CHECK((st.st_mode & 0777) == tests_succeed[i].retval))
    {
      TEST_MSG("Expected: %o", tests_succeed[i].retval);
      TEST_MSG("Actual:   %o", (st.st_mode & 0777));
    }
  }
}
