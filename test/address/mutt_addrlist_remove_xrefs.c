/**
 * @file
 * Test code for mutt_addrlist_remove_xrefs()
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
#include "config.h"
#include "acutest.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "common.h"

void test_mutt_addrlist_remove_xrefs(void)
{
  // void mutt_addrlist_remove_xrefs(const struct AddressList *a, struct AddressList *b);

  {
    struct AddressList al = { 0 };
    mutt_addrlist_remove_xrefs(NULL, &al);
    TEST_CHECK_(1, "mutt_addrlist_remove_xrefs(NULL, &al)");
  }

  {
    struct AddressList al = { 0 };
    mutt_addrlist_remove_xrefs(&al, NULL);
    TEST_CHECK_(1, "mutt_addrlist_remove_xrefs(&al, NULL)");
  }

  {
    struct AddressList al1 = TAILQ_HEAD_INITIALIZER(al1);
    struct AddressList al2 = TAILQ_HEAD_INITIALIZER(al2);
    mutt_addrlist_append(&al1, mutt_addr_create("Name 1", "foo@example.com"));
    mutt_addrlist_append(&al2, mutt_addr_create("Name 2", "foo@example.com"));
    mutt_addrlist_remove_xrefs(&al1, &al2);
    struct Address *a = TAILQ_FIRST(&al1);
    TEST_CHECK_STR_EQ("foo@example.com", a->mailbox);
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK(a == NULL);
    mutt_addrlist_clear(&al1);
    TEST_CHECK(TAILQ_EMPTY(&al2));
  }

  {
    struct AddressList al1 = TAILQ_HEAD_INITIALIZER(al1);
    struct AddressList al2 = TAILQ_HEAD_INITIALIZER(al2);
    mutt_addrlist_append(&al1, mutt_addr_create("Name 1", "foo@example.com"));
    mutt_addrlist_append(&al2, mutt_addr_create("Name 2", "foo@example.com"));
    mutt_addrlist_append(&al1, mutt_addr_create(NULL, "john@doe.org"));
    mutt_addrlist_append(&al1, mutt_addr_create(NULL, "foo@bar.baz"));
    mutt_addrlist_append(&al2, mutt_addr_create(NULL, "foo@bar.baz"));
    mutt_addrlist_append(&al2,
                         mutt_addr_create(NULL, "mr.pink@reservoir.movie"));
    mutt_addrlist_remove_xrefs(&al1, &al2);
    struct Address *a = TAILQ_FIRST(&al1);
    TEST_CHECK_STR_EQ("foo@example.com", a->mailbox);
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK_STR_EQ("john@doe.org", a->mailbox);
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK_STR_EQ("foo@bar.baz", a->mailbox);
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK(a == NULL);
    a = TAILQ_FIRST(&al2);
    TEST_CHECK_STR_EQ("mr.pink@reservoir.movie", a->mailbox);
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK(a == NULL);
    mutt_addrlist_clear(&al1);
    mutt_addrlist_clear(&al2);
  }
}
