/**
 * @file
 * Test code for the ConfigSubset object
 *
 * @authors
 * Copyright (C) 2020 Jakub Jindra <jakub.jindra@socialbakers.com>
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
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "common.h" // IWYU pragma: keep
#include "test_common.h"

// clang-format off
static struct ConfigDef Vars[] = {
  { "Apple",  DT_NUMBER,  42, 0, NULL, },
  { NULL },
};
// clang-format on

void test_config_subset(void)
{
  log_line(__func__);

  const char *name = "Apple";
  const char *expected = NULL;
  intptr_t value;
  int rc;

  struct ConfigSet *cs = cs_new(30);
  cs_register_type(cs, &CstNumber);
  if (!TEST_CHECK(cs_register_variables(cs, Vars)))
    return;

  struct NeoMutt *n = neomutt_new(cs);

  cs_subset_free(NULL);

  struct ConfigSubset *sub_a = cs_subset_new("account", n->sub, n->notify);
  sub_a->cs = cs;
  struct ConfigSubset *sub_m = cs_subset_new("mailbox", sub_a, sub_a->notify);
  sub_m->cs = cs;

  cs_subset_notify_observers(NULL, NULL, NT_CONFIG_SET);

  cs_subset_str_native_set(sub_m, name, 123, NULL);
  cs_subset_str_native_set(sub_a, name, 456, NULL);

  struct HashElem *he = NULL;

  he = cs_subset_lookup(NULL, NULL);
  if (!TEST_CHECK(he == NULL))
  {
    TEST_MSG("cs_subset_lookup failed");
    return;
  }

  he = cs_subset_lookup(n->sub, name);
  if (!TEST_CHECK(he != NULL))
  {
    TEST_MSG("cs_subset_lookup failed");
    return;
  }

  struct Buffer *err = buf_pool_get();
  buf_reset(err);
  rc = cs_subset_he_native_get(NULL, NULL, err);
  if (!TEST_CHECK(rc == INT_MIN))
  {
    TEST_MSG("This test should have failed");
    return;
  }

  buf_reset(err);
  value = cs_subset_he_native_get(n->sub, he, err);
  if (!TEST_CHECK(value != INT_MIN))
  {
    TEST_MSG("cs_subset_he_native_get failed");
    return;
  }

  buf_reset(err);
  rc = cs_subset_he_native_set(NULL, NULL, value + 100, err);
  if (!TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("cs_subset_he_native_set failed");
    return;
  }

  buf_reset(err);
  rc = cs_subset_he_native_set(n->sub, he, value + 100, err);
  if (!TEST_CHECK(value != INT_MIN))
  {
    TEST_MSG("cs_subset_he_native_set failed");
    return;
  }

  buf_reset(err);
  rc = cs_subset_str_native_set(n->sub, name, value + 100, err);
  if (!TEST_CHECK(value != INT_MIN))
  {
    TEST_MSG("cs_subset_str_native_set failed");
    return;
  }

  buf_reset(err);
  expected = "142";
  rc = cs_subset_he_string_get(n->sub, he, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS) ||
      !TEST_CHECK_STR_EQ(buf_string(err), expected))
  {
    return;
  }

  buf_reset(err);
  rc = cs_subset_str_string_get(NULL, NULL, err);
  if (!TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("This test should have failed");
    return;
  }

  buf_reset(err);
  expected = "142";
  rc = cs_subset_str_string_get(n->sub, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS) ||
      !TEST_CHECK_STR_EQ(buf_string(err), expected))
  {
    return;
  }

  buf_reset(err);
  expected = "142";
  rc = cs_subset_he_string_set(NULL, NULL, expected, err);
  if (!TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("This test should have failed");
    return;
  }

  buf_reset(err);
  expected = "678";
  rc = cs_subset_he_string_set(n->sub, he, expected, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("cs_subset_he_string_set failed");
    return;
  }

  buf_reset(err);
  expected = "678";
  rc = cs_subset_str_string_set(n->sub, name, expected, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("cs_subset_str_string_set failed");
    return;
  }

  buf_reset(err);
  expected = "142";
  rc = cs_subset_he_string_plus_equals(NULL, NULL, expected, err);
  if (!TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("This test should have failed");
    return;
  }

  buf_reset(err);
  expected = "678";
  rc = cs_subset_he_string_plus_equals(n->sub, he, expected, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("cs_subset_he_string_plus_equals failed");
    return;
  }

  buf_reset(err);
  expected = "142";
  rc = cs_subset_he_string_minus_equals(NULL, NULL, expected, err);
  if (!TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("This test should have failed");
    return;
  }

  buf_reset(err);
  expected = "678";
  rc = cs_subset_he_string_minus_equals(n->sub, he, expected, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("cs_subset_he_string_minus_equals failed");
    return;
  }

  buf_reset(err);
  rc = cs_subset_he_reset(NULL, NULL, err);
  if (!TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("This test should have failed");
    return;
  }

  buf_reset(err);
  rc = cs_subset_he_reset(n->sub, he, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("cs_subset_he_reset failed");
    return;
  }

  he = cs_subset_lookup(sub_a, name);
  if (!TEST_CHECK(he != NULL))
  {
    TEST_MSG("cs_subset_lookup failed");
    return;
  }

  he = cs_subset_lookup(sub_m, name);
  if (!TEST_CHECK(he != NULL))
  {
    TEST_MSG("cs_subset_lookup failed");
    return;
  }

  rc = cs_subset_he_delete(NULL, NULL, err);
  TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS);

  he = cs_subset_lookup(sub_a, name);
  if (!TEST_CHECK(he != NULL))
  {
    TEST_MSG("cs_subset_lookup failed");
    return;
  }

  he = cs_subset_lookup(n->sub, name);
  if (!TEST_CHECK(he != NULL))
  {
    TEST_MSG("cs_subset_lookup failed");
    return;
  }

  cs_subset_free(&sub_m);
  cs_subset_free(&sub_a);

  neomutt_free(&n);
  cs_free(&cs);
  buf_pool_release(&err);
  log_line(__func__);
}
