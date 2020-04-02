/**
 * @file
 * Test code for mutt_list_insert_tail()
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

void test_mutt_list_insert_tail(void)
{
  // struct ListNode *mutt_list_insert_tail(struct ListHead *h, char *s);

  {
    TEST_CHECK(!mutt_list_insert_tail(NULL, "apple"));
  }

  {
    struct ListHead listhead = STAILQ_HEAD_INITIALIZER(listhead);
    struct ListNode *newnode = NULL;
    TEST_CHECK((newnode = mutt_list_insert_tail(&listhead, NULL)) != NULL);
    FREE(&newnode);
  }

  {
    static const char *expected_names[] = { "Zelda", NULL };
    char *insert = "Zelda";
    struct ListHead start = STAILQ_HEAD_INITIALIZER(start);
    struct ListHead expected = test_list_create(expected_names, false);
    TEST_CHECK(mutt_list_insert_tail(&start, insert) != NULL);
    TEST_CHECK(mutt_list_compare(&start, &expected) == true);
    mutt_list_clear(&start);
    mutt_list_clear(&expected);
  }

  {
    static const char *start_names[] = { "Amy", "Beth", "Cathy", NULL };
    static const char *expected_names[] = { "Amy", "Beth", "Cathy", "Zelda", NULL };
    char *insert = "Zelda";
    struct ListHead start = test_list_create(start_names, false);
    struct ListHead expected = test_list_create(expected_names, false);
    TEST_CHECK(mutt_list_insert_tail(&start, insert) != NULL);
    TEST_CHECK(mutt_list_compare(&start, &expected) == true);
    mutt_list_clear(&start);
    mutt_list_clear(&expected);
  }
}
