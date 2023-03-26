/**
 * @file
 * Test code for test_email_header_update()
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

void test_email_header_update(void)
{
  // struct ListNode *header_update(sturct ListNode *hdr, const struct Buffer *buf)
  const char *existing_header = "X-Found: foo";
  const char *new_value = "X-Found: 3.14";

  struct ListNode *n = mutt_mem_calloc(1, sizeof(struct ListNode));
  n->data = mutt_str_dup(existing_header);

  {
    struct ListNode *got = header_update(n, new_value);
    TEST_CHECK(got == n);                    /* returns updated node */
    TEST_CHECK_STR_EQ(got->data, new_value); /* node updated to new value */
  }
  FREE(&n->data);
  FREE(&n);
}
