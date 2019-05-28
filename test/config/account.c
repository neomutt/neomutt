/**
 * @file
 * Test code for the CfgAccount object
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

  struct CfgAccount *ac = ac_new(cs, account, BrokenVarStr);
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

  const char *CfgAccountVarStr2[] = {
    "Apple",
    "Apple",
    NULL,
  };

  TEST_MSG("Expect error for next test\n");
  ac = ac_new(cs, account, CfgAccountVarStr2);
  if (!TEST_CHECK(!ac))
  {
    ac_free(cs, &ac);
    TEST_MSG("This test should have failed\n");
    return;
  }

  account = "fruit";
  const char *CfgAccountVarStr[] = {
    "Apple",
    "Cherry",
    NULL,
  };

  ac = ac_new(NULL, account, CfgAccountVarStr);
  if (!TEST_CHECK(ac == NULL))
    return;

  ac = ac_new(cs, NULL, CfgAccountVarStr);
  if (!TEST_CHECK(ac == NULL))
    return;

  ac = ac_new(cs, account, NULL);
  if (!TEST_CHECK(ac == NULL))
    return;

  ac = ac_new(cs, account, CfgAccountVarStr);
  if (!TEST_CHECK(ac != NULL))
    return;

  size_t index = 0;
  mutt_buffer_reset(&err);
  int rc = ac_set_value(NULL, index, 33, &err);

  rc = ac_set_value(ac, index, 33, &err);
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
  rc = ac_get_value(NULL, index, &err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_ERR_CODE))
  {
    TEST_MSG("%s\n", err.data);
  }

  rc = ac_get_value(ac, index, &err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err.data);
  }
  else
  {
    TEST_MSG("%s = %s\n", CfgAccountVarStr[index], err.data);
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
    TEST_MSG("%s = %s\n", CfgAccountVarStr[index], err.data);
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

  ac_free(NULL, &ac);
  ac_free(cs, NULL);

  ac_free(cs, &ac);
  cs_free(&cs);
  FREE(&err.data);
  log_line(__func__);
}
