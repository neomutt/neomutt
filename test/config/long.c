/**
 * @file
 * Test code for the Long object
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
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/buffer.h"
#include "mutt/memory.h"
#include "mutt/string2.h"
#include "config/account.h"
#include "config/common.h"
#include "config/long.h"
#include "config/set.h"
#include "config/types.h"

static short VarApple;
static short VarBanana;
static short VarCherry;
static short VarDamson;
static short VarElderberry;
static short VarFig;
static short VarGuava;
static short VarHawthorn;
static short VarIlama;
static short VarJackfruit;
static short VarKumquat;
static short VarLemon;
static short VarMango;
static short VarNectarine;
static short VarOlive;

// clang-format off
static struct ConfigDef Vars[] = {
  { "Apple",      DT_LONG,                 0, &VarApple,      -42, NULL              }, /* test_initial_values */
  { "Banana",     DT_LONG,                 0, &VarBanana,     99,  NULL              },
  { "Cherry",     DT_LONG,                 0, &VarCherry,     33,  NULL              },
  { "Damson",     DT_LONG,                 0, &VarDamson,     0,   NULL              }, /* test_string_set */
  { "Elderberry", DT_LONG|DT_NOT_NEGATIVE, 0, &VarElderberry, 0,   NULL              },
  { "Fig",        DT_LONG,                 0, &VarFig,        0,   NULL              }, /* test_string_get */
  { "Guava",      DT_LONG,                 0, &VarGuava,      0,   NULL              }, /* test_native_set */
  { "Hawthorn",   DT_LONG|DT_NOT_NEGATIVE, 0, &VarHawthorn,   0,   NULL              },
  { "Ilama",      DT_LONG,                 0, &VarIlama,      0,   NULL              }, /* test_native_get */
  { "Jackfruit",  DT_LONG,                 0, &VarJackfruit,  99,  NULL              }, /* test_reset */
  { "Kumquat",    DT_LONG,                 0, &VarKumquat,    33,  validator_fail    },
  { "Lemon",      DT_LONG,                 0, &VarLemon,      0,   validator_succeed }, /* test_validator */
  { "Mango",      DT_LONG,                 0, &VarMango,      0,   validator_warn    },
  { "Nectarine",  DT_LONG,                 0, &VarNectarine,  0,   validator_fail    },
  { "Olive",      DT_LONG,                 0, &VarOlive,      0,   NULL              }, /* test_inherit */
  { NULL },
};
// clang-format on

static bool test_initial_values(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  TEST_MSG("Apple = %d\n", VarApple);
  TEST_MSG("Banana = %d\n", VarBanana);

  if ((VarApple != -42) || (VarBanana != 99))
  {
    TEST_MSG("Error: initial values were wrong\n");
    return false;
  }

  cs_str_string_set(cs, "Apple", "2001", err);
  cs_str_string_set(cs, "Banana", "1999", err);

  struct Buffer value;
  mutt_buffer_init(&value);
  value.data = mutt_mem_calloc(1, STRING);
  value.dsize = STRING;
  mutt_buffer_reset(&value);

  int rc;

  mutt_buffer_reset(&value);
  rc = cs_str_initial_get(cs, "Apple", &value);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  if (mutt_str_strcmp(value.data, "-42") != 0)
  {
    TEST_MSG("Apple's initial value is wrong: '%s'\n", value.data);
    FREE(&value.data);
    return false;
  }
  TEST_MSG("Apple = %d\n", VarApple);
  TEST_MSG("Apple's initial value is '%s'\n", value.data);

  mutt_buffer_reset(&value);
  rc = cs_str_initial_get(cs, "Banana", &value);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  if (mutt_str_strcmp(value.data, "99") != 0)
  {
    TEST_MSG("Banana's initial value is wrong: '%s'\n", value.data);
    FREE(&value.data);
    return false;
  }
  TEST_MSG("Banana = %d\n", VarBanana);
  TEST_MSG("Banana's initial value is '%s'\n", NONULL(value.data));

  mutt_buffer_reset(&value);
  rc = cs_str_initial_set(cs, "Cherry", "123", &value);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  mutt_buffer_reset(&value);
  rc = cs_str_initial_get(cs, "Cherry", &value);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  TEST_MSG("Cherry = %d\n", VarCherry);
  TEST_MSG("Cherry's initial value is %s\n", value.data);

  FREE(&value.data);
  return true;
}

