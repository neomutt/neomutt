/**
 * @file
 * Test code for mutt_addrlist_clear()
 *
 * @authors
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
#include "mutt/lib.h"
#include "address/lib.h"

void test_mutt_addrlist_clear(void)
{
  // void mutt_addrlist_clear(struct AddressList *al);

  {
    mutt_addrlist_clear(NULL);
    TEST_CHECK_(1, "mutt_addrlist_clear(NULL)");
  }

  {
    struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
    mutt_addrlist_append(&al, mutt_addr_new());
    mutt_addrlist_append(&al, mutt_addr_new());
    mutt_addrlist_append(&al, mutt_addr_new());
    mutt_addrlist_append(&al, mutt_addr_new());
    mutt_addrlist_append(&al, mutt_addr_new());
    mutt_addrlist_clear(&al);
    TEST_CHECK(TAILQ_EMPTY(&al));
  }
}
