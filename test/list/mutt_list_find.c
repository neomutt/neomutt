/**
 * @file
 * Test code for mutt_list_find()
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
#include "mutt/lib.h"
#include "common.h"

void test_mutt_list_find(void)
{
  // struct ListNode *mutt_list_find(const struct ListHead *h, const char *data);

  {
    TEST_CHECK(!mutt_list_find(NULL, "apple"));
  }

  {
    struct ListHead listhead = { 0 };
    TEST_CHECK(!mutt_list_find(&listhead, NULL));
  }

  {
    static const char *names[] = { "Amy", "Beth", "Cathy", "Denise", NULL };
    const char *needle = "Amy";
    struct ListHead haystack = test_list_create(names, false);
    TEST_CHECK(mutt_list_find(&haystack, needle) != NULL);
    mutt_list_clear(&haystack);
  }

  {
    static const char *names[] = { "Amy", "Beth", "Cathy", "Denise", NULL };
    const char *needle = "Cathy";
    struct ListHead haystack = test_list_create(names, false);
    TEST_CHECK(mutt_list_find(&haystack, needle) != NULL);
    mutt_list_clear(&haystack);
  }

  {
    static const char *names[] = { "Amy", "Beth", "Cathy", "Denise", NULL };
    const char *needle = "Denise";
    struct ListHead haystack = test_list_create(names, false);
    TEST_CHECK(mutt_list_find(&haystack, needle) != NULL);
    mutt_list_clear(&haystack);
  }

  {
    static const char *names[] = { "Amy", "Beth", "Cathy", "Denise", NULL };
    const char *needle = "Erica";
    struct ListHead haystack = test_list_create(names, false);
    TEST_CHECK(mutt_list_find(&haystack, needle) == NULL);
    mutt_list_clear(&haystack);
  }

  {
    static const char *names[] = { "Amy", "Beth", "Cathy", "Denise", NULL };
    const char *needle = "amy";
    struct ListHead haystack = test_list_create(names, false);
    TEST_CHECK(mutt_list_find(&haystack, needle) == NULL);
    mutt_list_clear(&haystack);
  }

  {
    const char *needle = "";
    struct ListHead haystack = STAILQ_HEAD_INITIALIZER(haystack);
    TEST_CHECK(mutt_list_find(&haystack, needle) == NULL);
    mutt_list_clear(&haystack);
  }

  {
    const char *needle = "Amy";
    struct ListHead haystack = STAILQ_HEAD_INITIALIZER(haystack);
    TEST_CHECK(mutt_list_find(&haystack, needle) == NULL);
    mutt_list_clear(&haystack);
  }
}
