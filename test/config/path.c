/**
 * @file
 * Test code for the Path object
 *
 * @authors
 * Copyright (C) 2020-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 наб <nabijaczleweli@nabijaczleweli.xyz>
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
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "common.h" // IWYU pragma: keep
#include "test_common.h"

// clang-format off
static struct ConfigDef Vars[] = {
  { "Apple",      DT_PATH,              IP "apple",      0, NULL,              }, /* test_initial_values */
  { "Banana",     DT_PATH,              IP "banana",     0, NULL,              },
  { "Cherry",     DT_PATH,              IP "cherry",     0, NULL,              },
  { "Damson",     DT_PATH,              0,               0, NULL,              }, /* test_string_set */
  { "Elderberry", DT_PATH,              IP "elderberry", 0, NULL,              },
  { "Fig",        DT_PATH|D_NOT_EMPTY,  IP "fig",        0, NULL,              },
  { "Guava",      DT_PATH,              0,               0, NULL,              }, /* test_string_get */
  { "Hawthorn",   DT_PATH,              IP "hawthorn",   0, NULL,              },
  { "Ilama",      DT_PATH,              0,               0, NULL,              },
  { "Jackfruit",  DT_PATH,              0,               0, NULL,              }, /* test_native_set */
  { "Kumquat",    DT_PATH,              IP "kumquat",    0, NULL,              },
  { "Lemon",      DT_PATH|D_NOT_EMPTY,  IP "lemon",      0, NULL,              },
  { "Mango",      DT_PATH,              0,               0, NULL,              }, /* test_native_get */
  { "Nectarine",  DT_PATH,              IP "nectarine",  0, NULL,              }, /* test_reset */
  { "Olive",      DT_PATH,              IP "olive",      0, validator_fail,    },
  { "Papaya",     DT_PATH,              IP "papaya",     0, validator_succeed, }, /* test_validator */
  { "Quince",     DT_PATH,              IP "quince",     0, validator_warn,    },
  { "Raspberry",  DT_PATH,              IP "raspberry",  0, validator_fail,    },
  { "Strawberry", DT_PATH,              0,               0, NULL,              }, /* test_inherit */
  { "Tangerine",  DT_PATH|D_ON_STARTUP, IP "tangerine",  0, NULL,              }, /* startup */
  { NULL },
};
// clang-format on

static bool test_initial_values(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *VarApple = cs_subset_path(sub, "Apple");
  const char *VarBanana = cs_subset_path(sub, "Banana");

  TEST_MSG("Apple = %s", VarApple);
  TEST_MSG("Banana = %s", VarBanana);

  if (!TEST_CHECK_STR_EQ(VarApple, "apple"))
  {
    TEST_MSG("Error: initial values were wrong");
    return false;
  }

  if (!TEST_CHECK_STR_EQ(VarBanana, "banana"))
  {
    TEST_MSG("Error: initial values were wrong");
    return false;
  }

  cs_str_string_set(cs, "Apple", "car", err);
  cs_str_string_set(cs, "Banana", NULL, err);

  VarApple = cs_subset_path(sub, "Apple");
  VarBanana = cs_subset_path(sub, "Banana");

  struct Buffer *value = buf_pool_get();

  int rc;

  buf_reset(value);
  rc = cs_str_initial_get(cs, "Apple", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(value));
    return false;
  }

  VarApple = cs_subset_path(sub, "Apple");
  if (!TEST_CHECK_STR_EQ(buf_string(value), "apple"))
  {
    TEST_MSG("Apple's initial value is wrong: '%s'", buf_string(value));
    return false;
  }
  TEST_MSG("Apple = '%s'", VarApple);
  TEST_MSG("Apple's initial value is '%s'", buf_string(value));

  buf_reset(value);
  rc = cs_str_initial_get(cs, "Banana", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(value));
    return false;
  }

  VarBanana = cs_subset_path(sub, "Banana");
  if (!TEST_CHECK_STR_EQ(buf_string(value), "banana"))
  {
    TEST_MSG("Banana's initial value is wrong: '%s'", buf_string(value));
    return false;
  }
  TEST_MSG("Banana = '%s'", VarBanana);
  TEST_MSG("Banana's initial value is '%s'", NONULL(buf_string(value)));

  buf_reset(value);
  rc = cs_str_initial_set(cs, "Cherry", (const char *) Vars[2].initial, value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(value));
    return false;
  }

  buf_reset(value);
  rc = cs_str_initial_set(cs, "Cherry", "train", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(value));
    return false;
  }

  buf_reset(value);
  rc = cs_str_initial_set(cs, "Cherry", "plane", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(value));
    return false;
  }

  buf_reset(value);
  rc = cs_str_initial_get(cs, "Cherry", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(value));
    return false;
  }

  const char *VarCherry = cs_subset_path(sub, "Cherry");
  TEST_MSG("Cherry = '%s'", VarCherry);
  TEST_MSG("Cherry's initial value is '%s'", NONULL(buf_string(value)));

  buf_pool_release(&value);
  log_line(__func__);
  return true;
}

