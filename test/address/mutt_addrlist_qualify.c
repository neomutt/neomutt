/**
 * @file
 * Test code for mutt_addrlist_qualify()
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

void test_mutt_addrlist_qualify(void)
{
  // void mutt_addrlist_qualify(struct AddressList *al, const char *host);

  {
    mutt_addrlist_qualify(NULL, "example.com");
    TEST_CHECK_(1, "mutt_addrlist_qualify(NULL, \"example.com\")");
  }

  {
    struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
    mutt_addrlist_qualify(&al, NULL);
    TEST_CHECK_(1, "mutt_addrlist_qualify(&addr, NULL)");
  }

  {
    struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
    mutt_addrlist_parse(&al, "john@doe.org, user1, user2, test@example.com");
    mutt_addrlist_qualify(&al, "local.domain");
    struct Address *a = TAILQ_FIRST(&al);
    TEST_CHECK_STR_EQ("john@doe.org", a->mailbox);
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK_STR_EQ("user1@local.domain", a->mailbox);
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK_STR_EQ("user2@local.domain", a->mailbox);
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK_STR_EQ("test@example.com", a->mailbox);
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK(a == NULL);
    mutt_addrlist_clear(&al);
  }
}
