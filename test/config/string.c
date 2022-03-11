/**
 * @file
 * Test code for the String object
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
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/common.h"
#include "config/lib.h"
#include "core/lib.h"
#include "test_common.h"

// clang-format off
static struct ConfigDef Vars[] = {
  { "Apple",      DT_STRING,              IP "apple",      0, NULL,              }, /* test_initial_values */
  { "Banana",     DT_STRING,              IP "banana",     0, NULL,              },
  { "Cherry",     DT_STRING,              IP "cherry",     0, NULL,              },
  { "Damson",     DT_STRING,              0,               0, NULL,              }, /* test_string_set */
  { "Elderberry", DT_STRING,              IP "elderberry", 0, NULL,              },
  { "Fig",        DT_STRING|DT_NOT_EMPTY, IP "fig",        0, NULL,              },
  { "Guava",      DT_STRING,              0,               0, NULL,              }, /* test_string_get */
  { "Hawthorn",   DT_STRING,              IP "hawthorn",   0, NULL,              },
  { "Ilama",      DT_STRING,              0,               0, NULL,              },
  { "Jackfruit",  DT_STRING,              0,               0, NULL,              }, /* test_native_set */
  { "Kumquat",    DT_STRING,              IP "kumquat",    0, NULL,              },
  { "Lemon",      DT_STRING|DT_NOT_EMPTY, IP "lemon",      0, NULL,              },
  { "Mango",      DT_STRING,              0,               0, NULL,              }, /* test_native_get */
  { "Nectarine",  DT_STRING,              IP "nectarine",  0, NULL,              }, /* test_reset */
  { "Olive",      DT_STRING,              IP "olive",      0, validator_fail,    },
  { "Papaya",     DT_STRING,              IP "papaya",     0, validator_succeed, }, /* test_validator */
  { "Quince",     DT_STRING,              IP "quince",     0, validator_warn,    },
  { "Raspberry",  DT_STRING,              IP "raspberry",  0, validator_fail,    },
  { "Strawberry", DT_STRING,              0,               0, NULL,              }, /* test_inherit */
  { "Tangerine",  DT_STRING,              0,               0, NULL,              }, /* test_plus_equals */
  { "Ugli",       DT_STRING|DT_CHARSET_SINGLE, 0,          0, charset_validator, },
  { "Vanilla",    DT_STRING|DT_CHARSET_STRICT, 0,          0, charset_validator, },
  { NULL },
};
// clang-format on

