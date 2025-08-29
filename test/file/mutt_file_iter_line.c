/**
 * @file
 * Test code for mutt_file_iter_line()
 *
 * @authors
 * Copyright (C) 2019-2023 Richard Russon <rich@flatcap.org>
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
#include <stdbool.h>
#include <stdio.h>
#include <string.h> // IWYU pragma: keep
#include "mutt/lib.h"
#include "common.h"
#include "test_common.h"

void test_mutt_file_iter_line(void)
{
  // bool mutt_file_iter_line(struct MuttFileIter *iter, FILE *fp, ReadLineFlags flags);

  {
    FILE *fp = fopen("/dev/null", "r");
    TEST_CHECK(!mutt_file_iter_line(NULL, fp, MUTT_RL_NO_FLAGS));
    fclose(fp);
  }

  {
    struct MuttFileIter muttfileiter = { 0 };
    TEST_CHECK(!mutt_file_iter_line(&muttfileiter, NULL, MUTT_RL_NO_FLAGS));
  }

  {
    FILE *fp = SET_UP();
    if (!fp)
      return;
    struct MuttFileIter iter = { 0 };
    bool res;
    for (int i = 0; file_lines[i]; i++)
    {
      res = mutt_file_iter_line(&iter, fp, MUTT_RL_NO_FLAGS);
      if (!TEST_CHECK(res))
      {
        TEST_MSG("Expected: true");
        TEST_MSG("Actual: false");
      }
      TEST_CHECK_STR_EQ(iter.line, file_lines[i]);
      if (!TEST_CHECK(iter.line_num == (i + 1)))
      {
        TEST_MSG("Expected: %d", i + 1);
        TEST_MSG("Actual: %d", iter.line_num);
      }
    }
    res = mutt_file_iter_line(&iter, fp, MUTT_RL_NO_FLAGS);
    if (!TEST_CHECK(!res))
    {
      TEST_MSG("Expected: false");
      TEST_MSG("Actual: true");
    }
    TEAR_DOWN(fp);
  }
}
