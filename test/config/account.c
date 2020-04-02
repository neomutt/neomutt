/**
 * @file
 * Test code for the Account object
 *
 * @authors
 * Copyright (C) 2017-2018 Richard Russon <rich@flatcap.org>
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
#include <stdio.h>
#include "mutt/lib.h"
#include "config/common.h"
#include "config/lib.h"
#include "core/lib.h"

static short VarApple;
static short VarBanana;
static short VarCherry;

// clang-format off
static struct ConfigDef Vars[] = {
  { "Apple",  DT_NUMBER, &VarApple,  0, 0, NULL },
  { "Banana", DT_NUMBER, &VarBanana, 0, 0, NULL },
  { "Cherry", DT_NUMBER, &VarCherry, 0, 0, NULL },
  { NULL },
};
// clang-format on

void test_config_account(void)
{
  log_line(__func__);

  struct Buffer err;
  mutt_buffer_init(&err);
  err.dsize = 256;
  err.data = mutt_mem_calloc(1, err.dsize);
  mutt_buffer_reset(&err);

  struct ConfigSet *cs = cs_new(30);
  NeoMutt = neomutt_new(cs);
  int rc = 0;

  number_init(cs);
  if (!TEST_CHECK(cs_register_variables(cs, Vars, 0)))
    return;

  set_list(cs);

  notify_observer_add(NeoMutt->notify, log_observer, 0);

  const char *account = "damaged";
  const char *parent = "Pineapple";

  struct ConfigSubset *sub = cs_subset_new(NULL, NULL, NeoMutt->notify);
  sub->cs = cs;
  struct Account *a = account_new(account, sub);

  struct HashElem *he = cs_subset_create_inheritance(a->sub, parent);

  account_free(&a);

  if (he)
  {
    TEST_MSG("This test should have failed\n");
    return;
  }
  else
  {
    TEST_MSG("Expected error:\n");
  }
  account_free(&a);

  account = "fruit";
  a = account_new(account, sub);

  struct HashElem *he1 = cs_subset_create_inheritance(a->sub, "Apple");
  struct HashElem *he2 = cs_subset_create_inheritance(a->sub, "Apple");
  if (!he1 || !he2 || (he1 != he2))
  {
    TEST_MSG("%s\n", err.data);
    return;
  }

  account_free(&a);

  a = account_new(account, sub);

  he = cs_subset_create_inheritance(NULL, "Apple");
  if (he)
    return;
  he = cs_subset_create_inheritance(a->sub, NULL);
  if (he)
    return;

  he = cs_subset_create_inheritance(a->sub, "Apple");
  if (!he)
    return;

  he = cs_subset_create_inheritance(a->sub, "Cherry");
  if (!he)
    return;

  he = cs_subset_lookup(a->sub, "Apple");
  mutt_buffer_reset(&err);

  rc = cs_subset_he_native_set(NULL, he, 33, &err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", err.data);
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return;
  }

  rc = cs_subset_he_native_set(a->sub, NULL, 33, &err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", err.data);
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return;
  }

  rc = cs_subset_he_native_set(a->sub, he, 33, &err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err.data);
  }

  mutt_buffer_reset(&err);
  rc = cs_subset_he_string_get(a->sub, he, &err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s = %s\n", he->key.strkey, err.data);
  }
  else
  {
    TEST_MSG("%s\n", err.data);
  }

  he = cs_subset_lookup(a->sub, "Cherry");
  mutt_buffer_reset(&err);
  rc = cs_subset_he_string_get(a->sub, he, &err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s = %s\n", he->key.strkey, err.data);
  }
  else
  {
    TEST_MSG("%s\n", err.data);
  }

  const char *name = "fruit:Apple";
  mutt_buffer_reset(&err);
  rc = cs_str_string_get(cs, name, &err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s = '%s'\n", name, err.data);
  }
  else
  {
    TEST_MSG("%s\n", err.data);
    return;
  }

  mutt_buffer_reset(&err);
  rc = cs_str_native_set(cs, name, 42, &err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Set %s\n", name);
  }
  else
  {
    TEST_MSG("%s\n", err.data);
    return;
  }

  he = cs_get_elem(cs, name);
  if (!TEST_CHECK(he != NULL))
    return;

  mutt_buffer_reset(&err);
  rc = cs_str_initial_set(cs, name, "42", &err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error\n");
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return;
  }

  mutt_buffer_reset(&err);
  rc = cs_str_initial_get(cs, name, &err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Initial %s\n", err.data);
  }
  else
  {
    TEST_MSG("%s\n", err.data);
    return;
  }

  name = "Apple";
  he = cs_get_elem(cs, name);
  if (!TEST_CHECK(he != NULL))
    return;

  mutt_buffer_reset(&err);
  rc = cs_he_native_set(cs, he, 42, &err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Set %s\n", name);
  }
  else
  {
    TEST_MSG("%s\n", err.data);
    return;
  }

  account_free(NULL);

  account_free(&a);
  cs_subset_free(&sub);
  neomutt_free(&NeoMutt);
  cs_free(&cs);
  FREE(&err.data);
  log_line(__func__);
}
