/**
 * @file
 * Test code for mutt_addr_valid_msgid()
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

void test_mutt_addr_valid_msgid(void)
{
  // bool mutt_addr_valid_msgid(const char *msgid);

  TEST_CHECK(!mutt_addr_valid_msgid(NULL));
  TEST_CHECK(!mutt_addr_valid_msgid("test@example.com"));
  TEST_CHECK(!mutt_addr_valid_msgid("<ae>"));
  TEST_CHECK(!mutt_addr_valid_msgid("<Ÿ@example.com"));
  TEST_CHECK(!mutt_addr_valid_msgid("<king@gælic-republic.org>"));
  TEST_CHECK(mutt_addr_valid_msgid("<a@e>")); // the bare minimum
}
