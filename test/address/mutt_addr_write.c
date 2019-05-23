/**
 * @file
 * Test code for mutt_addr_write()
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

void test_mutt_addr_write(void)
{
  // size_t mutt_addr_write(char *buf, size_t buflen, struct Address *addr, bool display);

  {
    struct Address addr = { 0 };
    mutt_addr_write(NULL, 32, &addr, false);
    TEST_CHECK_(1, "mutt_addr_write(NULL, 32, &addr, false)");
  }

  {
    char buf[32] = { 0 };
    mutt_addr_write(buf, sizeof(buf), NULL, false);
    TEST_CHECK_(1, "mutt_addr_write(buf, sizeof(buf), NULL, false)");
  }

  { /* integration */
    char buf[256] = { 0 };
    char per[64] = "bobby bob";
    char mbx[64] = "bob@bobsdomain";

    struct Address addr = {
      .personal = per,
      .mailbox = mbx,
      .group = 0,
      .is_intl = 0,
      .intl_checked = 0,
    };

    size_t len = mutt_addr_write(buf, sizeof(buf), &addr, false);

    const char *expected = "bobby bob <bob@bobsdomain>";

    TEST_CHECK_STR_EQ(expected, buf);
    TEST_CHECK(len == strlen(expected));
  }
}
