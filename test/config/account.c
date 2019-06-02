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
#include "acutest.h"
#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/mutt.h"
#include "config/common.h"
#include "config/lib.h"
#include "account.h"

static short VarApple;
static short VarBanana;
static short VarCherry;

// clang-format off
static struct ConfigDef Vars[] = {
  { "Apple",  DT_NUMBER, 0, &VarApple,  0, NULL },
  { "Banana", DT_NUMBER, 0, &VarBanana, 0, NULL },
  { "Cherry", DT_NUMBER, 0, &VarCherry, 0, NULL },
  { NULL },
};
// clang-format on

void config_account(void)
{
  log_line(__func__);

  struct Buffer err;
  mutt_buffer_init(&err);
  err.dsize = 256;
  err.data = mutt_mem_calloc(1, err.dsize);
  mutt_buffer_reset(&err);

  struct ConfigSet *cs = cs_new(30);

  number_init(cs);
  if (!TEST_CHECK(cs_register_variables(cs, Vars, 0)))
    return;

  set_list(cs);

  notify_observer_add(cs->notify, NT_CONFIG, 0, log_observer, 0);

  const char *account = "damaged";
  const char *BrokenVarStr[] = {
    "Pineapple",
    NULL,
  };

  struct Account *a = account_new();
  bool result = account_add_config(a, cs, account, BrokenVarStr);
  account_free(&a);

  if (TEST_CHECK(!result))
  {
    TEST_MSG("Expected error:\n");
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return;
  }

  const char *AccountVarStr2[] = {
    "Apple",
    "Apple",
    NULL,
  };

  TEST_MSG("Expect error for next test\n");
  a = account_new();
  result = account_add_config(a, cs, account, AccountVarStr2);
  account_free(&a);

  if (!TEST_CHECK(!result))
  {
    TEST_MSG("This test should have failed\n");
    return;
  }

  account = "fruit";
  const char *AccountVarStr[] = {
    "Apple",
    "Cherry",
    NULL,
  };

  a = account_new();

  result = account_add_config(NULL, cs, account, AccountVarStr);
  if (!TEST_CHECK(!result))
    return;

  result = account_add_config(a, NULL, account, AccountVarStr);
  if (!TEST_CHECK(!result))
    return;

  result = account_add_config(a, cs, NULL, AccountVarStr);
  if (!TEST_CHECK(!result))
    return;

  result = account_add_config(a, cs, account, NULL);
  if (!TEST_CHECK(!result))
    return;

  result = account_add_config(a, cs, account, AccountVarStr);
  if (!TEST_CHECK(result))
    return;

  size_t index = 0;
  mutt_buffer_reset(&err);
  int rc = account_set_value(NULL, index, 33, &err);

  rc = account_set_value(a, index, 33, &err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err.data);
  }

  mutt_buffer_reset(&err);
  rc = account_set_value(a, 99, 42, &err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_ERR_UNKNOWN))
  {
    TEST_MSG("Expected error: %s\n", err.data);
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return;
  }

  mutt_buffer_reset(&err);
  rc = account_get_value(NULL, index, &err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_ERR_CODE))
  {
    TEST_MSG("%s\n", err.data);
  }

  rc = account_get_value(a, index, &err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err.data);
  }
  else
  {
    TEST_MSG("%s = %s\n", AccountVarStr[index], err.data);
  }

  index++;
  mutt_buffer_reset(&err);
  rc = account_get_value(a, index, &err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err.data);
  }
  else
  {
    TEST_MSG("%s = %s\n", AccountVarStr[index], err.data);
  }

  mutt_buffer_reset(&err);
  rc = account_get_value(a, 99, &err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_ERR_UNKNOWN))
  {
    TEST_MSG("Expected error\n");
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return;
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

  struct HashElem *he = cs_get_elem(cs, name);
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
  cs_free(&cs);
  FREE(&err.data);
  log_line(__func__);
}
