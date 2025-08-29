/**
 * @file
 * Test code for account_mailbox_add()
 *
 * @authors
 * Copyright (C) 2020-2023 Richard Russon <rich@flatcap.org>
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
#include <stdbool.h>
#include <stddef.h>
#include "config/lib.h"
#include "core/lib.h"
#include "test_common.h" // IWYU pragma: keep

static struct ConfigDef Vars[] = {
  // clang-format off
  { "Apple", DT_NUMBER, 42, 0, NULL },
  { NULL },
  // clang-format on
};

void test_account_mailbox_add(void)
{
  // bool account_mailbox_add(struct Account *a, struct Mailbox *m);

  {
    TEST_CHECK(account_mailbox_add(NULL, NULL) == false);
  }

  TEST_CHECK(cs_register_variables(NeoMutt->sub->cs, Vars));

  {
    struct ConfigSubset *sub = cs_subset_new("account", NULL, NULL);
    struct Account *a = account_new("apple", sub);
    TEST_CHECK(a != NULL);

    TEST_CHECK(account_mailbox_add(a, NULL) == false);

    account_free(&a);
    cs_subset_free(&sub);
  }

  {
    struct ConfigSubset *sub = cs_subset_new("account", NULL, NULL);
    struct Account *a = account_new("banana", sub);
    TEST_CHECK(a != NULL);

    struct Mailbox *m = mailbox_new();

    TEST_CHECK(account_mailbox_add(a, m) == true);

    account_free(&a);
    cs_subset_free(&sub);
  }
}
