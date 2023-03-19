/**
 * @file
 * Test code for the Number object
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
#include "config/lib.h"
#include "core/lib.h"
#include "common.h" // IWYU pragma: keep
#include "test_common.h"

// clang-format off
static struct ConfigDef Vars[] = {
  { "Apple",      DT_NUMBER,                 -42, 0, NULL,              }, /* test_initial_values */
  { "Banana",     DT_NUMBER,                 99,  0, NULL,              },
  { "Cherry",     DT_NUMBER,                 33,  0, NULL,              },
  { "Damson",     DT_NUMBER,                 0,   0, NULL,              }, /* test_string_set */
  { "Elderberry", DT_NUMBER|DT_NOT_NEGATIVE, 0,   0, NULL,              },
  { "Fig",        DT_NUMBER,                 0,   0, NULL,              }, /* test_string_get */
  { "Guava",      DT_NUMBER,                 0,   0, NULL,              }, /* test_native_set */
  { "Hawthorn",   DT_NUMBER|DT_NOT_NEGATIVE, 0,   0, NULL,              },
  { "Ilama",      DT_NUMBER,                 0,   0, NULL,              }, /* test_native_get */
  { "Jackfruit",  DT_NUMBER,                 99,  0, NULL,              }, /* test_reset */
  { "Kumquat",    DT_NUMBER,                 33,  0, validator_fail,    },
  { "Lemon",      DT_NUMBER,                 0,   0, validator_succeed, }, /* test_validator */
  { "Mango",      DT_NUMBER,                 0,   0, validator_warn,    },
  { "Nectarine",  DT_NUMBER,                 0,   0, validator_fail,    },
  { "Olive",      DT_NUMBER,                 0,   0, NULL,              }, /* test_inherit */
  { NULL },
};
// clang-format on

static bool test_initial_values(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  short VarApple = cs_subset_number(sub, "Apple");
  short VarBanana = cs_subset_number(sub, "Banana");

  TEST_MSG("Apple = %d\n", VarApple);
  TEST_MSG("Banana = %d\n", VarBanana);

  if (!TEST_CHECK(VarApple == -42))
  {
    TEST_MSG("Expected: %d\n", -42);
    TEST_MSG("Actual  : %d\n", VarApple);
  }

  if (!TEST_CHECK(VarBanana == 99))
  {
    TEST_MSG("Expected: %d\n", 99);
    TEST_MSG("Actual  : %d\n", VarBanana);
  }

  cs_str_string_set(cs, "Apple", "2001", err);
  cs_str_string_set(cs, "Banana", "1999", err);

  VarApple = cs_subset_number(sub, "Apple");
  VarBanana = cs_subset_number(sub, "Banana");

  struct Buffer *value = mutt_buffer_pool_get();

  int rc;

  mutt_buffer_reset(value);
  rc = cs_str_initial_get(cs, "Apple", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(value));
    return false;
  }

  if (!TEST_CHECK(mutt_str_equal(mutt_buffer_string(value), "-42")))
  {
    TEST_MSG("Apple's initial value is wrong: '%s'\n", mutt_buffer_string(value));
    return false;
  }
  VarApple = cs_subset_number(sub, "Apple");
  TEST_MSG("Apple = %d\n", VarApple);
  TEST_MSG("Apple's initial value is '%s'\n", mutt_buffer_string(value));

  mutt_buffer_reset(value);
  rc = cs_str_initial_get(cs, "Banana", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(value));
    return false;
  }

  if (!TEST_CHECK(mutt_str_equal(mutt_buffer_string(value), "99")))
  {
    TEST_MSG("Banana's initial value is wrong: '%s'\n", mutt_buffer_string(value));
    return false;
  }
  VarBanana = cs_subset_number(sub, "Banana");
  TEST_MSG("Banana = %d\n", VarBanana);
  TEST_MSG("Banana's initial value is '%s'\n", NONULL(mutt_buffer_string(value)));

  mutt_buffer_reset(value);
  rc = cs_str_initial_set(cs, "Cherry", "123", value);
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

  short VarCherry = cs_subset_number(sub, "Cherry");
  TEST_MSG("Cherry = %d\n", VarCherry);
  TEST_MSG("Cherry's initial value is %s\n", mutt_buffer_string(value));

  mutt_buffer_pool_release(&value);
  log_line(__func__);
  return true;
}