static bool test_string_set(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *valid[] = { "hello", "world", "world", "", NULL };
  const char *name = "Damson";

  int rc;
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    buf_reset(err);
    rc = cs_str_string_set(cs, name, valid[i], err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("%s", buf_string(err));
      return false;
    }

    if (rc & CSR_SUC_NO_CHANGE)
    {
      TEST_MSG("Value of %s wasn't changed", name);
      continue;
    }

    const char *VarDamson = cs_subset_path(sub, "Damson");
    if (!TEST_CHECK_STR_EQ(VarDamson, valid[i]))
    {
      TEST_MSG("Value of %s wasn't changed", name);
      return false;
    }
    TEST_MSG("%s = '%s', set by '%s'", name, NONULL(VarDamson), NONULL(valid[i]));
    short_line();
  }

  name = "Fig";
  buf_reset(err);
  rc = cs_str_string_set(cs, name, "", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }

  name = "Elderberry";
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    short_line();
    buf_reset(err);
    rc = cs_str_string_set(cs, name, valid[i], err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("%s", buf_string(err));
      return false;
    }

    if (rc & CSR_SUC_NO_CHANGE)
    {
      TEST_MSG("Value of %s wasn't changed", name);
      continue;
    }

    const char *VarElderberry = cs_subset_path(sub, "Elderberry");
    if (!TEST_CHECK_STR_EQ(VarElderberry, valid[i]))
    {
      TEST_MSG("Value of %s wasn't changed", name);
      return false;
    }
    TEST_MSG("%s = '%s', set by '%s'", name, NONULL(VarElderberry), NONULL(valid[i]));
  }

  name = "Tangerine";
  rc = cs_str_string_set(cs, name, "tangerine", err);
  TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS);

  rc = cs_str_string_set(cs, name, "apple", err);
  TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS);

  log_line(__func__);
  return true;
}

static bool test_string_get(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;
  const char *name = "Guava";

  buf_reset(err);
  int rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s", buf_string(err));
    return false;
  }
  const char *VarGuava = cs_subset_path(sub, "Guava");
  TEST_MSG("%s = '%s', '%s'", name, NONULL(VarGuava), buf_string(err));

  name = "Hawthorn";
  buf_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s", buf_string(err));
    return false;
  }
  const char *VarHawthorn = cs_subset_path(sub, "Hawthorn");
  TEST_MSG("%s = '%s', '%s'", name, NONULL(VarHawthorn), buf_string(err));

  name = "Ilama";
  rc = cs_str_string_set(cs, name, "ilama", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;

  buf_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s", buf_string(err));
    return false;
  }
  const char *VarIlama = cs_subset_path(sub, "Ilama");
  TEST_MSG("%s = '%s', '%s'", name, NONULL(VarIlama), buf_string(err));

  log_line(__func__);
  return true;
}

