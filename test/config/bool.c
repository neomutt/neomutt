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
#include "config.h"
#include "acutest.h"
#include <limits.h>
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
  { "Apple",      DT_BOOL, 0, 0, NULL,              }, /* test_initial_values */
  { "Banana",     DT_BOOL, 1, 0, NULL,              },
  { "Cherry",     DT_BOOL, 0, 0, NULL,              },
  { "Damson",     DT_BOOL, 0, 0, NULL,              }, /* test_string_set */
  { "Elderberry", DT_BOOL, 0, 0, NULL,              }, /* test_string_get */
  { "Fig",        DT_BOOL, 0, 0, NULL,              }, /* test_native_set */
  { "Guava",      DT_BOOL, 0, 0, NULL,              }, /* test_native_get */
  { "Hawthorn",   DT_BOOL, 0, 0, NULL,              }, /* test_reset */
  { "Ilama",      DT_BOOL, 0, 0, validator_fail,    },
  { "Jackfruit",  DT_BOOL, 0, 0, validator_succeed, }, /* test_validator */
  { "Kumquat",    DT_BOOL, 0, 0, validator_warn,    },
  { "Lemon",      DT_BOOL, 0, 0, validator_fail,    },
  { "Mango",      DT_BOOL, 0, 0, NULL,              }, /* test_inherit */
  { "Nectarine",  DT_BOOL, 0, 0, NULL,              }, /* test_toggle */
  { "Olive",      DT_QUAD, 0, 0, NULL,              },
  { NULL },
};
// clang-format on

static bool test_initial_values(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);

  struct ConfigSet *cs = sub->cs;

  bool VarApple = cs_subset_bool(sub, "Apple");
  bool VarBanana = cs_subset_bool(sub, "Banana");

  TEST_MSG("Apple = %d\n", VarApple);
  TEST_MSG("Banana = %d\n", VarBanana);

  if (!TEST_CHECK(VarApple == false))
  {
    TEST_MSG("Expected: %d\n", false);
    TEST_MSG("Actual  : %d\n", VarApple);
  }

  if (!TEST_CHECK(VarBanana == true))
  {
    TEST_MSG("Expected: %d\n", true);
    TEST_MSG("Actual  : %d\n", VarBanana);
  }

  cs_str_string_set(cs, "Apple", "true", err);
  cs_str_string_set(cs, "Banana", "false", err);

  struct Buffer *value = mutt_buffer_pool_get();

  int rc;

  mutt_buffer_reset(value);
  rc = cs_str_initial_get(cs, "Apple", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(value));
    return false;
  }

  if (!TEST_CHECK(mutt_str_equal(mutt_buffer_string(value), "no")))
  {
    TEST_MSG("Apple's initial value is wrong: '%s'\n", mutt_buffer_string(value));
    return false;
  }

  VarApple = cs_subset_bool(sub, "Apple");
  TEST_MSG("Apple = '%s'\n", VarApple ? "yes" : "no");
  TEST_MSG("Apple's initial value is '%s'\n", mutt_buffer_string(value));

  mutt_buffer_reset(value);
  rc = cs_str_initial_get(cs, "Banana", value);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", mutt_buffer_string(value));
    return false;
  }

  if (!TEST_CHECK(mutt_str_equal(mutt_buffer_string(value), "yes")))
  {
    TEST_MSG("Banana's initial value is wrong: '%s'\n", mutt_buffer_string(value));
    return false;
  }

  VarBanana = cs_subset_bool(sub, "Banana");
  TEST_MSG("Banana = '%s'\n", VarBanana ? "yes" : "no");
  TEST_MSG("Banana's initial value is '%s'\n", NONULL(mutt_buffer_string(value)));

  mutt_buffer_reset(value);
  rc = cs_str_initial_set(cs, "Cherry", "yes", value);
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

  bool VarCherry = cs_subset_bool(sub, "Cherry");
  TEST_MSG("Cherry = '%s'\n", VarCherry ? "yes" : "no");
  TEST_MSG("Cherry's initial value is '%s'\n", NONULL(mutt_buffer_string(value)));

  mutt_buffer_pool_release(&value);
  log_line(__func__);
  return true;
}

