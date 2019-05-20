/**
 * @file
 * Test code for mutt_addrlist_parse2()
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

void test_mutt_addrlist_parse2(void)
{
  // int mutt_addrlist_parse2(struct AddressList *al, const char *s);

  {
    struct AddressList alist = TAILQ_HEAD_INITIALIZER(alist);
    int parsed = mutt_addrlist_parse2(&alist, NULL);
    TEST_CHECK(parsed == 0);
    TEST_CHECK(TAILQ_EMPTY(&alist));
  }

  {
    struct AddressList alist = TAILQ_HEAD_INITIALIZER(alist);
    int parsed = mutt_addrlist_parse2(&alist, "apple");
    TEST_CHECK(parsed == 1);
    TEST_CHECK(!TAILQ_EMPTY(&alist));
    TEST_CHECK_STR_EQ(TAILQ_FIRST(&alist)->mailbox, "apple");
    mutt_addrlist_clear(&alist);
  }

  {
    /* Not extremely nice, but this is the way it works... */
    struct AddressList alist = TAILQ_HEAD_INITIALIZER(alist);
    int parsed = mutt_addrlist_parse2(&alist, "test@example.com John Doe <john@doe.org>");
    TEST_CHECK(parsed == 1);
    TEST_CHECK(!TAILQ_EMPTY(&alist));
    struct Address *a = TAILQ_FIRST(&alist);
    TEST_CHECK_STR_EQ("test@example.com John Doe", a->personal);
    TEST_CHECK_STR_EQ("john@doe.org", a->mailbox);
    mutt_addrlist_clear(&alist);
  }

  {
    struct AddressList alist = TAILQ_HEAD_INITIALIZER(alist);
    int parsed = mutt_addrlist_parse2(&alist, "test@example.com john@doe.org foo@bar.baz");
    TEST_CHECK(parsed == 3);
    TEST_CHECK(!TAILQ_EMPTY(&alist));
    struct Address *a = TAILQ_FIRST(&alist);
    TEST_CHECK(a->personal == NULL);
    TEST_CHECK_STR_EQ("test@example.com", a->mailbox);
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK(a->personal == NULL);
    TEST_CHECK_STR_EQ("john@doe.org", a->mailbox);
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK(a->personal == NULL);
    TEST_CHECK_STR_EQ("foo@bar.baz", a->mailbox);
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK(a == NULL);
    mutt_addrlist_clear(&alist);
  }
}
