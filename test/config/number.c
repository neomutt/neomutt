/**
 * @file
 * Test code for the Number object
 *
 * @authors
 * Copyright (C) 2018-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 Jakub Jindra <jakub.jindra@socialbakers.com>
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
  { "Elderberry", DT_NUMBER|D_INTEGER_NOT_NEGATIVE, 0, 0, NULL,         },
  { "Fig",        DT_NUMBER,                 0,   0, NULL,              }, /* test_string_get */
  { "Guava",      DT_NUMBER,                 0,   0, NULL,              }, /* test_native_set */
  { "Hawthorn",   DT_NUMBER|D_INTEGER_NOT_NEGATIVE, 0, 0, NULL,         },
  { "Ilama",      DT_NUMBER,                 0,   0, NULL,              }, /* test_native_get */
  { "Jackfruit",  DT_NUMBER,                 99,  0, NULL,              }, /* test_reset */
  { "Kumquat",    DT_NUMBER,                 33,  0, validator_fail,    },
  { "Lemon",      DT_NUMBER,                 0,   0, validator_succeed, }, /* test_validator */
  { "Mango",      DT_NUMBER,                 0,   0, validator_warn,    },
  { "Nectarine",  DT_NUMBER,                 0,   0, validator_fail,    },
  { "Olive",      DT_NUMBER,                 0,   0, NULL,              }, /* test_inherit */
  { "Papaya",     DT_NUMBER | D_ON_STARTUP,  1,   0, NULL,              }, /* startup */
  { "Quandong",   DT_NUMBER,               123,   0, NULL,              }, /* test_toggle */
  { "Raspberry",  DT_BOOL,                   0,   0, NULL,              },
  { NULL },
};
// clang-format on

static bool test_initial_values(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  short VarApple = cs_subset_number(sub, "Apple");
  short VarBanana = cs_subset_number(sub, "Banana");

  TEST_MSG("Apple = %d", VarApple);
  TEST_MSG("Banana = %d", VarBanana);

  if (!TEST_CHECK(VarApple == -42))
  {
    TEST_MSG("Expected: %d", -42);
    TEST_MSG("Actual  : %d", VarApple);
  }

  if (!TEST_CHECK(VarBanana == 99))
  {
    TEST_MSG("Expected: %d", 99);
    TEST_MSG("Actual  : %d", VarBanana);
  }

  cs_str_string_set(cs, "Apple", "2001", err);
  cs_str_string_set(cs, "Banana", "1999", err);

  VarApple = cs_subset_number(sub, "Apple");
  VarBanana = cs_subset_number(sub, "Banana");

  struct Buffer *value = buf_pool_get();

  int rc;

  buf_reset(value);
  rc = cs_str_initial_get(cs, "Apple", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(value));
    return false;
  }

  if (!TEST_CHECK_STR_EQ(buf_string(value), "-42"))
  {
    TEST_MSG("Apple's initial value is wrong: '%s'", buf_string(value));
    return false;
  }
  VarApple = cs_subset_number(sub, "Apple");
  TEST_MSG("Apple = %d", VarApple);
  TEST_MSG("Apple's initial value is '%s'", buf_string(value));

  buf_reset(value);
  rc = cs_str_initial_get(cs, "Banana", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(value));
    return false;
  }

  if (!TEST_CHECK_STR_EQ(buf_string(value), "99"))
  {
    TEST_MSG("Banana's initial value is wrong: '%s'", buf_string(value));
    return false;
  }
  VarBanana = cs_subset_number(sub, "Banana");
  TEST_MSG("Banana = %d", VarBanana);
  TEST_MSG("Banana's initial value is '%s'", NONULL(buf_string(value)));

  buf_reset(value);
  rc = cs_str_initial_set(cs, "Cherry", "123", value);
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

  short VarCherry = cs_subset_number(sub, "Cherry");
  TEST_MSG("Cherry = %d", VarCherry);
  TEST_MSG("Cherry's initial value is %s", buf_string(value));

  buf_pool_release(&value);
  log_line(__func__);
  return true;
}

