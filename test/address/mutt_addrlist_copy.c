/**
 * @file
 * Test code for mutt_addrlist_copy()
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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

void test_mutt_addrlist_copy(void)
{
  // void mutt_addrlist_copy(struct AddressList *dst, const struct AddressList *src, bool prune);

  {
    struct AddressList al = { 0 };
    mutt_addrlist_copy(NULL, &al, false);
    TEST_CHECK_(1, "mutt_addrlist_copy(NULL, &al, false)");
  }

  {
    struct AddressList al = { 0 };
    mutt_addrlist_copy(&al, NULL, false);
    TEST_CHECK_(1, "mutt_addrlist_copy(&al, NULL, false)");
  }

  {
    struct AddressList src = TAILQ_HEAD_INITIALIZER(src);
    struct AddressList dst = TAILQ_HEAD_INITIALIZER(dst);
    mutt_addrlist_copy(&dst, &src, false);
    TEST_CHECK(TAILQ_EMPTY(&src));
    TEST_CHECK(TAILQ_EMPTY(&dst));
  }

  {
    struct AddressList src = TAILQ_HEAD_INITIALIZER(src);
    struct AddressList dst = TAILQ_HEAD_INITIALIZER(dst);
    struct Address a1 = { .mailbox = "test@example.com" };
    struct Address a2 = { .mailbox = "john@doe.org" };
    struct Address a3 = { .mailbox = "the-who@stage.co.uk" };
    mutt_addrlist_append(&src, &a1);
    mutt_addrlist_append(&src, &a2);
    mutt_addrlist_append(&src, &a3);
    mutt_addrlist_copy(&dst, &src, false);
    TEST_CHECK(!TAILQ_EMPTY(&src));
    TEST_CHECK(!TAILQ_EMPTY(&dst));
    struct Address *adst = TAILQ_FIRST(&dst);
    TEST_CHECK_STR_EQ(a1.mailbox, adst->mailbox);
    adst = TAILQ_NEXT(adst, entries);
    TEST_CHECK_STR_EQ(a2.mailbox, adst->mailbox);
    adst = TAILQ_NEXT(adst, entries);
    TEST_CHECK_STR_EQ(a3.mailbox, adst->mailbox);
    mutt_addrlist_free_all(&dst);
  }
}