static bool test_string_set(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);

  const char *valid[] = {
    "no", "yes", "n", "y", "false", "true", "0", "1", "off", "on",
  };
  const char *invalid[] = {
    "nope",
    "ye",
    "",
    NULL,
  };

  struct ConfigSet *cs = sub->cs;
  const char *name = "Damson";
  bool VarDamson;

  int rc;
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    cs_str_native_set(cs, name, ((i + 1) % 2), NULL);

    TEST_MSG("Setting %s to %s\n", name, valid[i]);
    mutt_buffer_reset(err);
    rc = cs_str_string_set(cs, name, valid[i], err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("%s\n", mutt_buffer_string(err));
      return false;
    }

    VarDamson = cs_subset_bool(sub, "Damson");
    if (VarDamson != (i % 2))
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      return false;
    }
    TEST_MSG("%s = %d, set by '%s'\n", name, VarDamson, valid[i]);
    short_line();
  }

  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "yes", err);
  if (TEST_CHECK(rc & CSR_SUC_NO_CHANGE))
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return false;
  }

  short_line();
  for (unsigned int i = 0; i < mutt_array_size(invalid); i++)
  {
    mutt_buffer_reset(err);
    rc = cs_str_string_set(cs, name, invalid[i], err);
    if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
    {
      TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
    }
    else
    {
      VarDamson = cs_subset_bool(sub, "Damson");
      TEST_MSG("%s = %d, set by '%s'\n", name, VarDamson, invalid[i]);
      TEST_MSG("This test should have failed\n");
      return false;
    }
    short_line();
  }

  log_line(__func__);
  return true;
}

static bool test_string_get(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);

  struct ConfigSet *cs = sub->cs;
  const char *name = "Elderberry";
  int rc;

  cs_str_native_set(cs, name, false, NULL);
  mutt_buffer_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", mutt_buffer_string(err));
    return false;
  }
  bool VarElderberry = cs_subset_bool(sub, "Elderberry");
  TEST_MSG("%s = %d, %s\n", name, VarElderberry, mutt_buffer_string(err));

  cs_str_native_set(cs, name, true, NULL);
  mutt_buffer_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", mutt_buffer_string(err));
    return false;
  }
  VarElderberry = cs_subset_bool(sub, "Elderberry");
  TEST_MSG("%s = %d, %s\n", name, VarElderberry, mutt_buffer_string(err));

  // cs_str_native_set(cs, name, 3, NULL);
  // mutt_buffer_reset(err);
  // rc = cs_str_string_get(cs, name, err);
  // if (!TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  // {
  //   TEST_MSG("Get succeeded with invalid value: %s\n", mutt_buffer_string(err));
  //   return false;
  // }

  log_line(__func__);
  return true;
}

static bool test_native_set(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);

  struct ConfigSet *cs = sub->cs;
  const char *name = "Fig";
  bool value = true;

  TEST_MSG("Setting %s to %d\n", name, value);
  cs_str_native_set(cs, name, false, NULL);
  mutt_buffer_reset(err);
  int rc = cs_str_native_set(cs, name, value, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  bool VarFig = cs_subset_bool(sub, "Fig");
  if (!TEST_CHECK(VarFig == value))
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    return false;
  }

  TEST_MSG("%s = %d, set to '%d'\n", name, VarFig, value);

  short_line();
  mutt_buffer_reset(err);
  TEST_MSG("Setting %s to %d\n", name, value);
  rc = cs_str_native_set(cs, name, value, err);
  if (!TEST_CHECK((rc & CSR_SUC_NO_CHANGE) != 0))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  if (rc & CSR_SUC_NO_CHANGE)
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
  }

  int invalid[] = { -1, 2 };
  for (unsigned int i = 0; i < mutt_array_size(invalid); i++)
  {
    short_line();
    cs_str_native_set(cs, name, false, NULL);
    TEST_MSG("Setting %s to %d\n", name, invalid[i]);
    mutt_buffer_reset(err);
    rc = cs_str_native_set(cs, name, invalid[i], err);
    if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
    {
      TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
    }
    else
    {
      VarFig = cs_subset_bool(sub, "Fig");
      TEST_MSG("%s = %d, set by '%d'\n", name, VarFig, invalid[i]);
      TEST_MSG("This test should have failed\n");
      return false;
    }
  }

  log_line(__func__);
  return true;
}