static bool test_initial_values(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *VarApple = cs_subset_string(sub, "Apple");
  const char *VarBanana = cs_subset_string(sub, "Banana");

  TEST_MSG("Apple = %s\n", VarApple);
  TEST_MSG("Banana = %s\n", VarBanana);

  if (!TEST_CHECK(mutt_str_equal(VarApple, "apple")))
  {
    TEST_MSG("Error: initial values were wrong\n");
    return false;
  }

  if (!TEST_CHECK(mutt_str_equal(VarBanana, "banana")))
  {
    TEST_MSG("Error: initial values were wrong\n");
    return false;
  }

  cs_str_string_set(cs, "Apple", "car", err);
  cs_str_string_set(cs, "Banana", NULL, err);

  VarApple = cs_subset_string(sub, "Apple");
  VarBanana = cs_subset_string(sub, "Banana");

  struct Buffer *value = mutt_buffer_pool_get();

  int rc;

  mutt_buffer_reset(value);
  rc = cs_str_initial_get(cs, "Apple", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(value));
    return false;
  }

  VarApple = cs_subset_string(sub, "Apple");
  if (!TEST_CHECK(mutt_str_equal(mutt_buffer_string(value), "apple")))
  {
    TEST_MSG("Apple's initial value is wrong: '%s'\n", mutt_buffer_string(value));
    return false;
  }
  TEST_MSG("Apple = '%s'\n", VarApple);
  TEST_MSG("Apple's initial value is '%s'\n", mutt_buffer_string(value));

  mutt_buffer_reset(value);
  rc = cs_str_initial_get(cs, "Banana", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(value));
    return false;
  }

  VarBanana = cs_subset_string(sub, "Banana");
  if (!TEST_CHECK(mutt_str_equal(mutt_buffer_string(value), "banana")))
  {
    TEST_MSG("Banana's initial value is wrong: '%s'\n", mutt_buffer_string(value));
    return false;
  }
  TEST_MSG("Banana = '%s'\n", VarBanana);
  TEST_MSG("Banana's initial value is '%s'\n", NONULL(mutt_buffer_string(value)));

  mutt_buffer_reset(value);
  rc = cs_str_initial_set(cs, "Cherry", "train", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(value));
    return false;
  }

  mutt_buffer_reset(value);
  rc = cs_str_initial_set(cs, "Cherry", "plane", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(value));
    return false;
  }

  mutt_buffer_reset(value);
  rc = cs_str_initial_get(cs, "Cherry", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(value));
    return false;
  }

  const char *VarCherry = cs_subset_string(sub, "Cherry");
  TEST_MSG("Cherry = '%s'\n", VarCherry);
  TEST_MSG("Cherry's initial value is '%s'\n", NONULL(mutt_buffer_string(value)));

  mutt_buffer_pool_release(&value);
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
    mutt_buffer_reset(err);
    rc = cs_str_string_set(cs, name, valid[i], err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("%s\n", mutt_buffer_string(err));
      return false;
    }

    if (rc & CSR_SUC_NO_CHANGE)
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      continue;
    }

    const char *VarDamson = cs_subset_string(sub, "Damson");
    if (!TEST_CHECK(mutt_str_equal(VarDamson, valid[i])))
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      return false;
    }
    TEST_MSG("%s = '%s', set by '%s'\n", name, NONULL(VarDamson), NONULL(valid[i]));
    short_line();
  }

  name = "Fig";
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  name = "Elderberry";
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    short_line();
    mutt_buffer_reset(err);
    rc = cs_str_string_set(cs, name, valid[i], err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("%s\n", mutt_buffer_string(err));
      return false;
    }

    if (rc & CSR_SUC_NO_CHANGE)
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      continue;
    }

    const char *VarElderberry = cs_subset_string(sub, "Elderberry");
    if (!TEST_CHECK(mutt_str_equal(VarElderberry, valid[i])))
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      return false;
    }
    TEST_MSG("%s = '%s', set by '%s'\n", name, NONULL(VarElderberry), NONULL(valid[i]));
  }

  log_line(__func__);
  return true;
}

static bool test_string_get(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;
  const char *name = "Guava";

  mutt_buffer_reset(err);
  int rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", mutt_buffer_string(err));
    return false;
  }
  const char *VarGuava = cs_subset_string(sub, "Guava");
  TEST_MSG("%s = '%s', '%s'\n", name, NONULL(VarGuava), mutt_buffer_string(err));

  name = "Hawthorn";
  mutt_buffer_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", mutt_buffer_string(err));
    return false;
  }
  const char *VarHawthorn = cs_subset_string(sub, "Hawthorn");
  TEST_MSG("%s = '%s', '%s'\n", name, NONULL(VarHawthorn), mutt_buffer_string(err));

  name = "Ilama";
  rc = cs_str_string_set(cs, name, "ilama", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;

  mutt_buffer_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", mutt_buffer_string(err));
    return false;
  }
  const char *VarIlama = cs_subset_string(sub, "Ilama");
  TEST_MSG("%s = '%s', '%s'\n", name, NONULL(VarIlama), mutt_buffer_string(err));

  log_line(__func__);
  return true;
}