static bool test_string_set(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *valid[] = { "-123", "0", "-42", "456", "", NULL };
  const int numbers[] = { -123, 0, -42, 456, 0, 0 };
  const char *invalid[] = { "-32769", "32768", "junk" };
  const char *name = "Damson";

  int rc;
  short VarDamson;
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    cs_str_native_set(cs, name, -42, NULL);

    TEST_MSG("Setting %s to %s", name, valid[i]);
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

    VarDamson = cs_subset_number(sub, "Damson");
    if (!TEST_CHECK(VarDamson == numbers[i]))
    {
      TEST_MSG("Value of %s wasn't changed", name);
      return false;
    }
    TEST_MSG("%s = %d, set by '%s'", name, VarDamson, valid[i]);
    short_line();
  }

  for (unsigned int i = 0; i < mutt_array_size(invalid); i++)
  {
    TEST_MSG("Setting %s to %s", name, NONULL(invalid[i]));
    buf_reset(err);
    rc = cs_str_string_set(cs, name, invalid[i], err);
    if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
    {
      TEST_MSG("Expected error: %s", buf_string(err));
    }
    else
    {
      VarDamson = cs_subset_number(sub, "Damson");
      TEST_MSG("%s = %d, set by '%s'", name, VarDamson, invalid[i]);
      TEST_MSG("This test should have failed");
      return false;
    }
    short_line();
  }

  name = "Elderberry";
  buf_reset(err);
  TEST_MSG("Setting %s to %s", name, "-42");
  rc = cs_str_string_set(cs, name, "-42", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s", buf_string(err));
  }
  else
  {
    TEST_MSG("This test should have failed");
    return false;
  }

  name = "Papaya";
  rc = cs_str_string_set(cs, name, "1", err);
  TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS);

  rc = cs_str_string_set(cs, name, "0", err);
  TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS);

  log_line(__func__);
  return true;
}

static bool test_string_get(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;
  const char *name = "Fig";

  cs_str_native_set(cs, name, 123, NULL);
  buf_reset(err);
  int rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s", buf_string(err));
    return false;
  }
  short VarFig = cs_subset_number(sub, "Fig");
  TEST_MSG("%s = %d, %s", name, VarFig, buf_string(err));

  cs_str_native_set(cs, name, -789, NULL);
  buf_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s", buf_string(err));
    return false;
  }
  VarFig = cs_subset_number(sub, "Fig");
  TEST_MSG("%s = %d, %s", name, VarFig, buf_string(err));

  log_line(__func__);
  return true;
}

static bool test_native_set(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;
  const char *name = "Guava";
  short value = 12345;

  TEST_MSG("Setting %s to %d", name, value);
  cs_str_native_set(cs, name, 0, NULL);
  buf_reset(err);
  int rc = cs_str_native_set(cs, name, value, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }

  short VarGuava = cs_subset_number(sub, "Guava");
  if (!TEST_CHECK(VarGuava == value))
  {
    TEST_MSG("Value of %s wasn't changed", name);
    return false;
  }

  TEST_MSG("%s = %d, set to '%d'", name, VarGuava, value);

  short_line();
  TEST_MSG("Setting %s to %d", name, value);
  rc = cs_str_native_set(cs, name, value, err);
  if (TEST_CHECK((rc & CSR_SUC_NO_CHANGE) != 0))
  {
    TEST_MSG("Value of %s wasn't changed", name);
  }
  else
  {
    TEST_MSG("This test should have failed");
    return false;
  }

  name = "Hawthorn";
  value = -42;
  short_line();
  TEST_MSG("Setting %s to %d", name, value);
  rc = cs_str_native_set(cs, name, value, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s", buf_string(err));
  }
  else
  {
    TEST_MSG("This test should have failed");
    return false;
  }

  int invalid[] = { -32769, 32768 };
  for (unsigned int i = 0; i < mutt_array_size(invalid); i++)
  {
    short_line();
    cs_str_native_set(cs, name, 123, NULL);
    TEST_MSG("Setting %s to %d", name, invalid[i]);
    buf_reset(err);
    rc = cs_str_native_set(cs, name, invalid[i], err);
    if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
    {
      TEST_MSG("Expected error: %s", buf_string(err));
    }
    else
    {
      VarGuava = cs_subset_number(sub, "Guava");
      TEST_MSG("%s = %d, set by '%d'", name, VarGuava, invalid[i]);
      TEST_MSG("This test should have failed");
      return false;
    }
  }

  name = "Papaya";
  rc = cs_str_native_set(cs, name, 1, err);
  TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS);

  rc = cs_str_native_set(cs, name, 0, err);
  TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS);

  log_line(__func__);
  return true;
}