static bool test_string_set(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  const char *valid[] = { "-123", "0", "-42", "456" };
  int longs[] = { -123, 0, -42, 456 };
  const char *invalid[] = { "-2147483648", "2147483648", "junk", "", NULL };

  char *name = "Damson";

  int rc;
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    VarDamson = -42;

    mutt_buffer_reset(err);
    rc = cs_str_string_set(cs, name, valid[i], err);
    if (CSR_RESULT(rc) != CSR_SUCCESS)
    {
      TEST_MSG("%s\n", err->data);
      return false;
    }

    if (rc & CSR_SUC_NO_CHANGE)
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      continue;
    }

    if (VarDamson != longs[i])
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      return false;
    }
    TEST_MSG("%s = %d, set by '%s'\n", name, VarDamson, valid[i]);
  }

  TEST_MSG("\n");
  for (unsigned int i = 0; i < mutt_array_size(invalid); i++)
  {
    mutt_buffer_reset(err);
    rc = cs_str_string_set(cs, name, invalid[i], err);
    if (CSR_RESULT(rc) != CSR_SUCCESS)
    {
      TEST_MSG("Expected error: %s\n", err->data);
    }
    else
    {
      TEST_MSG("%s = %d, set by '%s'\n", name, VarDamson, invalid[i]);
      TEST_MSG("This test should have failed\n");
      // return false;
    }
  }

  name = "Elderberry";
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "-42", err);
  if (CSR_RESULT(rc) == CSR_SUCCESS)
  {
    TEST_MSG("This test should have failed\n");
    // return false;
  }
  else
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }

  return true;
}

static bool test_string_get(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  const char *name = "Fig";

  VarFig = 123;
  mutt_buffer_reset(err);
  int rc = cs_str_string_get(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Get failed: %s\n", err->data);
    return false;
  }
  TEST_MSG("%s = %d, %s\n", name, VarFig, err->data);

  VarFig = -789;
  mutt_buffer_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Get failed: %s\n", err->data);
    return false;
  }
  TEST_MSG("%s = %d, %s\n", name, VarFig, err->data);

  return true;
}

static bool test_native_set(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  char *name = "Guava";
  short value = 12345;

  VarGuava = 0;
  mutt_buffer_reset(err);
  int rc = cs_str_native_set(cs, name, value, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  if (VarGuava != value)
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    return false;
  }

  TEST_MSG("%s = %d, set to '%d'\n", name, VarGuava, value);

  rc = cs_str_native_set(cs, name, value, err);
  if (rc & CSR_SUC_NO_CHANGE)
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return false;
  }

  name = "Hawthorn";
  rc = cs_str_native_set(cs, name, -42, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return false;
  }

  int invalid[] = { -32769, 32768 };
  for (unsigned int i = 0; i < mutt_array_size(invalid); i++)
  {
    VarGuava = 123;
    mutt_buffer_reset(err);
    rc = cs_str_native_set(cs, name, invalid[i], err);
    if (CSR_RESULT(rc) != CSR_SUCCESS)
    {
      TEST_MSG("Expected error: %s\n", err->data);
    }
    else
    {
      TEST_MSG("%s = %d, set by '%d'\n", name, VarGuava, invalid[i]);
      TEST_MSG("This test should have failed\n");
      // return false;
    }
  }

  return true;
}

