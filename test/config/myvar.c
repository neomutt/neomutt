/**
 * @file
 * Test code for the MyVar object
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
#include <assert.h>
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
  { "Apple",      DT_MYVAR, IP "apple",      0, NULL, }, /* test_initial_values */
  { "Banana",     DT_MYVAR, IP "banana",     0, NULL, },
  { "Cherry",     DT_MYVAR, IP "cherry",     0, NULL, },
  { "Damson",     DT_MYVAR, 0,               0, NULL, }, /* test_string_set */
  { "Elderberry", DT_MYVAR, IP "elderberry", 0, NULL, },
  { "Fig",        DT_MYVAR, IP "fig",        0, NULL, },
  { "Guava",      DT_MYVAR, 0,               0, NULL, }, /* test_string_get */
  { "Hawthorn",   DT_MYVAR, IP "hawthorn",   0, NULL, },
  { "Ilama",      DT_MYVAR, 0,               0, NULL, },
  { "Jackfruit",  DT_MYVAR, 0,               0, NULL, }, /* test_native_set */
  { "Kumquat",    DT_MYVAR, IP "kumquat",    0, NULL, },
  { "Lemon",      DT_MYVAR, IP "lemon",      0, NULL, },
  { "Mango",      DT_MYVAR, 0,               0, NULL, }, /* test_native_get */
  { "Nectarine",  DT_MYVAR, IP "nectarine",  0, NULL, }, /* test_reset */
  { "Olive",      DT_MYVAR, 0,               0, NULL, },
  { "Strawberry", DT_MYVAR, 0,               0, NULL, }, /* test_inherit */
  { "Tangerine",  DT_MYVAR, 0,               0, NULL, }, /* test_plus_equals */
  { NULL },
};
// clang-format on

/**
 * cs_subset_myvar - Get a MyVar config item by name
 * @param sub   Config Subset
 * @param name  Name of config item
 * @retval ptr  String
 * @retval NULL Empty string
 */
const char *cs_subset_myvar(const struct ConfigSubset *sub, const char *name)
{
  assert(sub && name);

  struct HashElem *he = cs_subset_lookup(sub, name);
  assert(he);
  assert(DTYPE(he->type) == DT_MYVAR);

  intptr_t value = cs_subset_he_native_get(sub, he, NULL);
  assert(value != INT_MIN);

  return (const char *) value;
}

