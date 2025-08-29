/**
 * @file
 * Test code for mutt_addrlist_to_local()
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
#include "address/lib.h"

void test_mutt_addrlist_to_local(void)
{
  // int mutt_addrlist_to_local(struct AddressList *al);

  {
    TEST_CHECK(mutt_addrlist_to_local(NULL) == 0);
  }

  {
    // Back and forth tests (to_intl <-> to_local) are done in
    // test_mutt_addrlist_to_intl
  }
}