static bool test_native_get(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;
  const char *name = "Ilama";

  cs_str_native_set(cs, name, 3456, NULL);
  buf_reset(err);
  intptr_t value = cs_str_native_get(cs, name, err);
  if (!TEST_CHECK(value != INT_MIN))
  {
    TEST_MSG("Get failed: %s", buf_string(err));
    return false;
  }
  TEST_MSG("%s = %ld", name, value);

  log_line(__func__);
  return true;
}

static bool test_string_plus_equals(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *valid[] = { "-123", "0", "-42", "456" };
  const int numbers[] = { -165, -42, -84, 414 };
  const char *invalid[] = { "-33183", "111132868", "junk", "", NULL };
  const char *name = "Damson";

  int rc;
  short VarDamson;
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    cs_str_native_set(cs, name, -42, NULL);

    VarDamson = cs_subset_number(sub, "Damson");
    TEST_MSG("Increasing %s with initial value %d by %s", name, VarDamson, valid[i]);
    buf_reset(err);
    rc = cs_str_string_plus_equals(cs, name, valid[i], err);
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

    VarDamson = cs_subset_number(sub, "Damson");
    if (!TEST_CHECK(VarDamson == numbers[i]))
    {
      TEST_MSG("Value of %s wasn't changed", name);
      return false;
    }
    TEST_MSG("%s = %d, set by '%s'", name, VarDamson, valid[i]);
    short_line();
  }

  for (unsigned int i = 0; i < mutt_array_size(invalid); i++)
  {
    VarDamson = cs_subset_number(sub, "Damson");
    TEST_MSG("Increasing %s with initial value %d by %s", name, VarDamson,
             NONULL(invalid[i]));
    buf_reset(err);
    rc = cs_str_string_plus_equals(cs, name, invalid[i], err);
    if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
    {
      TEST_MSG("Expected error: %s", buf_string(err));
    }
    else
    {
      VarDamson = cs_subset_number(sub, "Damson");
      TEST_MSG("%s = %d, set by '%s'", name, VarDamson, invalid[i]);
      TEST_MSG("This test should have failed");
      return false;
    }
    short_line();
  }

  name = "Elderberry";
  buf_reset(err);
  TEST_MSG("Increasing %s by %s", name, "-42");
  rc = cs_str_string_plus_equals(cs, name, "-42", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s", buf_string(err));
  }
  else
  {
    TEST_MSG("This test should have failed");
    return false;
  }

  name = "Papaya";
  rc = cs_str_string_plus_equals(cs, name, "1", err);
  TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS);

  log_line(__func__);
  return true;
}

