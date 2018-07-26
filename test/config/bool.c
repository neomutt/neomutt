/**
 * @file
 * Test code for the Bool object
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
#include "config/bool.h"
#include "config/common.h"
#include "config/quad.h"
#include "config/set.h"
#include "config/types.h"

static bool VarApple;
static bool VarBanana;
static bool VarCherry;
static bool VarDamson;
static bool VarFig;
static bool VarGuava;
static bool VarHawthorn;
static bool VarJackfruit;
static bool VarKumquat;
static bool VarLemon;
static bool VarMango;
static char VarElderberry;
static char VarIlama;
static char VarNectarine;
static char VarOlive;

// clang-format off
static struct ConfigDef Vars[] = {
  { "Apple",      DT_BOOL, 0, &VarApple,      0, NULL              }, /* test_initial_values */
  { "Banana",     DT_BOOL, 0, &VarBanana,     1, NULL              },
  { "Cherry",     DT_BOOL, 0, &VarCherry,     0, NULL              },
  { "Damson",     DT_BOOL, 0, &VarDamson,     0, NULL              }, /* test_string_set */
  { "Elderberry", DT_BOOL, 0, &VarElderberry, 0, NULL              }, /* test_string_get */
  { "Fig",        DT_BOOL, 0, &VarFig,        0, NULL              }, /* test_native_set */
  { "Guava",      DT_BOOL, 0, &VarGuava,      0, NULL              }, /* test_native_get */
  { "Hawthorn",   DT_BOOL, 0, &VarHawthorn,   0, NULL              }, /* test_reset */
  { "Ilama",      DT_BOOL, 0, &VarIlama,      0, validator_fail    },
  { "Jackfruit",  DT_BOOL, 0, &VarJackfruit,  0, validator_succeed }, /* test_validator */
  { "Kumquat",    DT_BOOL, 0, &VarKumquat,    0, validator_warn    },
  { "Lemon",      DT_BOOL, 0, &VarLemon,      0, validator_fail    },
  { "Mango",      DT_BOOL, 0, &VarMango,      0, NULL              }, /* test_inherit */
  { "Nectarine",  DT_BOOL, 0, &VarNectarine,  0, NULL              }, /* test_toggle */
  { "Olive",      DT_QUAD, 0, &VarOlive,      0, NULL              },
  { NULL },
};
// clang-format on

static bool test_initial_values(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  TEST_MSG("Apple = %d\n", VarApple);
  TEST_MSG("Banana = %d\n", VarBanana);

  if ((VarApple != false) || (VarBanana != true))
  {
    TEST_MSG("Error: initial values were wrong\n");
    return false;
  }

  cs_str_string_set(cs, "Apple", "true", err);
  cs_str_string_set(cs, "Banana", "false", err);

  struct Buffer value;
  mutt_buffer_init(&value);
  value.data = mutt_mem_calloc(1, STRING);
  value.dsize = STRING;

  int rc;

  mutt_buffer_reset(&value);
  rc = cs_str_initial_get(cs, "Apple", &value);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  if (mutt_str_strcmp(value.data, "no") != 0)
  {
    TEST_MSG("Apple's initial value is wrong: '%s'\n", value.data);
    FREE(&value.data);
    return false;
  }
  TEST_MSG("Apple = '%s'\n", VarApple ? "yes" : "no");
  TEST_MSG("Apple's initial value is '%s'\n", value.data);

  mutt_buffer_reset(&value);
  rc = cs_str_initial_get(cs, "Banana", &value);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  if (mutt_str_strcmp(value.data, "yes") != 0)
  {
    TEST_MSG("Banana's initial value is wrong: '%s'\n", value.data);
    FREE(&value.data);
    return false;
  }
  TEST_MSG("Banana = '%s'\n", VarBanana ? "yes" : "no");
  TEST_MSG("Banana's initial value is '%s'\n", NONULL(value.data));

  mutt_buffer_reset(&value);
  rc = cs_str_initial_set(cs, "Cherry", "yes", &value);
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

  TEST_MSG("Cherry = '%s'\n", VarCherry ? "yes" : "no");
  TEST_MSG("Cherry's initial value is '%s'\n", NONULL(value.data));

  FREE(&value.data);
  return true;
}

