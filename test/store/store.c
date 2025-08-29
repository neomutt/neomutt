/**
 * @file
 * Test code for the store
 *
 * @authors
 * Copyright (C) 2020-2025 Richard Russon <rich@flatcap.org>
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
#include "mutt/lib.h"
#include "store/lib.h"

void test_store_store(void)
{
  struct Slist *list = store_backend_list();
  TEST_CHECK(list != NULL);
  slist_free(&list);

  const struct StoreOps *store_ops = NULL;

  store_ops = store_get_backend_ops(NULL);
  TEST_CHECK(store_ops != NULL);

#ifdef HAVE_TC
  store_ops = store_get_backend_ops("tokyocabinet");
  TEST_CHECK(store_ops != NULL);
#endif

  TEST_CHECK(store_is_valid_backend(NULL) == true);

#ifdef HAVE_TC
  TEST_CHECK(store_is_valid_backend("tokyocabinet") == true);
#endif
}
