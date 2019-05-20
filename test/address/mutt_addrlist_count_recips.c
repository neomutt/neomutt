/**
 * @file
 * Test code for mutt_addrlist_count_recips()
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

void test_mutt_addrlist_count_recips(void)
{
  // int mutt_addrlist_count_recips(const struct AddressList *al);

  {
    TEST_CHECK(mutt_addrlist_count_recips(NULL) == 0);
  }

  {
    struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
    TEST_CHECK(mutt_addrlist_count_recips(&al) == 0);
  }

  {
    struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
    mutt_addrlist_parse(&al, "test@example.com, john@doe.org");
    TEST_CHECK(mutt_addrlist_count_recips(&al) == 2);
  }

  {
    struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
    mutt_addrlist_parse(&al, "test@example.com, john@doe.org");
    mutt_addrlist_append(&al, mutt_addr_new());
    struct Address *a = mutt_addr_new();
    a->mailbox = mutt_str_strdup("foo@bar.baz");
    mutt_addrlist_append(&al, a);
    TEST_CHECK(mutt_addrlist_count_recips(&al) == 3);
  }

  {
    struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
    mutt_addrlist_parse(&al, "test@example.com, john@doe.org");
    mutt_addrlist_append(&al, mutt_addr_new());
    struct Address *a = mutt_addr_new();
    a->mailbox = mutt_str_strdup("foo@bar.baz");
    a->group = 1;
    mutt_addrlist_append(&al, a);
    TEST_CHECK(mutt_addrlist_count_recips(&al) == 2);
  }
}