static bool test_native_set(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *valid[] = { "hello", "world", "world", "", NULL };
  const char *name = "Jackfruit";

  int rc;
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    buf_reset(err);
    rc = cs_str_native_set(cs, name, (intptr_t) valid[i], err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("%s", buf_string(err));
      return false;
    }

    if (rc & CSR_SUC_NO_CHANGE)
    {
      TEST_MSG("Value of %s wasn't changed", name);
      continue;
    }

    const char *VarJackfruit = cs_subset_path(sub, "Jackfruit");
    if (!TEST_CHECK_STR_EQ(VarJackfruit, valid[i]))
    {
      TEST_MSG("Value of %s wasn't changed", name);
      return false;
    }
    TEST_MSG("%s = '%s', set by '%s'", name, NONULL(VarJackfruit), NONULL(valid[i]));
    short_line();
  }

  name = "Lemon";
  buf_reset(err);
  rc = cs_str_native_set(cs, name, (intptr_t) "", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }

  name = "Kumquat";
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    short_line();
    buf_reset(err);
    rc = cs_str_native_set(cs, name, (intptr_t) valid[i], err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("%s", buf_string(err));
      return false;
    }

    if (rc & CSR_SUC_NO_CHANGE)
    {
      TEST_MSG("Value of %s wasn't changed", name);
      continue;
    }

    const char *VarKumquat = cs_subset_path(sub, "Kumquat");
    if (!TEST_CHECK_STR_EQ(VarKumquat, valid[i]))
    {
      TEST_MSG("Value of %s wasn't changed", name);
      return false;
    }
    TEST_MSG("%s = '%s', set by '%s'", name, NONULL(VarKumquat), NONULL(valid[i]));
  }

  name = "Tangerine";
  rc = cs_str_native_set(cs, name, (intptr_t) "tangerine", err);
  TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS);

  rc = cs_str_native_set(cs, name, (intptr_t) "apple", err);
  TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS);

  log_line(__func__);
  return true;
}

static bool test_native_get(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;
  const char *name = "Mango";

  int rc = cs_str_string_set(cs, name, "mango", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;

  const char *VarMango = cs_subset_path(sub, "Mango");
  buf_reset(err);
  intptr_t value = cs_str_native_get(cs, name, err);
  if (!TEST_CHECK_STR_EQ(VarMango, (char *) value))
  {
    TEST_MSG("Get failed: %s", buf_string(err));
    return false;
  }
  TEST_MSG("%s = '%s', '%s'", name, VarMango, (char *) value);

  log_line(__func__);
  return true;
}

static bool test_reset(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *name = "Nectarine";
  buf_reset(err);

  const char *VarNectarine = cs_subset_path(sub, "Nectarine");
  TEST_MSG("Initial: %s = '%s'", name, VarNectarine);
  int rc = cs_str_string_set(cs, name, "hello", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  VarNectarine = cs_subset_path(sub, "Nectarine");
  TEST_MSG("Set: %s = '%s'", name, VarNectarine);

  rc = cs_str_reset(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }

  VarNectarine = cs_subset_path(sub, "Nectarine");
  if (!TEST_CHECK_STR_EQ(VarNectarine, "nectarine"))
  {
    TEST_MSG("Value of %s wasn't changed", name);
    return false;
  }

  TEST_MSG("Reset: %s = '%s'", name, VarNectarine);

  rc = cs_str_reset(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }

  name = "Olive";
  buf_reset(err);

  const char *VarOlive = cs_subset_path(sub, "Olive");
  TEST_MSG("Initial: %s = '%s'", name, VarOlive);
  dont_fail = true;
  rc = cs_str_string_set(cs, name, "hello", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  VarOlive = cs_subset_path(sub, "Olive");
  TEST_MSG("Set: %s = '%s'", name, VarOlive);
  dont_fail = false;

  rc = cs_str_reset(cs, name, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }

  VarOlive = cs_subset_path(sub, "Olive");
  if (!TEST_CHECK_STR_EQ(VarOlive, "hello"))
  {
    TEST_MSG("Value of %s changed", name);
    return false;
  }

  TEST_MSG("Reset: %s = '%s'", name, VarOlive);

  name = "Tangerine";
  rc = cs_str_reset(cs, name, err);
  TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS);

  StartupComplete = false;
  rc = cs_str_native_set(cs, name, (intptr_t) "apple", err);
  TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS);
  StartupComplete = true;

  rc = cs_str_reset(cs, name, err);
  TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS);

  log_line(__func__);
  return true;
}