static bool test_initial_values(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *VarApple = cs_subset_myvar(sub, "Apple");
  const char *VarBanana = cs_subset_myvar(sub, "Banana");

  if (!TEST_CHECK(mutt_str_equal(VarApple, "apple")))
  {
    TEST_MSG("Error: initial values were wrong: Apple = %s\n", VarApple);
    return false;
  }

  if (!TEST_CHECK(mutt_str_equal(VarBanana, "banana")))
  {
    TEST_MSG("Error: initial values were wrong: Banana = %s\n", VarBanana);
    return false;
  }

  cs_str_string_set(cs, "Apple", "car", err);
  cs_str_string_set(cs, "Banana", NULL, err);

  VarApple = cs_subset_myvar(sub, "Apple");
  VarBanana = cs_subset_myvar(sub, "Banana");

  struct Buffer *value = buf_pool_get();

  int rc;

  buf_reset(value);
  rc = cs_str_initial_get(cs, "Apple", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", buf_string(value));
    return false;
  }

  VarApple = cs_subset_myvar(sub, "Apple");
  if (!TEST_CHECK(mutt_str_equal(buf_string(value), "apple")))
  {
    TEST_MSG("Apple's initial value is wrong: '%s'\n", buf_string(value));
    return false;
  }
  TEST_MSG("Apple = '%s'\n", VarApple);
  TEST_MSG("Apple's initial value is '%s'\n", buf_string(value));

  buf_reset(value);
  rc = cs_str_initial_get(cs, "Banana", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", buf_string(value));
    return false;
  }

  VarBanana = cs_subset_myvar(sub, "Banana");
  if (!TEST_CHECK(mutt_str_equal(buf_string(value), "banana")))
  {
    TEST_MSG("Banana's initial value is wrong: '%s'\n", buf_string(value));
    return false;
  }
  TEST_MSG("Banana = '%s'\n", VarBanana);
  TEST_MSG("Banana's initial value is '%s'\n", NONULL(buf_string(value)));

  buf_reset(value);
  rc = cs_str_initial_set(cs, "Cherry", "train", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", buf_string(value));
    return false;
  }

  buf_reset(value);
  rc = cs_str_initial_set(cs, "Cherry", "plane", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", buf_string(value));
    return false;
  }

  buf_reset(value);
  rc = cs_str_initial_get(cs, "Cherry", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", buf_string(value));
    return false;
  }

  const char *VarCherry = cs_subset_myvar(sub, "Cherry");
  TEST_MSG("Cherry = '%s'\n", VarCherry);
  TEST_MSG("Cherry's initial value is '%s'\n", NONULL(buf_string(value)));

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
      TEST_MSG("%s\n", buf_string(err));
      return false;
    }

    if (rc & CSR_SUC_NO_CHANGE)
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      continue;
    }

    const char *VarDamson = cs_subset_myvar(sub, "Damson");
    if (!TEST_CHECK(mutt_str_equal(VarDamson, valid[i])))
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      return false;
    }
    TEST_MSG("%s = '%s', set by '%s'\n", name, NONULL(VarDamson), NONULL(valid[i]));
    short_line();
  }

  name = "Fig";
  buf_reset(err);
  rc = cs_str_string_set(cs, name, "", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", buf_string(err));
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
      TEST_MSG("%s\n", buf_string(err));
      return false;
    }

    if (rc & CSR_SUC_NO_CHANGE)
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      continue;
    }

    const char *VarElderberry = cs_subset_myvar(sub, "Elderberry");
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

  buf_reset(err);
  int rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", buf_string(err));
    return false;
  }
  const char *VarGuava = cs_subset_myvar(sub, "Guava");
  TEST_MSG("%s = '%s', '%s'\n", name, NONULL(VarGuava), buf_string(err));

  name = "Hawthorn";
  buf_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", buf_string(err));
    return false;
  }
  const char *VarHawthorn = cs_subset_myvar(sub, "Hawthorn");
  TEST_MSG("%s = '%s', '%s'\n", name, NONULL(VarHawthorn), buf_string(err));

  name = "Ilama";
  rc = cs_str_string_set(cs, name, "ilama", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;

  buf_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", buf_string(err));
    return false;
  }
  const char *VarIlama = cs_subset_myvar(sub, "Ilama");
  TEST_MSG("%s = '%s', '%s'\n", name, NONULL(VarIlama), buf_string(err));

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
    buf_reset(err);
    rc = cs_str_native_set(cs, name, (intptr_t) valid[i], err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("%s\n", buf_string(err));
      return false;
    }

    if (rc & CSR_SUC_NO_CHANGE)
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      continue;
    }

    const char *VarJackfruit = cs_subset_myvar(sub, "Jackfruit");
    if (!TEST_CHECK(mutt_str_equal(VarJackfruit, valid[i])))
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      return false;
    }
    TEST_MSG("%s = '%s', set by '%s'\n", name, NONULL(VarJackfruit), NONULL(valid[i]));
    short_line();
  }

  name = "Lemon";
  buf_reset(err);
  rc = cs_str_native_set(cs, name, (intptr_t) "", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", buf_string(err));
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
      TEST_MSG("%s\n", buf_string(err));
      return false;
    }

    if (rc & CSR_SUC_NO_CHANGE)
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      continue;
    }

    const char *VarKumquat = cs_subset_myvar(sub, "Kumquat");
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

  const char *VarMango = cs_subset_myvar(sub, "Mango");
  buf_reset(err);
  intptr_t value = cs_str_native_get(cs, name, err);
  if (!TEST_CHECK(mutt_str_equal(VarMango, (char *) value)))
  {
    TEST_MSG("Get failed: %s\n", buf_string(err));
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
    buf_reset(err);
    rc = cs_str_string_set(cs, name, PlusTests[i][0], err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("Set failed: %s\n", buf_string(err));
      return false;
    }

    rc = cs_str_string_plus_equals(cs, name, PlusTests[i][1], err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("PlusEquals failed: %s\n", buf_string(err));
      return false;
    }

    rc = cs_str_string_get(cs, name, err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("Get failed: %s\n", buf_string(err));
      return false;
    }

    if (!TEST_CHECK(mutt_str_equal(PlusTests[i][2], buf_string(err))))
    {
      TEST_MSG("Expected: %s\n", PlusTests[i][2]);
      TEST_MSG("Actual  : %s\n", buf_string(err));
      return false;
    }
  }

  log_line(__func__);
  return true;
}

