/**
 * @file
 * Test code for account_free()
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
#include "mutt/lib.h"
#include "core/lib.h"

void free_adata(void **ptr)
{
  FREE(ptr);
}

void test_account_free(void)
{
  // void account_free(struct Account **ptr);

  {
    account_free(NULL);
    TEST_CHECK_(1, "account_free(NULL)");
  }

  {
    struct Account *a = NULL;
    account_free(&a);
    TEST_CHECK_(1, "account_free(&a)");
  }

  {
    struct Account *a = mutt_mem_calloc(1, sizeof(*a));
    account_free(&a);
    TEST_CHECK_(1, "account_free(&a)");
  }

  {
    struct Account *a = mutt_mem_calloc(1, sizeof(*a));
    a->adata = mutt_mem_calloc(1, 32);
    a->free_adata = free_adata;

    account_free(&a);
    TEST_CHECK_(1, "account_free(&a)");
  }
}