static bool test_string_minus_equals(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *valid[] = { "-123", "0", "-42", "456" };
  const int numbers[] = { 81, -42, 0, -498 };
  const char *invalid[] = { "32271", "-1844674407370955161000005", "junk", "", NULL };
  const char *name = "Damson";

  int rc;
  short VarDamson;
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    cs_str_native_set(cs, name, -42, NULL);

    VarDamson = cs_subset_number(sub, "Damson");
    TEST_MSG("Decreasing %s with initial value %d by %s", name, VarDamson, valid[i]);
    buf_reset(err);
    rc = cs_str_string_minus_equals(cs, name, valid[i], err);
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

    VarDamson = cs_subset_number(sub, "Damson");
    if (!TEST_CHECK(VarDamson == numbers[i]))
    {
      TEST_MSG("Value of %s wasn't changed", name);
      return false;
    }
    TEST_MSG("%s = %d, set by '%s'", name, VarDamson, valid[i]);
    short_line();
  }

  for (unsigned int i = 0; i < mutt_array_size(invalid); i++)
  {
    VarDamson = cs_subset_number(sub, "Damson");
    TEST_MSG("Decreasing %s with initial value %d by %s", name, VarDamson,
             NONULL(invalid[i]));
    buf_reset(err);
    rc = cs_str_string_minus_equals(cs, name, invalid[i], err);
    if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
    {
      TEST_MSG("Expected error: %s", buf_string(err));
    }
    else
    {
      VarDamson = cs_subset_number(sub, "Damson");
      TEST_MSG("%s = %d, decreased by '%s'", name, VarDamson, invalid[i]);
      TEST_MSG("This test should have failed");
      return false;
    }
    short_line();
  }

  name = "Elderberry";
  buf_reset(err);
  TEST_MSG("Increasing %s by %s", name, "42");
  rc = cs_str_string_minus_equals(cs, name, "42", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s", buf_string(err));
  }
  else
  {
    TEST_MSG("This test should have failed");
    return false;
  }

  name = "Papaya";
  rc = cs_str_string_minus_equals(cs, name, "1", err);
  TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS);

  log_line(__func__);
  return true;
}

