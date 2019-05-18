/**
 * @file
 * Test code for mutt_addrlist_prepend()
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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
#include "common.h"
#include "config.h"
#include "mutt/mutt.h"
#include "address/lib.h"

void test_mutt_addrlist_prepend(void)
{
  // void mutt_addrlist_prepend(struct AddressList *al, struct Address *a);

  {
    struct Address a = { 0 };
    mutt_addrlist_prepend(NULL, &a);
    TEST_CHECK_(1, "mutt_addrlist_prepend(NULL, &a)");
  }

  {
    struct AddressList al = { 0 };
    mutt_addrlist_prepend(&al, NULL);
    TEST_CHECK_(1, "mutt_addrlist_prepend(&al, NULL)");
  }

  {
    struct Address a1 = { .mailbox = "test@example.com" };
    struct Address a2 = { .mailbox = "john@doe.org" };
    struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
    mutt_addrlist_prepend(&al, &a1);
    mutt_addrlist_prepend(&al, &a2);
    struct Address *a = TAILQ_FIRST(&al);
    TEST_CHECK_STR_EQ("john@doe.org", a->mailbox);
    a = TAILQ_NEXT(a, entries);
    TEST_CHECK_STR_EQ("test@example.com", a->mailbox);
  }
}
