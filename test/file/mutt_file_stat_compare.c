/**
 * @file
 * Test code for mutt_file_stat_compare()
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
#include <string.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "common.h"
#include "test_common.h"

void test_mutt_file_stat_compare(void)
{
  // int mutt_file_stat_compare(struct stat *sba, enum MuttStatType sba_type, struct stat *sbb, enum MuttStatType sbb_type);

  // clang-format off
  static struct TestValue tests[] = {
    { "%s/file/stat/old",   "%s/file/stat/same1", -1 },
    { "%s/file/stat/same1", "%s/file/stat/same2",  0 },
    { "%s/file/stat/same2", "%s/file/stat/same1",  0 },
    { "%s/file/stat/new",   "%s/file/stat/same2",  1 },
  };
  // clang-format on

  {
    struct stat stat = { 0 };
    TEST_CHECK(mutt_file_stat_compare(NULL, 0, &stat, 0) == 0);
  }

  {
    struct stat stat = { 0 };
    TEST_CHECK(mutt_file_stat_compare(&stat, 0, NULL, 0) == 0);
  }

  int rc;
  struct stat st1;
  struct stat st2;
  struct Buffer *first = buf_pool_get();
  struct Buffer *second = buf_pool_get();

  for (size_t i = 0; i < countof(tests); i++)
  {
    memset(&st1, 0, sizeof(st1));
    memset(&st2, 0, sizeof(st2));
    test_gen_path(first, tests[i].first);
    test_gen_path(second, tests[i].second);

    TEST_CASE(buf_string(first));
    TEST_CHECK(stat(buf_string(first), &st1) == 0);
    TEST_CHECK(stat(buf_string(second), &st2) == 0);

    rc = mutt_file_stat_compare(&st1, MUTT_STAT_MTIME, &st2, MUTT_STAT_MTIME);
    if (!TEST_CHECK(rc == tests[i].retval))
    {
      TEST_MSG("Expected: %d", tests[i].retval);
      TEST_MSG("Actual:   %d", rc);
    }
  }

  buf_pool_release(&first);
  buf_pool_release(&second);
}