static bool test_reset(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *name = "Jackfruit";
  cs_str_native_set(cs, name, 345, NULL);
  buf_reset(err);

  short VarJackfruit = cs_subset_number(sub, "Jackfruit");
  TEST_MSG("%s = %d", name, VarJackfruit);
  int rc = cs_str_reset(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }

  VarJackfruit = cs_subset_number(sub, "Jackfruit");
  if (!TEST_CHECK(VarJackfruit != 345))
  {
    TEST_MSG("Value of %s wasn't changed", name);
    return false;
  }

  TEST_MSG("Reset: %s = %d", name, VarJackfruit);

  short_line();
  name = "Kumquat";
  buf_reset(err);

  short VarKumquat = cs_subset_number(sub, "Kumquat");
  TEST_MSG("Initial: %s = %d", name, VarKumquat);
  dont_fail = true;
  rc = cs_str_string_set(cs, name, "99", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  VarKumquat = cs_subset_number(sub, "Kumquat");
  TEST_MSG("Set: %s = %d", name, VarKumquat);
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

  VarKumquat = cs_subset_number(sub, "Kumquat");
  if (!TEST_CHECK(VarKumquat == 99))
  {
    TEST_MSG("Value of %s changed", name);
    return false;
  }

  TEST_MSG("Reset: %s = %ld", name, VarKumquat);

  name = "Papaya";
  rc = cs_str_reset(cs, name, err);
  TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS);

  StartupComplete = false;
  rc = cs_str_native_set(cs, name, 0, err);
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

  const char *name = "Lemon";
  cs_str_native_set(cs, name, 123, NULL);
  buf_reset(err);
  int rc = cs_str_string_set(cs, name, "456", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }
  short VarLemon = cs_subset_number(sub, "Lemon");
  TEST_MSG("String: %s = %d", name, VarLemon);
  short_line();

  cs_str_native_set(cs, name, 456, NULL);
  buf_reset(err);
  rc = cs_str_native_set(cs, name, 123, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }
  VarLemon = cs_subset_number(sub, "Lemon");
  TEST_MSG("Native: %s = %d", name, VarLemon);
  short_line();

  cs_str_native_set(cs, name, 456, NULL);
  buf_reset(err);
  rc = cs_str_string_plus_equals(cs, name, "123", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }
  VarLemon = cs_subset_number(sub, "Lemon");
  TEST_CHECK(VarLemon == 579);
  TEST_MSG("PlusEquals: %s = %d", name, VarLemon);
  short_line();

  cs_str_native_set(cs, name, 456, NULL);
  buf_reset(err);
  rc = cs_str_string_minus_equals(cs, name, "123", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }
  VarLemon = cs_subset_number(sub, "Lemon");
  TEST_CHECK(VarLemon == 333);
  TEST_MSG("MinusEquals: %s = %d", name, VarLemon);
  short_line();

  name = "Mango";
  cs_str_native_set(cs, name, 123, NULL);
  buf_reset(err);
  rc = cs_str_string_set(cs, name, "456", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }
  short VarMango = cs_subset_number(sub, "Mango");
  TEST_MSG("String: %s = %d", name, VarMango);
  short_line();

  cs_str_native_set(cs, name, 456, NULL);
  buf_reset(err);
  rc = cs_str_native_set(cs, name, 123, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }
  VarMango = cs_subset_number(sub, "Mango");
  TEST_MSG("Native: %s = %d", name, VarMango);
  short_line();

  name = "Nectarine";
  dont_fail = true;
  cs_str_native_set(cs, name, 123, NULL);
  dont_fail = false;
  buf_reset(err);
  rc = cs_str_string_set(cs, name, "456", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }
  short VarNectarine = cs_subset_number(sub, "Nectarine");
  TEST_MSG("String: %s = %d", name, VarNectarine);
  short_line();

  struct HashElem *he = cs_get_elem(cs, name);
  cs_str_native_set(cs, name, 123, NULL);
  buf_reset(err);
  rc = cs_he_native_set(cs, he, 456, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }
  VarNectarine = cs_subset_number(sub, "Nectarine");
  TEST_MSG("String: %s = %d", name, VarNectarine);
  short_line();

  dont_fail = true;
  cs_str_native_set(cs, name, 456, NULL);
  dont_fail = false;
  buf_reset(err);
  rc = cs_str_native_set(cs, name, 123, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }
  VarNectarine = cs_subset_number(sub, "Nectarine");
  TEST_MSG("Native: %s = %d", name, VarNectarine);
  short_line();

  dont_fail = true;
  cs_str_native_set(cs, name, 456, NULL);
  dont_fail = false;
  buf_reset(err);
  rc = cs_str_string_plus_equals(cs, name, "123", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }
  VarNectarine = cs_subset_number(sub, "Nectarine");
  TEST_CHECK(VarNectarine == 456);
  TEST_MSG("PlusEquals: %s = %d", name, VarNectarine);
  short_line();

  dont_fail = true;
  cs_str_native_set(cs, name, 456, NULL);
  dont_fail = false;
  buf_reset(err);
  rc = cs_str_string_minus_equals(cs, name, "123", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }
  VarNectarine = cs_subset_number(sub, "Nectarine");
  TEST_CHECK(VarNectarine == 456);
  TEST_MSG("MinusEquals: %s = %d", name, VarNectarine);

  log_line(__func__);
  return true;
}

