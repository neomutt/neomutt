/**
 * @file
 * Test code for mutt_file_chmod_rm_stat()
 *
 * @authors
 * Copyright (C) 2019-2020 Richard Russon <rich@flatcap.org>
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
#include <sys/stat.h>
#include "mutt/lib.h"
#include "common.h"
#include "test_common.h"

void test_mutt_file_chmod_rm_stat(void)
{
  // int mutt_file_chmod_rm_stat(const char *path, mode_t mode, struct stat *st);

  {
    struct stat stat = { 0 };
    TEST_CHECK(mutt_file_chmod_rm_stat(NULL, 0, &stat) != 0);
  }

  {
    TEST_CHECK(mutt_file_chmod_rm_stat("apple", 0, NULL) != 0);
  }

  struct Buffer *first = buf_pool_get();
  int rc;

  // clang-format off
  static struct TestValue tests[] = {
    { "%s/file/chmod",         NULL, 0444 }, // Real file
    { "%s/file/chmod_symlink", NULL, 0444 }, // Symlink
  };
  // clang-format on

  struct stat st;
  for (size_t i = 0; i < countof(tests); i++)
  {
    test_gen_path(first, tests[i].first);

    TEST_CASE(buf_string(first));

    TEST_CHECK(chmod(buf_string(first), 0666) == 0);
    TEST_CHECK(stat(buf_string(first), &st) == 0);
    rc = mutt_file_chmod_rm_stat(buf_string(first), 0222, &st);
    TEST_CHECK_NUM_EQ(rc, 0);
    TEST_CHECK(stat(buf_string(first), &st) == 0);
    if (!TEST_CHECK((st.st_mode & 0777) == tests[i].retval))
    {
      TEST_MSG("Expected: %o", tests[i].retval);
      TEST_MSG("Actual:   %o", (st.st_mode & 0777));
    }

    TEST_CHECK(chmod(buf_string(first), 0444) == 0);
    TEST_CHECK(stat(buf_string(first), &st) == 0);
    rc = mutt_file_chmod_rm_stat(buf_string(first), 0222, &st);
    TEST_CHECK_NUM_EQ(rc, 0);
    TEST_CHECK(stat(buf_string(first), &st) == 0);
    if (!TEST_CHECK((st.st_mode & 0777) == tests[i].retval))
    {
      TEST_MSG("Expected: %o", tests[i].retval);
      TEST_MSG("Actual:   %o", (st.st_mode & 0777));
    }
  }

  buf_pool_release(&first);
}