static bool test_native_get(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  char *name = "Ilama";

  VarIlama = 3456;
  mutt_buffer_reset(err);
  intptr_t value = cs_str_native_get(cs, name, err);
  if (value == INT_MIN)
  {
    TEST_MSG("Get failed: %s\n", err->data);
    return false;
  }
  TEST_MSG("%s = %ld\n", name, value);

  return true;
}

static bool test_reset(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  char *name = "Jackfruit";
  VarJackfruit = 345;
  mutt_buffer_reset(err);

  int rc = cs_str_reset(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  if (VarJackfruit == 345)
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = %d\n", name, VarJackfruit);

  name = "Kumquat";
  mutt_buffer_reset(err);

  TEST_MSG("Initial: %s = %d\n", name, VarKumquat);
  dont_fail = true;
  rc = cs_str_string_set(cs, name, "99", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
    return false;
  TEST_MSG("Set: %s = %d\n", name, VarKumquat);
  dont_fail = false;

  rc = cs_str_reset(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  if (VarKumquat != 99)
  {
    TEST_MSG("Value of %s changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = %d\n", name, VarKumquat);

  return true;
}

static bool test_validator(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  char *name = "Lemon";
  VarLemon = 123;
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, name, "456", err);
  if (CSR_RESULT(rc) == CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("String: %s = %d\n", name, VarLemon);

  VarLemon = 456;
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, 123, err);
  if (CSR_RESULT(rc) == CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("Native: %s = %d\n", name, VarLemon);

  name = "Mango";
  VarMango = 123;
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "456", err);
  if (CSR_RESULT(rc) == CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("String: %s = %d\n", name, VarMango);

  VarMango = 456;
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, 123, err);
  if (CSR_RESULT(rc) == CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("Native: %s = %d\n", name, VarMango);

  name = "Nectarine";
  VarNectarine = 123;
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "456", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("String: %s = %d\n", name, VarNectarine);

  VarNectarine = 456;
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, 123, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("Native: %s = %d\n", name, VarNectarine);

  return true;
}

static void dump_native(struct ConfigSet *cs, const char *parent, const char *child)
{
  intptr_t pval = cs_str_native_get(cs, parent, NULL);
  intptr_t cval = cs_str_native_get(cs, child, NULL);

  TEST_MSG("%15s = %ld\n", parent, pval);
  TEST_MSG("%15s = %ld\n", child, cval);
}

static bool test_inherit(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  bool result = false;

  const char *account = "fruit";
  const char *parent = "Olive";
  char child[128];
  snprintf(child, sizeof(child), "%s:%s", account, parent);

  const char *AccountVarStr[] = {
    parent, NULL,
  };

  struct Account *ac = ac_create(cs, account, AccountVarStr);

  // set parent
  VarOlive = 123;
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, parent, "456", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // set child
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, child, "-99", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // reset child
  mutt_buffer_reset(err);
  rc = cs_str_reset(cs, child, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // reset parent
  mutt_buffer_reset(err);
  rc = cs_str_reset(cs, parent, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);

  result = true;
ti_out:
  ac_free(cs, &ac);
  return result;
}

void config_long(void)
{
  log_line(__func__);

  struct Buffer err;
  mutt_buffer_init(&err);
  err.data = mutt_mem_calloc(1, STRING);
  err.dsize = STRING;
  mutt_buffer_reset(&err);

  struct ConfigSet *cs = cs_create(30);

  long_init(cs);
  dont_fail = true;
  if (!cs_register_variables(cs, Vars, 0))
    return;
  dont_fail = false;

  cs_add_listener(cs, log_listener);

  set_list(cs);

  test_initial_values(cs, &err);
  test_string_set(cs, &err);
  test_string_get(cs, &err);
  test_native_set(cs, &err);
  test_native_get(cs, &err);
  test_reset(cs, &err);
  test_validator(cs, &err);
  test_inherit(cs, &err);

  cs_free(&cs);
  FREE(&err.data);
}