static bool test_native_get(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);

  struct ConfigSet *cs = sub->cs;
  const char *name = "Guava";

  cs_str_native_set(cs, name, true, NULL);
  mutt_buffer_reset(err);
  intptr_t value = cs_str_native_get(cs, name, err);
  if (!TEST_CHECK(value != INT_MIN))
  {
    TEST_MSG("Get failed: %s\n", mutt_buffer_string(err));
    return false;
  }
  TEST_MSG("%s = %ld\n", name, value);

  log_line(__func__);
  return true;
}

static bool test_reset(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);

  struct ConfigSet *cs = sub->cs;
  const char *name = "Hawthorn";
  cs_str_native_set(cs, name, true, NULL);
  mutt_buffer_reset(err);

  bool VarHawthorn = cs_subset_bool(sub, "Hawthorn");
  TEST_MSG("%s = %d\n", name, VarHawthorn);
  int rc = cs_str_reset(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  VarHawthorn = cs_subset_bool(sub, "Hawthorn");
  if (!TEST_CHECK(VarHawthorn != true))
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = %d\n", name, VarHawthorn);

  short_line();
  name = "Ilama";
  mutt_buffer_reset(err);

  bool VarIlama = cs_subset_bool(sub, "Ilama");
  TEST_MSG("Initial: %s = %d\n", name, VarIlama);
  dont_fail = true;
  rc = cs_str_string_set(cs, name, "yes", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  VarIlama = cs_subset_bool(sub, "Ilama");
  TEST_MSG("Set: %s = %d\n", name, VarIlama);
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

  rc = cs_str_reset(cs, "unknown", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  VarIlama = cs_subset_bool(sub, "Ilama");
  if (!TEST_CHECK(VarIlama))
  {
    TEST_MSG("Value of %s changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = %d\n", name, VarIlama);

  log_line(__func__);
  return true;
}

static bool test_validator(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);

  struct ConfigSet *cs = sub->cs;
  const char *name = "Jackfruit";
  cs_str_native_set(cs, name, false, NULL);
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, name, "yes", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }
  bool VarJackfruit = cs_subset_bool(sub, "Jackfruit");
  TEST_MSG("String: %s = %d\n", name, VarJackfruit);
  short_line();

  cs_str_native_set(cs, name, false, NULL);
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, 1, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }
  VarJackfruit = cs_subset_bool(sub, "Jackfruit");
  TEST_MSG("Native: %s = %d\n", name, VarJackfruit);
  short_line();

  name = "Kumquat";
  cs_str_native_set(cs, name, false, NULL);
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "yes", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }
  bool VarKumquat = cs_subset_bool(sub, "Kumquat");
  TEST_MSG("String: %s = %d\n", name, VarKumquat);
  short_line();

  cs_str_native_set(cs, name, false, NULL);
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, 1, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }
  VarKumquat = cs_subset_bool(sub, "Kumquat");
  TEST_MSG("Native: %s = %d\n", name, VarKumquat);
  short_line();

  name = "Lemon";
  cs_str_native_set(cs, name, false, NULL);
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "yes", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }
  bool VarLemon = cs_subset_bool(sub, "Lemon");
  TEST_MSG("String: %s = %d\n", name, VarLemon);
  short_line();

  cs_str_native_set(cs, name, false, NULL);
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, 1, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }
  VarLemon = cs_subset_bool(sub, "Lemon");
  TEST_MSG("Native: %s = %d\n", name, VarLemon);

  log_line(__func__);
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
  cs_str_native_set(cs, parent, false, NULL);
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, parent, "1", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", mutt_buffer_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);
  short_line();

  // set child
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, child, "0", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", mutt_buffer_string(err));
    goto ti_out;
  }
  if (rc & CSR_SUC_NO_CHANGE)
  {
    TEST_MSG("Value of %s wasn't changed\n", parent);
  }
  dump_native(cs, parent, child);
  short_line();

  // reset child
  mutt_buffer_reset(err);
  rc = cs_str_reset(cs, child, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", mutt_buffer_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);
  short_line();

  // reset the already-reset child
  rc = cs_str_reset(cs, child, err);

  // reset parent
  mutt_buffer_reset(err);
  rc = cs_str_reset(cs, parent, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
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

static bool test_toggle(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);

  struct ToggleTest
  {
    bool before;
    bool after;
  };

  struct ToggleTest tests[] = { { false, true }, { true, false } };

  struct ConfigSet *cs = sub->cs;
  const char *name = "Nectarine";
  int rc;

  struct HashElem *he = cs_get_elem(cs, name);
  if (!he)
    return false;

  rc = bool_he_toggle(NULL, he, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_ERR_CODE))
  {
    TEST_MSG("Toggle succeeded when is shouldn't have\n");
    return false;
  }

  rc = bool_he_toggle(NeoMutt->sub, NULL, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_ERR_CODE))
  {
    TEST_MSG("Toggle succeeded when is shouldn't have\n");
    return false;
  }

  rc = bool_str_toggle(NULL, "Apple", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_ERR_CODE))
  {
    TEST_MSG("Toggle succeeded when is shouldn't have\n");
    return false;
  }

  rc = bool_str_toggle(NeoMutt->sub, NULL, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_ERR_CODE))
  {
    TEST_MSG("Toggle succeeded when is shouldn't have\n");
    return false;
  }

  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    bool before = tests[i].before;
    bool after = tests[i].after;
    TEST_MSG("test %zu\n", i);

    cs_str_native_set(cs, name, before, NULL);
    mutt_buffer_reset(err);
    intptr_t value = cs_he_native_get(cs, he, err);
    if (!TEST_CHECK(value != INT_MIN))
    {
      TEST_MSG("Get failed: %s\n", mutt_buffer_string(err));
      return false;
    }

    bool copy = value;
    if (!TEST_CHECK(copy == before))
    {
      TEST_MSG("Initial value is wrong: %s\n", mutt_buffer_string(err));
      return false;
    }

    rc = bool_he_toggle(NeoMutt->sub, he, err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("Toggle failed: %s\n", mutt_buffer_string(err));
      return false;
    }

    bool VarNectarine = cs_subset_bool(sub, "Nectarine");
    if (!TEST_CHECK(VarNectarine == after))
    {
      TEST_MSG("Toggle value is wrong: %s\n", mutt_buffer_string(err));
      return false;
    }
    short_line();
  }

  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    bool before = tests[i].before;
    bool after = tests[i].after;
    TEST_MSG("test %zu\n", i);

    cs_str_native_set(cs, name, before, NULL);
    mutt_buffer_reset(err);
    intptr_t value = cs_he_native_get(cs, he, err);
    if (!TEST_CHECK(value != INT_MIN))
    {
      TEST_MSG("Get failed: %s\n", mutt_buffer_string(err));
      return false;
    }

    bool copy = value;
    if (!TEST_CHECK(copy == before))
    {
      TEST_MSG("Initial value is wrong: %s\n", mutt_buffer_string(err));
      return false;
    }

    rc = bool_str_toggle(NeoMutt->sub, "Nectarine", err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("Toggle failed: %s\n", mutt_buffer_string(err));
      return false;
    }

    bool VarNectarine = cs_subset_bool(sub, "Nectarine");
    if (!TEST_CHECK(VarNectarine == after))
    {
      TEST_MSG("Toggle value is wrong: %s\n", mutt_buffer_string(err));
      return false;
    }
    short_line();
  }

  mutt_buffer_reset(err);
  struct ConfigSubset sub2 = { 0 };
  rc = bool_he_toggle(&sub2, he, err);
  if (!TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }

  name = "Olive";
  he = cs_get_elem(cs, name);
  if (!he)
    return false;

  mutt_buffer_reset(err);
  rc = bool_he_toggle(NeoMutt->sub, he, err);
  if (!TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }

  mutt_buffer_reset(err);
  rc = bool_str_toggle(NeoMutt->sub, "unknown", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return false;
  }

  log_line(__func__);
  return true;
}

void test_config_bool(void)
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
  TEST_CHECK(test_reset(sub, err));
  TEST_CHECK(test_validator(sub, err));
  TEST_CHECK(test_inherit(cs, err));
  TEST_CHECK(test_toggle(sub, err));
  mutt_buffer_pool_release(&err);

  test_neomutt_destroy(&NeoMutt);
}
