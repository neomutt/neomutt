/**
 * @file
 * Test code for mutt_str_inbox_cmp()
 *
 * @authors
 * Copyright (C) 2022 Pietro Cerutti <gahr@gahr.ch>
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
#include "test_common.h"

/* This is basically browser_sort_subject and sb_sort_path */
static int sort(const void *a, const void *b, void *state)
{
  const char *fa = *(char **) a;
  const char *fb = *(char **) b;

  const int rc = mutt_str_inbox_cmp(fa, fb);
  if (rc != 0)
  {
    return rc;
  }
  return mutt_str_coll(fa, fb);
}

void test_mutt_str_inbox_cmp(void)
{
  char *folders[] = { "+Inbox", "+Inbox.Archive", "+FooBar", "+FooBar.Baz" };
  mutt_qsort_r(&folders, mutt_array_size(folders), sizeof(*folders), sort, NULL);
  TEST_CHECK_STR_EQ(folders[0], "+Inbox");
  TEST_CHECK_STR_EQ(folders[1], "+Inbox.Archive");
  TEST_CHECK_STR_EQ(folders[2], "+FooBar");
  TEST_CHECK_STR_EQ(folders[3], "+FooBar.Baz");
}