static void dump_native(struct ConfigSet *cs, const char *parent, const char *child)
{
  intptr_t pval = cs_str_native_get(cs, parent, NULL);
  intptr_t cval = cs_str_native_get(cs, child, NULL);

  TEST_MSG("%15s = %ld", parent, pval);
  TEST_MSG("%15s = %ld", child, cval);
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
    TEST_MSG("Error: %s", buf_string(err));
    goto ti_out;
  }

  // set parent
  cs_str_native_set(cs, parent, 123, NULL);
  buf_reset(err);
  int rc = cs_str_string_set(cs, parent, "456", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s", buf_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);
  short_line();

  // set child
  buf_reset(err);
  rc = cs_str_string_set(cs, child, "-99", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s", buf_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);
  short_line();

  // reset child
  buf_reset(err);
  rc = cs_str_reset(cs, child, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s", buf_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);
  short_line();

  // reset parent
  buf_reset(err);
  rc = cs_str_reset(cs, parent, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s", buf_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);
  short_line();

  // plus_equals parent
  cs_str_native_set(cs, parent, 123, NULL);
  buf_reset(err);
  rc = cs_str_string_plus_equals(cs, parent, "456", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s", buf_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);
  short_line();

  // plus_equals child
  buf_reset(err);
  rc = cs_str_string_plus_equals(cs, child, "-99", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s", buf_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);
  short_line();

  // minus_equals parent
  cs_str_native_set(cs, parent, 123, NULL);
  buf_reset(err);
  rc = cs_str_string_minus_equals(cs, parent, "456", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s", buf_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);
  short_line();

  // minus_equals child
  buf_reset(err);
  rc = cs_str_string_minus_equals(cs, child, "-99", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s", buf_string(err));
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

static bool test_toggle(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;
  const char *name = "Quandong";
  int rc;

  struct HashElem *he = cs_get_elem(cs, name);
  if (!he)
    return false;

  int value = 0;
  const int initial = cs_subset_number(sub, name);

  rc = number_he_toggle(NULL, he, err);
  TEST_CHECK(rc == CSR_ERR_CODE);

  rc = number_he_toggle(sub, NULL, err);
  TEST_CHECK(rc == CSR_ERR_CODE);

  rc = number_he_toggle(sub, he, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Toggle failed: %s", buf_string(err));
    return false;
  }

  value = cs_subset_number(sub, name);
  if (!TEST_CHECK(value == 0))
  {
    TEST_MSG("Toggle value is wrong: %d, expected: %d", value, 0);
    return false;
  }

  rc = number_he_toggle(sub, he, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Toggle failed: %s", buf_string(err));
    return false;
  }

  value = cs_subset_number(sub, name);
  if (!TEST_CHECK(value == initial))
  {
    TEST_MSG("Toggle value is wrong: %d, expected: %d", value, initial);
    return false;
  }

  rc = number_he_toggle(sub, he, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Toggle failed: %s", buf_string(err));
    return false;
  }

  value = cs_subset_number(sub, name);
  if (!TEST_CHECK(value == 0))
  {
    TEST_MSG("Toggle value is wrong: %d, expected: %d", value, 0);
    return false;
  }

  rc = cs_str_native_set(cs, name, -123, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Set failed: %s", buf_string(err));
    return false;
  }

  value = cs_subset_number(sub, name);
  if (!TEST_CHECK(value == -123))
  {
    TEST_MSG("Toggle value is wrong: %d, expected: %d", value, -123);
    return false;
  }

  rc = number_he_toggle(sub, he, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Toggle failed: %s", buf_string(err));
    return false;
  }

  value = cs_subset_number(sub, name);
  if (!TEST_CHECK(value == 0))
  {
    TEST_MSG("Toggle value is wrong: %d, expected: %d", value, 0);
    return false;
  }

  rc = number_he_toggle(sub, he, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Toggle failed: %s", buf_string(err));
    return false;
  }

  value = cs_subset_number(sub, name);
  if (!TEST_CHECK(value == -123))
  {
    TEST_MSG("Toggle value is wrong: %d, expected: %d", value, -123);
    return false;
  }

  name = "Raspberry";
  he = cs_get_elem(cs, name);
  if (!he)
    return false;

  rc = number_he_toggle(sub, he, err);
  TEST_CHECK(rc == CSR_ERR_CODE);

  return true;
}

void test_config_number(void)
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
  TEST_CHECK(test_string_plus_equals(sub, err));
  TEST_CHECK(test_string_minus_equals(sub, err));
  TEST_CHECK(test_native_set(sub, err));
  TEST_CHECK(test_native_get(sub, err));
  TEST_CHECK(test_reset(sub, err));
  TEST_CHECK(test_validator(sub, err));
  TEST_CHECK(test_inherit(cs, err));
  TEST_CHECK(test_toggle(sub, err));
  buf_pool_release(&err);
}