static bool test_string_set(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *valid[] = { "-123", "0", "-42", "456" };
  int numbers[] = { -123, 0, -42, 456 };
  const char *invalid[] = { "-32769", "32768", "junk", "", NULL };
  const char *name = "Damson";

  int rc;
  short VarDamson;
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    cs_str_native_set(cs, name, -42, NULL);

    TEST_MSG("Setting %s to %s\n", name, valid[i]);
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

    VarDamson = cs_subset_number(sub, "Damson");
    if (!TEST_CHECK(VarDamson == numbers[i]))
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      return false;
    }
    TEST_MSG("%s = %d, set by '%s'\n", name, VarDamson, valid[i]);
    short_line();
  }

  for (unsigned int i = 0; i < mutt_array_size(invalid); i++)
  {
    TEST_MSG("Setting %s to %s\n", name, NONULL(invalid[i]));
    mutt_buffer_reset(err);
    rc = cs_str_string_set(cs, name, invalid[i], err);
    if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
    {
      TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
    }
    else
    {
      VarDamson = cs_subset_number(sub, "Damson");
      TEST_MSG("%s = %d, set by '%s'\n", name, VarDamson, invalid[i]);
      TEST_MSG("This test should have failed\n");
      return false;
    }
    short_line();
  }

  name = "Elderberry";
  mutt_buffer_reset(err);
  TEST_MSG("Setting %s to %s\n", name, "-42");
  rc = cs_str_string_set(cs, name, "-42", err);
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

static bool test_string_get(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;
  const char *name = "Fig";

  cs_str_native_set(cs, name, 123, NULL);
  mutt_buffer_reset(err);
  int rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", mutt_buffer_string(err));
    return false;
  }
  short VarFig = cs_subset_number(sub, "Fig");
  TEST_MSG("%s = %d, %s\n", name, VarFig, mutt_buffer_string(err));

  cs_str_native_set(cs, name, -789, NULL);
  mutt_buffer_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", mutt_buffer_string(err));
    return false;
  }
  VarFig = cs_subset_number(sub, "Fig");
  TEST_MSG("%s = %d, %s\n", name, VarFig, mutt_buffer_string(err));

  log_line(__func__);
  return true;
}

static bool test_native_set(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;
  const char *name = "Guava";
  short value = 12345;

  TEST_MSG("Setting %s to %d\n", name, value);
  cs_str_native_set(cs, name, 0, NULL);
  mutt_buffer_reset(err);
  int rc = cs_str_native_set(cs, name, value, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  short VarGuava = cs_subset_number(sub, "Guava");
  if (!TEST_CHECK(VarGuava == value))
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    return false;
  }

  TEST_MSG("%s = %d, set to '%d'\n", name, VarGuava, value);

  short_line();
  TEST_MSG("Setting %s to %d\n", name, value);
  rc = cs_str_native_set(cs, name, value, err);
  if (TEST_CHECK((rc & CSR_SUC_NO_CHANGE) != 0))
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return false;
  }

  name = "Hawthorn";
  value = -42;
  short_line();
  TEST_MSG("Setting %s to %d\n", name, value);
  rc = cs_str_native_set(cs, name, value, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return false;
  }

  int invalid[] = { -32769, 32768 };
  for (unsigned int i = 0; i < mutt_array_size(invalid); i++)
  {
    short_line();
    cs_str_native_set(cs, name, 123, NULL);
    TEST_MSG("Setting %s to %d\n", name, invalid[i]);
    mutt_buffer_reset(err);
    rc = cs_str_native_set(cs, name, invalid[i], err);
    if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
    {
      TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
    }
    else
    {
      VarGuava = cs_subset_number(sub, "Guava");
      TEST_MSG("%s = %d, set by '%d'\n", name, VarGuava, invalid[i]);
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
  const char *name = "Ilama";

  cs_str_native_set(cs, name, 3456, NULL);
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

static bool test_string_plus_equals(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *valid[] = { "-123", "0", "-42", "456" };
  int numbers[] = { -165, -42, -84, 414 };
  const char *invalid[] = { "-33183", "111132868", "junk", "", NULL };
  const char *name = "Damson";

  int rc;
  short VarDamson;
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    cs_str_native_set(cs, name, -42, NULL);

    VarDamson = cs_subset_number(sub, "Damson");
    TEST_MSG("Increasing %s with initial value %d by %s\n", name, VarDamson, valid[i]);
    mutt_buffer_reset(err);
    rc = cs_str_string_plus_equals(cs, name, valid[i], err);
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

    VarDamson = cs_subset_number(sub, "Damson");
    if (!TEST_CHECK(VarDamson == numbers[i]))
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      return false;
    }
    TEST_MSG("%s = %d, set by '%s'\n", name, VarDamson, valid[i]);
    short_line();
  }

  for (unsigned int i = 0; i < mutt_array_size(invalid); i++)
  {
    VarDamson = cs_subset_number(sub, "Damson");
    TEST_MSG("Increasing %s with initial value %d by %s\n", name, VarDamson,
             NONULL(invalid[i]));
    mutt_buffer_reset(err);
    rc = cs_str_string_plus_equals(cs, name, invalid[i], err);
    if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
    {
      TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
    }
    else
    {
      VarDamson = cs_subset_number(sub, "Damson");
      TEST_MSG("%s = %d, set by '%s'\n", name, VarDamson, invalid[i]);
      TEST_MSG("This test should have failed\n");
      return false;
    }
    short_line();
  }

  name = "Elderberry";
  mutt_buffer_reset(err);
  TEST_MSG("Increasing %s by %s\n", name, "-42");
  rc = cs_str_string_plus_equals(cs, name, "-42", err);
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

static bool test_string_minus_equals(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *valid[] = { "-123", "0", "-42", "456" };
  int numbers[] = { 81, -42, 0, -498 };
  const char *invalid[] = { "32271", "-1844674407370955161000005", "junk", "", NULL };
  const char *name = "Damson";

  int rc;
  short VarDamson;
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    cs_str_native_set(cs, name, -42, NULL);

    VarDamson = cs_subset_number(sub, "Damson");
    TEST_MSG("Decreasing %s with initial value %d by %s\n", name, VarDamson, valid[i]);
    mutt_buffer_reset(err);
    rc = cs_str_string_minus_equals(cs, name, valid[i], err);
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

    VarDamson = cs_subset_number(sub, "Damson");
    if (!TEST_CHECK(VarDamson == numbers[i]))
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      return false;
    }
    TEST_MSG("%s = %d, set by '%s'\n", name, VarDamson, valid[i]);
    short_line();
  }

  for (unsigned int i = 0; i < mutt_array_size(invalid); i++)
  {
    VarDamson = cs_subset_number(sub, "Damson");
    TEST_MSG("Decreasing %s with initial value %d by %s\n", name, VarDamson,
             NONULL(invalid[i]));
    mutt_buffer_reset(err);
    rc = cs_str_string_minus_equals(cs, name, invalid[i], err);
    if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
    {
      TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
    }
    else
    {
      VarDamson = cs_subset_number(sub, "Damson");
      TEST_MSG("%s = %d, decreased by '%s'\n", name, VarDamson, invalid[i]);
      TEST_MSG("This test should have failed\n");
      return false;
    }
    short_line();
  }

  name = "Elderberry";
  mutt_buffer_reset(err);
  TEST_MSG("Increasing %s by %s\n", name, "42");
  rc = cs_str_string_minus_equals(cs, name, "42", err);
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

