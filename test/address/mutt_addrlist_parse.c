/**
 * @file
 * Test code for mutt_addrlist_parse()
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

void test_mutt_addrlist_parse(void)
{
  // int mutt_addrlist_parse(struct AddressList *al, const char *s);
  {
    struct AddressList alist = TAILQ_HEAD_INITIALIZER(alist);
    int parsed = mutt_addrlist_parse(&alist, NULL);
    TEST_CHECK(parsed == 0);
    TEST_CHECK(TAILQ_EMPTY(&alist));
  }

  {
    struct AddressList alist = TAILQ_HEAD_INITIALIZER(alist);
    int parsed = mutt_addrlist_parse(&alist, "apple");
    TEST_CHECK(parsed == 1);
    TEST_CHECK(!TAILQ_EMPTY(&alist));
    TEST_CHECK_STR_EQ(TAILQ_FIRST(&alist)->mailbox, "apple");
    mutt_addrlist_clear(&alist);
  }

  {
    struct AddressList alist = TAILQ_HEAD_INITIALIZER(alist);
    int parsed = mutt_addrlist_parse(&alist, "Incomplete <address");
    TEST_CHECK(parsed == 0);
    TEST_CHECK(TAILQ_EMPTY(&alist));
  }

  {
    struct AddressList alist = TAILQ_HEAD_INITIALIZER(alist);
    int parsed = mutt_addrlist_parse(&alist,
        "Complete <address@example.com>, Incomplete <address");
    TEST_CHECK(parsed == 0);
    TEST_CHECK(TAILQ_EMPTY(&alist));
  }

  {
    struct AddressList alist = TAILQ_HEAD_INITIALIZER(alist);
    int parsed = mutt_addrlist_parse(&alist,
        "Simple Address <test@example.com>,"
        "My Group: member1@group.org, member2@group.org, "
        "\"John M. Doe\" <john@doe.org>; Another One <foo@bar.baz>");
    TEST_CHECK(parsed == 5);
    TEST_CHECK(!TAILQ_EMPTY(&alist));
    struct Address *a = TAILQ_FIRST(&alist);
    TEST_CHECK_STR_EQ("test@example.com", a->mailbox);
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK_STR_EQ("My Group", a->mailbox);
    TEST_CHECK(a->group == 1);
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK_STR_EQ("member1@group.org", a->mailbox);
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK_STR_EQ("member2@group.org", a->mailbox);
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK_STR_EQ("john@doe.org", a->mailbox);
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK(a->mailbox == NULL);
    TEST_CHECK(a->group == 0);
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK_STR_EQ("foo@bar.baz", a->mailbox);
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK(a == NULL);
    mutt_addrlist_clear(&alist);
  }
}
