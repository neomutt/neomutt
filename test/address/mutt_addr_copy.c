/**
 * @file
 * Test code for mutt_addr_copy()
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
#include "address/lib.h"
#include "common.h"

void test_mutt_addr_copy(void)
{
  // struct Address *mutt_addr_copy(const struct Address *addr);

  {
    TEST_CHECK(!mutt_addr_copy(NULL));
  }

  {
    struct Address a1 = { .personal = "John Doe",
                          .mailbox = "john@doe.com",
                          .group = 0,
                          .is_intl = 0,
                          .intl_checked = false };
    struct Address *a2 = mutt_addr_copy(&a1);
    TEST_CHECK(a2 != NULL);
    TEST_CHECK_STR_EQ(a1.personal, a2->personal);
    TEST_CHECK_STR_EQ(a1.mailbox, a2->mailbox);
    TEST_CHECK(a1.group == a2->group);
    TEST_CHECK(a1.is_intl == a2->is_intl);
    TEST_CHECK(a1.intl_checked == a2->intl_checked);
    mutt_addr_free(&a2);
  }

  {
    struct Address a1 = {
      .personal = NULL, .mailbox = "john@doe.com", .group = 0, .is_intl = 0, .intl_checked = false
    };
    struct Address *a2 = mutt_addr_copy(&a1);
    TEST_CHECK(a2 != NULL);
    TEST_CHECK(a2->personal == NULL);
    TEST_CHECK_STR_EQ(a1.mailbox, a2->mailbox);
    TEST_CHECK(a1.group == a2->group);
    TEST_CHECK(a1.is_intl == a2->is_intl);
    TEST_CHECK(a1.intl_checked == a2->intl_checked);
    mutt_addr_free(&a2);
  }
}
