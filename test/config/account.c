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
#include "config/account.h"
#include "config/common.h"
#include "config/lib.h"

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
  err.data = mutt_mem_calloc(1, STRING);
  err.dsize = STRING;
  mutt_buffer_reset(&err);

  struct ConfigSet *cs = cs_create(30);

  number_init(cs);
  if (!TEST_CHECK(cs_register_variables(cs, Vars, 0)))
    return;

  set_list(cs);

  cs_add_listener(cs, log_listener);

  const char *account = "damaged";
  const char *BrokenVarStr[] = {
    "Pineapple",
    NULL,
  };

  struct Account *ac = ac_create(cs, account, BrokenVarStr);
  if (TEST_CHECK(!ac))
  {
    TEST_MSG("Expected error:\n");
  }
  else
  {
    ac_free(cs, &ac);
    TEST_MSG("This test should have failed\n");
    return;
  }

  const char *AccountVarStr2[] = {
    "Apple",
    "Apple",
    NULL,
  };

  TEST_MSG("Expect error for next test\n");
  ac = ac_create(cs, account, AccountVarStr2);
  if (!TEST_CHECK(!ac))
  {
    ac_free(cs, &ac);
    TEST_MSG("This test should have failed\n");
    return;
  }

  account = "fruit";
  const char *AccountVarStr[] = {
    "Apple",
    "Cherry",
    NULL,
  };

  ac = ac_create(cs, account, AccountVarStr);
  if (!TEST_CHECK(ac != NULL))
    return;

  size_t index = 0;
  mutt_buffer_reset(&err);
  int rc = ac_set_value(ac, index, 33, &err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err.data);
  }

  mutt_buffer_reset(&err);
  rc = ac_set_value(ac, 99, 42, &err);
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
  rc = ac_get_value(ac, index, &err);
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
  rc = ac_get_value(ac, index, &err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err.data);
  }
  else
  {
    TEST_MSG("%s = %s\n", AccountVarStr[index], err.data);
  }

  mutt_buffer_reset(&err);
  rc = ac_get_value(ac, 99, &err);
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
    TEST_MSG("%s\n", err.data);
    return;
  }

  mutt_buffer_reset(&err);
  rc = cs_str_initial_get(cs, name, &err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error\n");
  }
  else
  {
    TEST_MSG("This test should have failed\n");
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

  ac_free(cs, &ac);
  cs_free(&cs);
  FREE(&err.data);
  log_line(__func__);
}
