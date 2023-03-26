/**
 * @file
 * Test code for test_email_header_set()
 *
 * @authors
 * Copyright (C) 2020 Matthew Hughes <matthewhughes934@gmail.com>
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
#include "mutt/lib.h"
#include "email/lib.h"
#include "test_common.h"

void test_email_header_set(void)
{
  // struct ListNode *header_set(struct ListHead *hdrlist, const struct Buffer *buf)
  const char *starting_value = "X-TestHeader: 0.57";
  const char *updated_value = "X-TestHeader: 6.28";

  struct ListHead hdrlist = STAILQ_HEAD_INITIALIZER(hdrlist);

  {
    /* Set value for first time */
    struct ListNode *got = header_set(&hdrlist, starting_value);
    TEST_CHECK_STR_EQ(got->data, starting_value); /* value set */
    TEST_CHECK(got == STAILQ_FIRST(&hdrlist));    /* header was added to list */
  }

  {
    /* Update value */
    struct ListNode *got = header_set(&hdrlist, updated_value);
    TEST_CHECK_STR_EQ(got->data, updated_value); /* value set*/
    TEST_CHECK(got == STAILQ_FIRST(&hdrlist));   /* no new header added*/
  }
  mutt_list_free(&hdrlist);
}
