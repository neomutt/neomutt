/**
 * @file
 * Test code for grouplist_add_addrlist()
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
#include "mutt/lib.h"
#include "address/lib.h"

void test_grouplist_add_addrlist(void)
{
  // void grouplist_add_addrlist(struct GroupList *gl, struct AddressList *al);

  {
    struct AddressList addr = TAILQ_HEAD_INITIALIZER(addr);
    grouplist_add_addrlist(NULL, &addr);
    TEST_CHECK_(1, "grouplist_add_addrlist(NULL, &addr)");
  }

  {
    struct GroupList gl = { 0 };
    grouplist_add_addrlist(&gl, NULL);
    TEST_CHECK_(1, "grouplist_add_addrlist(&gl, NULL)");
  }
}
