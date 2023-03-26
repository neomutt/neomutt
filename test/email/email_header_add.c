/**
 * @file
 * Test code for test_email_header_add()
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

void test_email_header_add(void)
{
  // struct ListNode *header_add(struct ListHead *hdrlist, const struct Buffer *buf)
  const char *header = "X-TestHeader: 123";

  struct ListHead hdrlist = STAILQ_HEAD_INITIALIZER(hdrlist);

  {
    struct ListNode *n = header_add(&hdrlist, header);
    TEST_CHECK_STR_EQ(n->data, header);             /* header stored in node */
    TEST_CHECK(n == header_find(&hdrlist, header)); /* node added to list */
  }
  mutt_list_free(&hdrlist);
}
