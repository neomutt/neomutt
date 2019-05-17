/**
 * @file
 * Test code for mutt_addrlist_remove()
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
#include "config.h"
#include "mutt/mutt.h"
#include "address/lib.h"

void test_mutt_addrlist_remove(void)
{
  // int mutt_addrlist_remove(struct AddressList *al, const char *mailbox);

  {
    int rc = mutt_addrlist_remove(NULL, "apple");
    TEST_CHECK(rc == -1);
  }

  {
    struct AddressList a;
    int rc = mutt_addrlist_remove(&a, NULL);
    TEST_CHECK(rc == 0);
  }
}
