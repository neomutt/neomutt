/**
 * @file
 * Test code for mutt_addrlist_remove()
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

void test_mutt_addrlist_remove(void)
{
  // int mutt_addrlist_remove(struct AddressList *al, const char *mailbox);

  {
    TEST_CHECK(mutt_addrlist_remove(NULL, "apple") == -1);
  }

  {
    struct AddressList a;
    TEST_CHECK(mutt_addrlist_remove(&a, NULL) == 0);
  }

  {
    struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
    mutt_addrlist_append(&al, mutt_addr_create("Foo", "foobar@example.com"));
    TEST_CHECK(mutt_addrlist_remove(&al, "foobar@example.co.uk") == -1);
    TEST_CHECK(!TAILQ_EMPTY(&al));
    mutt_addrlist_clear(&al);
  }

  {
    struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
    mutt_addrlist_append(&al, mutt_addr_create("Foo", "foobar@example.com"));
    TEST_CHECK(mutt_addrlist_remove(&al, "foobar@example.com") == 0);
    TEST_CHECK(TAILQ_EMPTY(&al));
  }

  {
    struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
    mutt_addrlist_append(&al, mutt_addr_create("Upper", "UPPER@EXAMPLE.com"));
    mutt_addrlist_append(&al, mutt_addr_create("lower", "upper@example.com"));
    TEST_CHECK(mutt_addrlist_remove(&al, "uPPeR@ExAmple.com") == 0);
    TEST_CHECK(TAILQ_EMPTY(&al));
  }
}