static bool test_validator(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *name = "Papaya";
  buf_reset(err);
  int rc = cs_str_string_set(cs, name, "hello", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }
  const char *VarPapaya = cs_subset_path(sub, "Papaya");
  TEST_MSG("Path: %s = %s", name, VarPapaya);

  buf_reset(err);
  rc = cs_str_native_set(cs, name, IP "world", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }
  VarPapaya = cs_subset_path(sub, "Papaya");
  TEST_MSG("Native: %s = %s", name, VarPapaya);

  name = "Quince";
  buf_reset(err);
  rc = cs_str_string_set(cs, name, "hello", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }
  const char *VarQuince = cs_subset_path(sub, "Quince");
  TEST_MSG("Path: %s = %s", name, VarQuince);

  buf_reset(err);
  rc = cs_str_native_set(cs, name, IP "world", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }
  VarQuince = cs_subset_path(sub, "Quince");
  TEST_MSG("Native: %s = %s", name, VarQuince);

  name = "Raspberry";
  buf_reset(err);
  rc = cs_str_string_set(cs, name, "hello", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }
  const char *VarRaspberry = cs_subset_path(sub, "Raspberry");
  TEST_MSG("Path: %s = %s", name, VarRaspberry);

  buf_reset(err);
  rc = cs_str_native_set(cs, name, IP "world", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }
  VarRaspberry = cs_subset_path(sub, "Raspberry");
  TEST_MSG("Native: %s = %s", name, VarRaspberry);

  log_line(__func__);
  return true;
}

static void dump_native(struct ConfigSet *cs, const char *parent, const char *child)
{
  intptr_t pval = cs_str_native_get(cs, parent, NULL);
  intptr_t cval = cs_str_native_get(cs, child, NULL);

  TEST_MSG("%15s = %s", parent, (char *) pval);
  TEST_MSG("%15s = %s", child, (char *) cval);
}

static bool test_inherit(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  bool result = false;

  const char *account = "fruit";
  const char *parent = "Strawberry";
  char child[128];
  snprintf(child, sizeof(child), "%s:%s", account, parent);

  struct ConfigSubset *sub = cs_subset_new(NULL, NULL, NeoMutt->notify);
  sub->cs = cs;
  struct Account *a = account_new(account, sub);

  struct HashElem *he = cs_subset_create_inheritance(a->sub, parent);
  if (!he)
  {
    TEST_MSG("Error: %s", buf_string(err));
    goto ti_out;
  }

  // set parent
  buf_reset(err);
  int rc = cs_str_string_set(cs, parent, "hello", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Error: %s", buf_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // set child
  buf_reset(err);
  rc = cs_str_string_set(cs, child, "world", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Error: %s", buf_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // reset child
  buf_reset(err);
  rc = cs_str_reset(cs, child, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Error: %s", buf_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // reset parent
  buf_reset(err);
  rc = cs_str_reset(cs, parent, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Error: %s", buf_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);

  log_line(__func__);
  result = true;
ti_out:
  account_free(&a);
  cs_subset_free(&sub);
  return result;
}

void test_config_path(void)
{
  struct ConfigSubset *sub = NeoMutt->sub;
  struct ConfigSet *cs = sub->cs;

  StartupComplete = false;
  dont_fail = true;
  if (!TEST_CHECK(cs_register_variables(cs, Vars)))
    return;
  dont_fail = false;
  StartupComplete = true;

  notify_observer_add(NeoMutt->notify, NT_CONFIG, log_observer, 0);

  set_list(cs);

  struct Buffer *err = buf_pool_get();
  TEST_CHECK(test_initial_values(sub, err));
  TEST_CHECK(test_string_set(sub, err));
  TEST_CHECK(test_string_get(sub, err));
  TEST_CHECK(test_native_set(sub, err));
  TEST_CHECK(test_native_get(sub, err));
  TEST_CHECK(test_reset(sub, err));
  TEST_CHECK(test_validator(sub, err));
  TEST_CHECK(test_inherit(cs, err));
  buf_pool_release(&err);
}
