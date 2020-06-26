/**
 * @file
 * Common code for store tests
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
#include <stdbool.h>
#include "mutt/lib.h"
#include "test_common.h"
#include "store/lib.h"

bool test_store_setup(char *buf, size_t buflen)
{
  if (!buf)
    return false;

  test_gen_path(buf, buflen, "%s/tmp/XXXXXX");

  if (!mkdtemp(buf))
    return false;

  return true;
}

bool test_store_degenerate(const struct StoreOps *sops, const char *name)
{
  if (!sops)
    return false;

  if (!TEST_CHECK(mutt_str_equal(sops->name, name)))
    return false;

  if (!TEST_CHECK(sops->open(NULL) == NULL))
    return false;

  if (!TEST_CHECK(sops->fetch(NULL, NULL, 0, NULL) == NULL))
    return false;

  void *ptr = NULL;
  sops->free(NULL, NULL);
  TEST_CHECK_(1, "sops->free(NULL, NULL)");
  sops->free(NULL, &ptr);
  TEST_CHECK_(1, "sops->free(NULL, ptr)");

  if (!TEST_CHECK(sops->store(NULL, NULL, 0, NULL, 0) != 0))
    return false;

  if (!TEST_CHECK(sops->delete_record(NULL, NULL, 0) != 0))
    return false;

  sops->close(NULL);
  TEST_CHECK_(1, "sops->close(NULL)");

  sops->close(&ptr);
  TEST_CHECK_(1, "sops->close(&ptr)");

  if (!TEST_CHECK(sops->version() != NULL))
    return false;

  return true;
}

bool test_store_db(const struct StoreOps *sops, void *db)
{
  if (!sops || !db)
    return false;

  const char *key = "one";
  size_t klen = strlen(key);
  char *value = "abcdefghijklmnopqrstuvwxyz";
  size_t vlen = strlen(value);
  int rc;

  rc = sops->store(db, key, klen, value, vlen);
  if (!TEST_CHECK(rc == 0))
    return false;

  void *data = NULL;
  vlen = 0;
  data = sops->fetch(db, key, klen, &vlen);
  if (!TEST_CHECK(data != NULL))
    return false;

  sops->free(db, &data);
  TEST_CHECK_(1, "sops->free(db, &data)");

  rc = sops->delete_record(db, key, klen);
  if (!TEST_CHECK(rc == 0))
    return false;

  return true;
}
