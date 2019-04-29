/**
 * @file
 * Test code for mutt_list_insert_after()
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
#include "mutt/mutt.h"

void test_mutt_list_insert_after(void)
{
  // struct ListNode *mutt_list_insert_after(struct ListHead *h, struct ListNode *n, char *s);

  {
    struct ListNode listnode = { 0 };
    TEST_CHECK(!mutt_list_insert_after(NULL, &listnode, "apple"));
  }

  {
    struct ListHead listhead = { 0 };
    TEST_CHECK(!mutt_list_insert_after(&listhead, NULL, "apple"));
  }

  {
    struct ListHead listhead = STAILQ_HEAD_INITIALIZER(listhead);
    struct ListNode listnode = { 0 };
    TEST_CHECK(mutt_list_insert_after(&listhead, &listnode, NULL) != NULL);
  }
}