static bool test_string_set(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  const char *valid[] = {
    "no", "yes", "n", "y", "false", "true", "0", "1", "off", "on",
  };
  const char *invalid[] = {
    "nope", "ye", "", NULL,
  };
  char *name = "Damson";

  int rc;
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    VarDamson = ((i + 1) % 2);

    mutt_buffer_reset(err);
    rc = cs_str_string_set(cs, name, valid[i], err);
    if (CSR_RESULT(rc) != CSR_SUCCESS)
    {
      TEST_MSG("%s\n", err->data);
      return false;
    }

    if (VarDamson != (i % 2))
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      return false;
    }
    TEST_MSG("%s = %d, set by '%s'\n", name, VarDamson, valid[i]);
  }

  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "yes", err);
  if (rc & CSR_SUC_NO_CHANGE)
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return false;
  }

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
      return false;
    }
  }

  return true;
}

static bool test_string_get(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  const char *name = "Elderberry";
  int rc;

  VarElderberry = false;
  mutt_buffer_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Get failed: %s\n", err->data);
    return false;
  }
  TEST_MSG("%s = %d, %s\n", name, VarElderberry, err->data);

  VarElderberry = true;
  mutt_buffer_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Get failed: %s\n", err->data);
    return false;
  }
  TEST_MSG("%s = %d, %s\n", name, VarElderberry, err->data);

  VarElderberry = 3;
  mutt_buffer_reset(err);
  TEST_MSG("Expect error for next test\n");
  rc = cs_str_string_get(cs, name, err);
  if (CSR_RESULT(rc) == CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
    // return false;
  }

  return true;
}

static bool test_native_set(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  char *name = "Fig";
  bool value = true;
  int rc;

  VarFig = false;
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, value, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  if (VarFig != value)
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    return false;
  }

  TEST_MSG("%s = %d, set to '%d'\n", name, VarFig, value);

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, value, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  if (rc & CSR_SUC_NO_CHANGE)
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
  }

  int invalid[] = { -1, 2 };
  for (unsigned int i = 0; i < mutt_array_size(invalid); i++)
  {
    VarFig = false;
    mutt_buffer_reset(err);
    rc = cs_str_native_set(cs, name, invalid[i], err);
    if (CSR_RESULT(rc) != CSR_SUCCESS)
    {
      TEST_MSG("Expected error: %s\n", err->data);
    }
    else
    {
      TEST_MSG("%s = %d, set by '%d'\n", name, VarFig, invalid[i]);
      TEST_MSG("This test should have failed\n");
      return false;
    }
  }

  return true;
}

