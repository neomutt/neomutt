/**
 * @file
 * Test code for mutt_addr_cmp()
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2023 Anna Figueiredo Gomes <navi@vlhl.dev>
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

void test_mutt_addr_cmp(void)
{
  // bool mutt_addr_cmp(const struct Address *a, const struct Address *b);

  {
    struct Address addr = { 0 };
    TEST_CHECK(!mutt_addr_cmp(NULL, &addr));
  }

  {
    struct Address addr = { 0 };
    TEST_CHECK(!mutt_addr_cmp(&addr, NULL));
  }

  {
    struct Address a1 = { .mailbox = buf_new("test@example.com") };
    struct Address a2 = { .mailbox = buf_new("test@example.com") };
    TEST_CHECK(mutt_addr_cmp(&a1, &a2));
    buf_free(&a1.mailbox);
    buf_free(&a2.mailbox);
  }

  {
    struct Address a1 = { .mailbox = buf_new("test@example.com") };
    struct Address a2 = { .mailbox = buf_new("TEST@example.COM") };
    TEST_CHECK(mutt_addr_cmp(&a1, &a2));
    buf_free(&a1.mailbox);
    buf_free(&a2.mailbox);
  }

  {
    struct Address a1 = { .mailbox = buf_new("test@example.com") };
    struct Address a2 = { .mailbox = buf_new("test@example.com.org") };
    TEST_CHECK(!mutt_addr_cmp(&a1, &a2));
    buf_free(&a1.mailbox);
    buf_free(&a2.mailbox);
  }
}