static bool test_reset(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *name = "Jackfruit";
  cs_str_native_set(cs, name, 345, NULL);
  mutt_buffer_reset(err);

  short VarJackfruit = cs_subset_number(sub, "Jackfruit");
  TEST_MSG("%s = %d\n", name, VarJackfruit);
  int rc = cs_str_reset(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  VarJackfruit = cs_subset_number(sub, "Jackfruit");
  if (!TEST_CHECK(VarJackfruit != 345))
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = %d\n", name, VarJackfruit);

  short_line();
  name = "Kumquat";
  mutt_buffer_reset(err);

  short VarKumquat = cs_subset_number(sub, "Kumquat");
  TEST_MSG("Initial: %s = %d\n", name, VarKumquat);
  dont_fail = true;
  rc = cs_str_string_set(cs, name, "99", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  VarKumquat = cs_subset_number(sub, "Kumquat");
  TEST_MSG("Set: %s = %d\n", name, VarKumquat);
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

  VarKumquat = cs_subset_number(sub, "Kumquat");
  if (!TEST_CHECK(VarKumquat == 99))
  {
    TEST_MSG("Value of %s changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = %d\n", name, VarKumquat);

  log_line(__func__);
  return true;
}

static bool test_validator(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *name = "Lemon";
  cs_str_native_set(cs, name, 123, NULL);
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, name, "456", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }
  short VarLemon = cs_subset_number(sub, "Lemon");
  TEST_MSG("String: %s = %d\n", name, VarLemon);
  short_line();

  cs_str_native_set(cs, name, 456, NULL);
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, 123, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }
  VarLemon = cs_subset_number(sub, "Lemon");
  TEST_MSG("Native: %s = %d\n", name, VarLemon);
  short_line();

  cs_str_native_set(cs, name, 456, NULL);
  mutt_buffer_reset(err);
  rc = cs_str_string_plus_equals(cs, name, "123", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }
  VarLemon = cs_subset_number(sub, "Lemon");
  TEST_CHECK(VarLemon == 579);
  TEST_MSG("PlusEquals: %s = %d\n", name, VarLemon);
  short_line();

  cs_str_native_set(cs, name, 456, NULL);
  mutt_buffer_reset(err);
  rc = cs_str_string_minus_equals(cs, name, "123", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }
  VarLemon = cs_subset_number(sub, "Lemon");
  TEST_CHECK(VarLemon == 333);
  TEST_MSG("MinusEquals: %s = %d\n", name, VarLemon);
  short_line();

  name = "Mango";
  cs_str_native_set(cs, name, 123, NULL);
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "456", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }
  short VarMango = cs_subset_number(sub, "Mango");
  TEST_MSG("String: %s = %d\n", name, VarMango);
  short_line();

  cs_str_native_set(cs, name, 456, NULL);
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, 123, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }
  VarMango = cs_subset_number(sub, "Mango");
  TEST_MSG("Native: %s = %d\n", name, VarMango);
  short_line();

  name = "Nectarine";
  dont_fail = true;
  cs_str_native_set(cs, name, 123, NULL);
  dont_fail = false;
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "456", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }
  short VarNectarine = cs_subset_number(sub, "Nectarine");
  TEST_MSG("String: %s = %d\n", name, VarNectarine);
  short_line();

  struct HashElem *he = cs_get_elem(cs, name);
  cs_str_native_set(cs, name, 123, NULL);
  mutt_buffer_reset(err);
  rc = cs_he_native_set(cs, he, 456, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }
  VarNectarine = cs_subset_number(sub, "Nectarine");
  TEST_MSG("String: %s = %d\n", name, VarNectarine);
  short_line();

  dont_fail = true;
  cs_str_native_set(cs, name, 456, NULL);
  dont_fail = false;
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, 123, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }
  VarNectarine = cs_subset_number(sub, "Nectarine");
  TEST_MSG("Native: %s = %d\n", name, VarNectarine);
  short_line();

  dont_fail = true;
  cs_str_native_set(cs, name, 456, NULL);
  dont_fail = false;
  mutt_buffer_reset(err);
  rc = cs_str_string_plus_equals(cs, name, "123", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }
  VarNectarine = cs_subset_number(sub, "Nectarine");
  TEST_CHECK(VarNectarine == 456);
  TEST_MSG("PlusEquals: %s = %d\n", name, VarNectarine);
  short_line();

  dont_fail = true;
  cs_str_native_set(cs, name, 456, NULL);
  dont_fail = false;
  mutt_buffer_reset(err);
  rc = cs_str_string_minus_equals(cs, name, "123", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }
  VarNectarine = cs_subset_number(sub, "Nectarine");
  TEST_CHECK(VarNectarine == 456);
  TEST_MSG("MinusEquals: %s = %d\n", name, VarNectarine);

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
  const char *parent = "Olive";
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
  cs_str_native_set(cs, parent, 123, NULL);
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, parent, "456", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", mutt_buffer_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);
  short_line();

  // set child
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, child, "-99", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", mutt_buffer_string(err));
    goto ti_out;
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

  // reset parent
  mutt_buffer_reset(err);
  rc = cs_str_reset(cs, parent, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", mutt_buffer_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);
  short_line();

  // plus_equals parent
  cs_str_native_set(cs, parent, 123, NULL);
  mutt_buffer_reset(err);
  rc = cs_str_string_plus_equals(cs, parent, "456", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", mutt_buffer_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);
  short_line();

  // plus_equals child
  mutt_buffer_reset(err);
  rc = cs_str_string_plus_equals(cs, child, "-99", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", mutt_buffer_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);
  short_line();

  // minus_equals parent
  cs_str_native_set(cs, parent, 123, NULL);
  mutt_buffer_reset(err);
  rc = cs_str_string_minus_equals(cs, parent, "456", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", mutt_buffer_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);
  short_line();

  // minus_equals child
  mutt_buffer_reset(err);
  rc = cs_str_string_minus_equals(cs, child, "-99", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", mutt_buffer_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);
  short_line();

  log_line(__func__);
  result = true;
ti_out:
  account_free(&a);
  cs_subset_free(&sub);
  return result;
}

void test_config_number(void)
{
  NeoMutt = test_neomutt_create();
  struct ConfigSubset *sub = NeoMutt->sub;
  struct ConfigSet *cs = sub->cs;

  dont_fail = true;
  if (!cs_register_variables(cs, Vars, DT_NO_FLAGS))
    return;
  dont_fail = false;

  notify_observer_add(NeoMutt->notify, NT_CONFIG, log_observer, 0);

  set_list(cs);

  struct Buffer *err = mutt_buffer_pool_get();
  TEST_CHECK(test_initial_values(sub, err));
  TEST_CHECK(test_string_set(sub, err));
  TEST_CHECK(test_string_get(sub, err));
  TEST_CHECK(test_string_plus_equals(sub, err));
  TEST_CHECK(test_string_minus_equals(sub, err));
  TEST_CHECK(test_native_set(sub, err));
  TEST_CHECK(test_native_get(sub, err));
  TEST_CHECK(test_reset(sub, err));
  TEST_CHECK(test_validator(sub, err));
  TEST_CHECK(test_inherit(cs, err));
  mutt_buffer_pool_release(&err);

  test_neomutt_destroy(&NeoMutt);
}