static bool test_native_set(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);

  const char *valid[] = { "hello", "world", "world", "", NULL };
  const char *name = "Jackfruit";
  struct ConfigSet *cs = sub->cs;

  int rc;
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    mutt_buffer_reset(err);
    rc = cs_str_native_set(cs, name, (intptr_t) valid[i], err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("%s\n", mutt_buffer_string(err));
      return false;
    }

    if (rc & CSR_SUC_NO_CHANGE)
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      continue;
    }

    const char *VarJackfruit = cs_subset_string(sub, "Jackfruit");
    if (!TEST_CHECK(mutt_str_equal(VarJackfruit, valid[i])))
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      return false;
    }
    TEST_MSG("%s = '%s', set by '%s'\n", name, NONULL(VarJackfruit), NONULL(valid[i]));
    short_line();
  }

  name = "Lemon";
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, (intptr_t) "", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  name = "Kumquat";
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    short_line();
    mutt_buffer_reset(err);
    rc = cs_str_native_set(cs, name, (intptr_t) valid[i], err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("%s\n", mutt_buffer_string(err));
      return false;
    }

    if (rc & CSR_SUC_NO_CHANGE)
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      continue;
    }

    const char *VarKumquat = cs_subset_string(sub, "Kumquat");
    if (!TEST_CHECK(mutt_str_equal(VarKumquat, valid[i])))
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      return false;
    }
    TEST_MSG("%s = '%s', set by '%s'\n", name, NONULL(VarKumquat), NONULL(valid[i]));
  }

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

  const char *VarMango = cs_subset_string(sub, "Mango");
  mutt_buffer_reset(err);
  intptr_t value = cs_str_native_get(cs, name, err);
  if (!TEST_CHECK(mutt_str_equal(VarMango, (char *) value)))
  {
    TEST_MSG("Get failed: %s\n", mutt_buffer_string(err));
    return false;
  }
  TEST_MSG("%s = '%s', '%s'\n", name, VarMango, (char *) value);

  log_line(__func__);
  return true;
}

static bool test_string_plus_equals(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *name = "Tangerine";
  static char *PlusTests[][3] = {
    // clang-format off
    // Initial,        Plus,     Result
    { "",              "",       ""                   }, // Add nothing to various lists
    { "one",           "",       "one"                },
    { "one two",       "",       "one two"            },
    { "one two three", "",       "one two three"      },

    { "",              "nine",   "nine"               }, // Add an item to various lists
    { "one",           " nine",   "one nine"           },
    { "one two",       " nine",   "one two nine"       },
    { "one two three", " nine",   "one two three nine" },
    // clang-format on
  };

  int rc;
  for (unsigned int i = 0; i < mutt_array_size(PlusTests); i++)
  {
    mutt_buffer_reset(err);
    rc = cs_str_string_set(cs, name, PlusTests[i][0], err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("Set failed: %s\n", mutt_buffer_string(err));
      return false;
    }

    rc = cs_str_string_plus_equals(cs, name, PlusTests[i][1], err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("PlusEquals failed: %s\n", mutt_buffer_string(err));
      return false;
    }

    rc = cs_str_string_get(cs, name, err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("Get failed: %s\n", mutt_buffer_string(err));
      return false;
    }

    if (!TEST_CHECK(mutt_str_equal(PlusTests[i][2], mutt_buffer_string(err))))
    {
      TEST_MSG("Expected: %s\n", PlusTests[i][2]);
      TEST_MSG("Actual  : %s\n", mutt_buffer_string(err));
      return false;
    }
  }

  log_line(__func__);
  return true;
}