static bool test_non_existing(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  int rc;
  const char *name = "Tangerine";

  buf_reset(err);
  rc = cs_str_string_get(cs, "does_not_exist", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_ERR_UNKNOWN))
  {
    TEST_MSG("Get succeeded but should have failed: %s\n", buf_string(err));
    return false;
  }

  // Distinguish an empty string from an unset one
  buf_reset(err);
  rc = cs_str_string_set(cs, name, "", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Set failed: %s\n", buf_string(err));
    return false;
  }
  rc = cs_str_string_get(cs, name, err);
  buf_reset(err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", buf_string(err));
    return false;
  }
  if (!TEST_CHECK(mutt_str_equal("", buf_string(err))))
  {
    TEST_MSG("Expected: '%s'\n", "");
    TEST_MSG("Actual  : '%s'\n", buf_string(err));
    return false;
  }

  // Delete should remove the variable
  buf_reset(err);
  rc = cs_str_delete(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Delete failed: %s\n", buf_string(err));
    return false;
  }
  buf_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_ERR_UNKNOWN))
  {
    TEST_MSG("Get succeeded but should have failed: %s\n", buf_string(err));
    return false;
  }

  log_line(__func__);
  return true;
}

static bool test_reset(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *name = "Nectarine";
  buf_reset(err);

  const char *VarNectarine = cs_subset_myvar(sub, "Nectarine");
  TEST_MSG("Initial: %s = '%s'\n", name, VarNectarine);
  int rc = cs_str_string_set(cs, name, "hello", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  VarNectarine = cs_subset_myvar(sub, "Nectarine");
  TEST_MSG("Set: %s = '%s'\n", name, VarNectarine);

  rc = cs_str_reset(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", buf_string(err));
    return false;
  }

  buf_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", buf_string(err));
    return false;
  }
  if (!TEST_CHECK(mutt_str_equal(buf_string(err), "nectarine")))
  {
    TEST_MSG("Reset failed: expected = %s, got = %s\n", "nectarine", buf_string(err));
    return false;
  }

  name = "Olive";
  buf_reset(err);

  const char *VarOlive = cs_subset_myvar(sub, "Olive");
  TEST_MSG("Initial: %s = '%s'\n", name, VarOlive);
  rc = cs_str_string_set(cs, name, "hello", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  VarOlive = cs_subset_myvar(sub, "Olive");
  TEST_MSG("Set: %s = '%s'\n", name, VarOlive);

  rc = cs_str_reset(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", buf_string(err));
    return false;
  }

  log_line(__func__);
  return true;
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

  buf_reset(err);
  struct HashElem *he = cs_subset_create_inheritance(a->sub, parent);
  if (he)
  {
    TEST_MSG("Error: Inheritance succeeded but should have failed.\n", buf_string(err));
    goto ti_out;
  }

  log_line(__func__);
  result = true;
ti_out:
  account_free(&a);
  cs_subset_free(&sub);
  return result;
}

void test_config_myvar(void)
{
  test_neomutt_create();
  struct ConfigSubset *sub = NeoMutt->sub;
  struct ConfigSet *cs = sub->cs;

  dont_fail = true;
  if (!TEST_CHECK(cs_register_variables(cs, Vars, DT_NO_FLAGS)))
    return;
  dont_fail = false;

  notify_observer_add(NeoMutt->notify, NT_CONFIG, log_observer, 0);

  set_list(cs);

  struct Buffer *err = buf_pool_get();
  TEST_CHECK(test_initial_values(sub, err));
  TEST_CHECK(test_string_set(sub, err));
  TEST_CHECK(test_string_get(sub, err));
  TEST_CHECK(test_native_set(sub, err));
  TEST_CHECK(test_native_get(sub, err));
  TEST_CHECK(test_string_plus_equals(sub, err));
  TEST_CHECK(test_non_existing(sub, err));
  TEST_CHECK(test_reset(sub, err));
  TEST_CHECK(test_inherit(cs, err));
  buf_pool_release(&err);

  test_neomutt_destroy();
}
