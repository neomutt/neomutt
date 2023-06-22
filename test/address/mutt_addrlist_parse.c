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
#include "config.h"
#include "acutest.h"
#include <stddef.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "test_common.h"

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
    TEST_CHECK_STR_EQ(buf_string(TAILQ_FIRST(&alist)->mailbox), "apple");
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
    int parsed = mutt_addrlist_parse(&alist, "Complete <address@example.com>, Incomplete <address");
    TEST_CHECK(parsed == 0);
    TEST_CHECK(TAILQ_EMPTY(&alist));
  }

  {
    struct AddressList alist = TAILQ_HEAD_INITIALIZER(alist);
    int parsed = mutt_addrlist_parse(&alist, "Simple Address <test@example.com>, My Group: member1@group.org, member2@group.org, \"John M. Doe\" <john@doe.org>;, Another One <foo@bar.baz>, Elvis (The Pelvis) Presley <elvis@king.com>");
    TEST_CHECK(parsed == 6);
    TEST_CHECK(!TAILQ_EMPTY(&alist));
    struct Address *a = TAILQ_FIRST(&alist);
    TEST_CHECK_STR_EQ(buf_string(a->mailbox), "test@example.com");
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK_STR_EQ(buf_string(a->mailbox), "My Group");
    TEST_CHECK(a->group == 1);
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK_STR_EQ(buf_string(a->mailbox), "member1@group.org");
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK_STR_EQ(buf_string(a->mailbox), "member2@group.org");
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK_STR_EQ(buf_string(a->mailbox), "john@doe.org");
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK(a->mailbox == NULL);
    TEST_CHECK(a->group == 0);
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK_STR_EQ(buf_string(a->mailbox), "foo@bar.baz");
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK_STR_EQ(buf_string(a->mailbox), "elvis@king.com");
    TEST_CHECK_STR_EQ(buf_string(a->personal), "Elvis Presley");
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK(a == NULL);
    mutt_addrlist_clear(&alist);
  }

  {
    struct AddressList alist = TAILQ_HEAD_INITIALIZER(alist);
    int parsed = mutt_addrlist_parse(&alist, "Foo \\(Bar\\) <foo@bar.baz>");
    TEST_CHECK(parsed == 1);
    const struct Address *a = TAILQ_FIRST(&alist);
    TEST_CHECK_STR_EQ(buf_string(a->personal), "Foo (Bar)");
    TEST_CHECK_STR_EQ(buf_string(a->mailbox), "foo@bar.baz");
    mutt_addrlist_clear(&alist);
  }

  {
    struct AddressList alist = TAILQ_HEAD_INITIALIZER(alist);
    int parsed = mutt_addrlist_parse(&alist, "\\");
    TEST_CHECK(parsed == 0);
  }

  {
    struct AddressList alist = TAILQ_HEAD_INITIALIZER(alist);
    int parsed = mutt_addrlist_parse(&alist, "empty-group:; (some comment)");
    TEST_CHECK(parsed == 0);
    const struct Address *a = TAILQ_FIRST(&alist);
    TEST_CHECK(a->personal == NULL);
    TEST_CHECK_STR_EQ(buf_string(a->mailbox), "empty-group");
    TEST_CHECK(a->group == true);
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK(a->personal == NULL);
    TEST_CHECK(a->mailbox == NULL);
    TEST_CHECK(a->group == false);
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK(a == NULL);
    mutt_addrlist_clear(&alist);
  }

  {
    // So this is strange, I'm not sure I understand the logic. We use a
    // comment at the end of an address as the personal part only if the
    // address doesn't already have a personal part. Otherwise we ignore the
    // comment.
    //
    struct AddressList alist = TAILQ_HEAD_INITIALIZER(alist);
    int parsed = mutt_addrlist_parse(&alist, "my-group: "
                                             "                  <foo@bar.baz> (comment 1),"
                                             "\"I have a name\" <bar@baz.com> (comment 2);");

    TEST_CHECK(parsed == 2);

    const struct Address *a = TAILQ_FIRST(&alist);
    TEST_CHECK(a->personal == NULL);
    TEST_CHECK_STR_EQ(buf_string(a->mailbox), "my-group");
    TEST_CHECK(a->group == true);

    a = TAILQ_NEXT(a, entries);
    TEST_CHECK_STR_EQ(buf_string(a->personal), "comment 1");
    TEST_CHECK_STR_EQ(buf_string(a->mailbox), "foo@bar.baz");
    TEST_CHECK(a->group == false);

    a = TAILQ_NEXT(a, entries);
    TEST_CHECK_STR_EQ(buf_string(a->personal), "I have a name"); // no comment
    TEST_CHECK_STR_EQ(buf_string(a->mailbox), "bar@baz.com");
    TEST_CHECK(a->group == false);

    a = TAILQ_NEXT(a, entries);
    TEST_CHECK(a->personal == NULL);
    TEST_CHECK(a->mailbox == NULL);
    TEST_CHECK(a->group == false);

    a = TAILQ_NEXT(a, entries);
    TEST_CHECK(a == NULL);
    mutt_addrlist_clear(&alist);
  }
}
