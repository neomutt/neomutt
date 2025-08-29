/**
 * @file
 * Test code for mutt_addrlist_dedupe()
 *
 * @authors
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2019-2022 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Anna Figueiredo Gomes <navi@vlhl.dev>
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
#include "mutt/lib.h"
#include "address/lib.h"
#include "test_common.h"

void test_mutt_addrlist_dedupe(void)
{
  // void mutt_addrlist_dedupe(struct AddressList *al);

  {
    mutt_addrlist_dedupe(NULL);
    TEST_CHECK_(1, "mutt_addrlist_dedupe(NULL)");
  }

  {
    struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
    int parsed = mutt_addrlist_parse(&al, "Name 1 <test@example.com>, john@doe.org, toast@example.com, Another <test@example.com>, toast@bar.org, foo@bar.baz, john@doe.org");
    TEST_CHECK(parsed == 7);
    mutt_addrlist_dedupe(&al);
    struct Address *a = TAILQ_FIRST(&al);
    TEST_CHECK_STR_EQ(buf_string(a->mailbox), "test@example.com");
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK_STR_EQ(buf_string(a->mailbox), "john@doe.org");
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK_STR_EQ(buf_string(a->mailbox), "toast@example.com");
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK_STR_EQ(buf_string(a->mailbox), "toast@bar.org");
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK_STR_EQ(buf_string(a->mailbox), "foo@bar.baz");
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK(a == NULL);
    mutt_addrlist_clear(&al);
  }
}
