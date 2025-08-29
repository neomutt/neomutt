/**
 * @file
 * Common code for store tests
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // IWYU pragma: keep
#include "mutt/lib.h"
#include "store/lib.h"
#include "test_common.h"

bool test_store_setup(struct Buffer *path)
{
  if (!path)
    return false;

  test_gen_path(path, "%s/tmp/XXXXXX");

  if (!mkdtemp(path->data))
    return false;

  return true;
}

bool test_store_degenerate(const struct StoreOps *store_ops, const char *name)
{
  if (!store_ops)
    return false;

  if (!TEST_CHECK_STR_EQ(store_ops->name, name))
    return false;

  if (!TEST_CHECK(store_ops->open(NULL, false) == NULL))
    return false;

  if (!TEST_CHECK(store_ops->fetch(NULL, NULL, 0, NULL) == NULL))
    return false;

  void *ptr = NULL;
  store_ops->free(NULL, NULL);
  TEST_CHECK_(1, "store_ops->free(NULL, NULL)");
  store_ops->free(NULL, &ptr);
  TEST_CHECK_(1, "store_ops->free(NULL, ptr)");

  if (!TEST_CHECK(store_ops->store(NULL, NULL, 0, NULL, 0) != 0))
    return false;

  if (!TEST_CHECK(store_ops->delete_record(NULL, NULL, 0) != 0))
    return false;

  store_ops->close(NULL);
  TEST_CHECK_(1, "store_ops->close(NULL)");

  store_ops->close(&ptr);
  TEST_CHECK_(1, "store_ops->close(&ptr)");

  if (!TEST_CHECK(store_ops->version() != NULL))
    return false;

  return true;
}

bool test_store_db(const struct StoreOps *store_ops, StoreHandle *store_handle)
{
  if (!store_ops || !store_handle)
    return false;

  const char *key = "one";
  size_t klen = strlen(key);
  char *value = "abcdefghijklmnopqrstuvwxyz";
  size_t vlen = strlen(value);
  int rc;

  rc = store_ops->store(store_handle, key, klen, value, vlen);
  if (!TEST_CHECK(rc == 0))
    return false;

  void *data = NULL;
  vlen = 0;
  data = store_ops->fetch(store_handle, key, klen, &vlen);
  if (!TEST_CHECK(data != NULL))
    return false;

  store_ops->free(store_handle, &data);
  TEST_CHECK_(1, "store_ops->free(store_handle, &data)");

  rc = store_ops->delete_record(store_handle, key, klen);
  if (!TEST_CHECK(rc == 0))
    return false;

  return true;
}
