/**
 * @file
 * Test code for the rocksdb store
 *
 * @authors
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
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
#include "mutt/lib.h"
#include "store/lib.h"
#include "common.h" // IWYU pragma: keep

#define DB_NAME "rocksdb"

void test_store_rocksdb(void)
{
  struct Buffer *path = buf_pool_get();

  const struct StoreOps *store_ops = store_get_backend_ops(DB_NAME);
  TEST_CHECK(store_ops != NULL);

  TEST_CHECK(test_store_degenerate(store_ops, DB_NAME) == true);

  TEST_CHECK(test_store_setup(path) == true);

  buf_addch(path, '/');
  buf_addstr(path, DB_NAME);

  StoreHandle *store_handle = store_ops->open(buf_string(path), true);
  TEST_CHECK(store_handle != NULL);

  TEST_CHECK(test_store_db(store_ops, store_handle) == true);

  store_ops->close(&store_handle);

  buf_pool_release(&path);
}
