/**
 * @file
 * Test code for the qdbm store
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
#include <limits.h>
#include "mutt/lib.h"
#include "common.h"
#include "store/lib.h"

#define DB_NAME "qdbm"

void test_store_qdbm(void)
{
  char path[PATH_MAX];

  const struct StoreOps *sops = store_get_backend_ops(DB_NAME);
  TEST_CHECK(sops != NULL);

  TEST_CHECK(test_store_degenerate(sops, DB_NAME) == true);

  TEST_CHECK(test_store_setup(path, sizeof(path)) == true);

  mutt_str_cat(path, sizeof(path), "/");
  mutt_str_cat(path, sizeof(path), DB_NAME);

  void *db = sops->open(path);
  TEST_CHECK(db != NULL);

  TEST_CHECK(test_store_db(sops, db) == true);

  sops->close(&db);
}
