/**
 * @file
 * Test code for mutt_addrlist_free()
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

void test_mutt_addrlist_free(void)
{
  // void mutt_addrlist_free(struct AddressList **al);

  {
    mutt_addrlist_free(NULL);
    TEST_CHECK_(1, "mutt_addrlist_free(NULL)");
  }

  {
    struct AddressList *al = mutt_addrlist_new();
    int parsed =
        mutt_addrlist_parse(al, "john@doe.org, foo@example.com, bar@baz.org");
    TEST_CHECK(parsed == 3);
    TEST_CHECK_STR_EQ(TAILQ_FIRST(al)->mailbox, "john@doe.org");
    TEST_CHECK_STR_EQ(TAILQ_LAST(al, AddressList)->mailbox, "bar@baz.org");
    mutt_addrlist_free(&al);
    TEST_CHECK(al == NULL);
  }
}