static bool test_reset(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *name = "Nectarine";
  mutt_buffer_reset(err);

  const char *VarNectarine = cs_subset_string(sub, "Nectarine");
  TEST_MSG("Initial: %s = '%s'\n", name, VarNectarine);
  int rc = cs_str_string_set(cs, name, "hello", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  VarNectarine = cs_subset_string(sub, "Nectarine");
  TEST_MSG("Set: %s = '%s'\n", name, VarNectarine);

  rc = cs_str_reset(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  VarNectarine = cs_subset_string(sub, "Nectarine");
  if (!TEST_CHECK(mutt_str_equal(VarNectarine, "nectarine")))
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = '%s'\n", name, VarNectarine);

  rc = cs_str_reset(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  name = "Olive";
  mutt_buffer_reset(err);

  const char *VarOlive = cs_subset_string(sub, "Olive");
  TEST_MSG("Initial: %s = '%s'\n", name, VarOlive);
  dont_fail = true;
  rc = cs_str_string_set(cs, name, "hello", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  VarOlive = cs_subset_string(sub, "Olive");
  TEST_MSG("Set: %s = '%s'\n", name, VarOlive);
  dont_fail = false;

  rc = cs_str_reset(cs, name, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  VarOlive = cs_subset_string(sub, "Olive");
  if (!TEST_CHECK(mutt_str_equal(VarOlive, "hello")))
  {
    TEST_MSG("Value of %s changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = '%s'\n", name, VarOlive);

  log_line(__func__);
  return true;
}

static bool test_validator(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *name = "Papaya";
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, name, "hello", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }
  const char *VarPapaya = cs_subset_string(sub, "Papaya");
  TEST_MSG("String: %s = %s\n", name, VarPapaya);

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP "world", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }
  VarPapaya = cs_subset_string(sub, "Papaya");
  TEST_MSG("Native: %s = %s\n", name, VarPapaya);

  name = "Quince";
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "hello", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }
  const char *VarQuince = cs_subset_string(sub, "Quince");
  TEST_MSG("String: %s = %s\n", name, VarQuince);

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP "world", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }
  VarQuince = cs_subset_string(sub, "Quince");
  TEST_MSG("Native: %s = %s\n", name, VarQuince);

  name = "Raspberry";
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "hello", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }
  const char *VarRaspberry = cs_subset_string(sub, "Raspberry");
  TEST_MSG("String: %s = %s\n", name, VarRaspberry);

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP "world", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }
  VarRaspberry = cs_subset_string(sub, "Raspberry");
  TEST_MSG("Native: %s = %s\n", name, VarRaspberry);

  name = "Olive";
  mutt_buffer_reset(err);
  rc = cs_str_string_plus_equals(cs, name, "hello", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }
  const char *VarOlive = cs_subset_string(sub, "Olive");
  TEST_MSG("String: %s = %s\n", name, VarOlive);

  name = "Ugli";
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "utf-8", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, NULL, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "utf-8:us-ascii", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  name = "Vanilla";
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "apple", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  log_line(__func__);
  return true;
}

static void dump_native(struct ConfigSet *cs, const char *parent, const char *child)
{
  intptr_t pval = cs_str_native_get(cs, parent, NULL);
  intptr_t cval = cs_str_native_get(cs, child, NULL);

  TEST_MSG("%15s = %s\n", parent, (char *) pval);
  TEST_MSG("%15s = %s\n", child, (char *) cval);
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
    TEST_MSG("Error: %s\n", mutt_buffer_string(err));
    goto ti_out;
  }

  // set parent
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, parent, "hello", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Error: %s\n", mutt_buffer_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // set child
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, child, "world", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Error: %s\n", mutt_buffer_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // reset child
  mutt_buffer_reset(err);
  rc = cs_str_reset(cs, child, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Error: %s\n", mutt_buffer_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // reset parent
  mutt_buffer_reset(err);
  rc = cs_str_reset(cs, parent, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Error: %s\n", mutt_buffer_string(err));
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

void test_config_string(void)
{
  NeoMutt = test_neomutt_create();
  struct ConfigSubset *sub = NeoMutt->sub;
  struct ConfigSet *cs = sub->cs;

  dont_fail = true;
  if (!TEST_CHECK(cs_register_variables(cs, Vars, 0)))
    return;
  dont_fail = false;

  notify_observer_add(NeoMutt->notify, NT_CONFIG, log_observer, 0);

  set_list(cs);

  struct Buffer *err = mutt_buffer_pool_get();
  TEST_CHECK(test_initial_values(sub, err));
  TEST_CHECK(test_string_set(sub, err));
  TEST_CHECK(test_string_get(sub, err));
  TEST_CHECK(test_native_set(sub, err));
  TEST_CHECK(test_native_get(sub, err));
  TEST_CHECK(test_string_plus_equals(sub, err));
  TEST_CHECK(test_reset(sub, err));
  TEST_CHECK(test_validator(sub, err));
  TEST_CHECK(test_inherit(cs, err));
  mutt_buffer_pool_release(&err);

  test_neomutt_destroy(&NeoMutt);
}