static bool test_native_get(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  char *name = "Guava";

  VarGuava = true;
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

  char *name = "Hawthorn";
  VarHawthorn = true;
  mutt_buffer_reset(err);

  int rc = cs_str_reset(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  if (VarHawthorn == true)
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = %d\n", name, VarHawthorn);

  name = "Ilama";
  mutt_buffer_reset(err);

  TEST_MSG("Initial: %s = %d\n", name, VarIlama);
  dont_fail = true;
  rc = cs_str_string_set(cs, name, "yes", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
    return false;
  TEST_MSG("Set: %s = %d\n", name, VarIlama);
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

  if (!VarIlama)
  {
    TEST_MSG("Value of %s changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = %d\n", name, VarIlama);

  return true;
}

static bool test_validator(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  char *name = "Jackfruit";
  VarJackfruit = false;
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, name, "yes", err);
  if (CSR_RESULT(rc) == CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("String: %s = %d\n", name, VarJackfruit);

  VarJackfruit = false;
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, 1, err);
  if (CSR_RESULT(rc) == CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("Native: %s = %d\n", name, VarJackfruit);

  name = "Kumquat";
  VarKumquat = false;
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "yes", err);
  if (CSR_RESULT(rc) == CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("String: %s = %d\n", name, VarKumquat);

  VarKumquat = false;
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, 1, err);
  if (CSR_RESULT(rc) == CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("Native: %s = %d\n", name, VarKumquat);

  name = "Lemon";
  VarLemon = false;
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "yes", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("String: %s = %d\n", name, VarLemon);

  VarLemon = false;
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, 1, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("Native: %s = %d\n", name, VarLemon);

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
  const char *parent = "Mango";
  char child[128];
  snprintf(child, sizeof(child), "%s:%s", account, parent);

  const char *AccountVarStr[] = {
    parent, NULL,
  };

  struct Account *ac = ac_create(cs, account, AccountVarStr);

  // set parent
  VarMango = false;
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, parent, "1", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // set child
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, child, "0", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Error: %s\n", err->data);
    goto ti_out;
  }
  if (rc & CSR_SUC_NO_CHANGE)
  {
    TEST_MSG("Value of %s wasn't changed\n", parent);
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

static bool test_toggle(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  struct ToggleTest
  {
    bool before;
    bool after;
  };

  struct ToggleTest tests[] = { { false, true }, { true, false } };

  char *name = "Nectarine";
  int rc;

  struct HashElem *he = cs_get_elem(cs, name);
  if (!he)
    return false;

  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    bool before = tests[i].before;
    bool after = tests[i].after;
    TEST_MSG("test %zu\n", i);

    VarNectarine = before;
    mutt_buffer_reset(err);
    intptr_t value = cs_he_native_get(cs, he, err);
    if (value == INT_MIN)
    {
      TEST_MSG("Get failed: %s\n", err->data);
      return false;
    }

    bool copy = value;
    if (copy != before)
    {
      TEST_MSG("Initial value is wrong: %s\n", err->data);
      return false;
    }

    rc = bool_he_toggle(cs, he, err);
    if (CSR_RESULT(rc) != CSR_SUCCESS)
    {
      TEST_MSG("Toggle failed: %s\n", err->data);
      return false;
    }

    if (VarNectarine != after)
    {
      TEST_MSG("Toggle value is wrong: %s\n", err->data);
      return false;
    }
  }

  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    bool before = tests[i].before;
    bool after = tests[i].after;
    TEST_MSG("test %zu\n", i);

    VarNectarine = before;
    mutt_buffer_reset(err);
    intptr_t value = cs_he_native_get(cs, he, err);
    if (value == INT_MIN)
    {
      TEST_MSG("Get failed: %s\n", err->data);
      return false;
    }

    bool copy = value;
    if (copy != before)
    {
      TEST_MSG("Initial value is wrong: %s\n", err->data);
      return false;
    }

    rc = bool_str_toggle(cs, "Nectarine", err);
    if (CSR_RESULT(rc) != CSR_SUCCESS)
    {
      TEST_MSG("Toggle failed: %s\n", err->data);
      return false;
    }

    if (VarNectarine != after)
    {
      TEST_MSG("Toggle value is wrong: %s\n", err->data);
      return false;
    }
  }

  VarNectarine = 8;
  mutt_buffer_reset(err);
  rc = bool_he_toggle(cs, he, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }

  name = "Olive";
  he = cs_get_elem(cs, name);
  if (!he)
    return false;

  mutt_buffer_reset(err);
  rc = bool_he_toggle(cs, he, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }

  mutt_buffer_reset(err);
  rc = bool_str_toggle(cs, "unknown", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return false;
  }

  return true;
}

void config_bool(void)
{
  log_line(__func__);

  struct Buffer err;
  mutt_buffer_init(&err);
  err.data = mutt_mem_calloc(1, STRING);
  err.dsize = STRING;
  mutt_buffer_reset(&err);

  struct ConfigSet *cs = cs_create(30);

  bool_init(cs);
  quad_init(cs);
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
  test_toggle(cs, &err);

  cs_free(&cs);
  FREE(&err.data);
}
