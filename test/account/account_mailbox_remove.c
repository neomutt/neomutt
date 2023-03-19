/**
 * @file
 * Test code for account_mailbox_remove()
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
#include <stdbool.h>
#include "config/lib.h"
#include "core/lib.h"
#include "test_common.h"

static struct ConfigDef Vars[] = {
  // clang-format off
  { "Apple", DT_NUMBER, 42, 0, NULL },
  { NULL },
  // clang-format on
};

void test_account_mailbox_remove(void)
{
  // bool account_mailbox_remove(struct Account *a, struct Mailbox *m);

  {
    TEST_CHECK(account_mailbox_remove(NULL, NULL) == false);
  }

  {
    NeoMutt = test_neomutt_create();
    TEST_CHECK(cs_register_variables(NeoMutt->sub->cs, Vars, DT_NO_FLAGS));

    struct ConfigSubset *sub = cs_subset_new("account", NULL, NULL);
    struct Account *a = account_new("dummy", sub);
    TEST_CHECK(a != NULL);

    struct Mailbox *m = mailbox_new();

    TEST_CHECK(account_mailbox_add(a, m) == true);

    TEST_CHECK(account_mailbox_remove(a, m) == true);

    mailbox_free(&m);
    account_free(&a);
    cs_subset_free(&sub);
    test_neomutt_destroy(&NeoMutt);
  }
}
