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
  { "Apple",      DT_LONG,                 -42, 0, NULL,              }, /* test_initial_values */
  { "Banana",     DT_LONG,                 99,  0, NULL,              },
  { "Cherry",     DT_LONG,                 33,  0, NULL,              },
  { "Damson",     DT_LONG,                 0,   0, NULL,              }, /* test_string_set */
  { "Elderberry", DT_LONG|DT_NOT_NEGATIVE, 0,   0, NULL,              },
  { "Fig",        DT_LONG,                 0,   0, NULL,              }, /* test_string_get */
  { "Guava",      DT_LONG,                 0,   0, NULL,              }, /* test_native_set */
  { "Hawthorn",   DT_LONG|DT_NOT_NEGATIVE, 0,   0, NULL,              },
  { "Ilama",      DT_LONG,                 0,   0, NULL,              }, /* test_native_get */
  { "Jackfruit",  DT_LONG,                 99,  0, NULL,              }, /* test_reset */
  { "Kumquat",    DT_LONG,                 33,  0, validator_fail,    },
  { "Lemon",      DT_LONG,                 0,   0, validator_succeed, }, /* test_validator */
  { "Mango",      DT_LONG,                 0,   0, validator_warn,    },
  { "Nectarine",  DT_LONG,                 0,   0, validator_fail,    },
  { "Olive",      DT_LONG,                 0,   0, NULL,              }, /* test_inherit */
  { NULL },
};
// clang-format on

static bool test_initial_values(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  long VarApple = cs_subset_long(sub, "Apple");
  long VarBanana = cs_subset_long(sub, "Banana");

  TEST_MSG("Apple = %ld\n", VarApple);
  TEST_MSG("Banana = %ld\n", VarBanana);

  if (!TEST_CHECK(VarApple == -42))
  {
    TEST_MSG("Expected: %d\n", -42);
    TEST_MSG("Actual  : %ld\n", VarApple);
  }

  if (!TEST_CHECK(VarBanana == 99))
  {
    TEST_MSG("Expected: %d\n", 99);
    TEST_MSG("Actual  : %ld\n", VarBanana);
  }

  cs_str_string_set(cs, "Apple", "2001", err);
  cs_str_string_set(cs, "Banana", "1999", err);

  VarApple = cs_subset_long(sub, "Apple");
  VarBanana = cs_subset_long(sub, "Banana");

  struct Buffer *value = buf_pool_get();

  int rc;

  buf_reset(value);
  rc = cs_str_initial_get(cs, "Apple", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", buf_string(value));
    return false;
  }

  if (!TEST_CHECK_STR_EQ(buf_string(value), "-42"))
  {
    TEST_MSG("Apple's initial value is wrong: '%s'\n", buf_string(value));
    return false;
  }
  VarApple = cs_subset_long(sub, "Apple");
  TEST_MSG("Apple = %ld\n", VarApple);
  TEST_MSG("Apple's initial value is '%s'\n", buf_string(value));

  buf_reset(value);
  rc = cs_str_initial_get(cs, "Banana", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", buf_string(value));
    return false;
  }

  if (!TEST_CHECK_STR_EQ(buf_string(value), "99"))
  {
    TEST_MSG("Banana's initial value is wrong: '%s'\n", buf_string(value));
    return false;
  }
  VarBanana = cs_subset_long(sub, "Banana");
  TEST_MSG("Banana = %ld\n", VarBanana);
  TEST_MSG("Banana's initial value is '%s'\n", NONULL(buf_string(value)));

  buf_reset(value);
  rc = cs_str_initial_set(cs, "Cherry", "123", value);
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

  long VarCherry = cs_subset_long(sub, "Cherry");
  TEST_MSG("Cherry = %ld\n", VarCherry);
  TEST_MSG("Cherry's initial value is %s\n", buf_string(value));

  buf_pool_release(&value);
  log_line(__func__);
  return true;
}

static bool test_string_set(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *valid[] = { "-123", "0", "-42", "456" };
  int longs[] = { -123, 0, -42, 456 };
  const char *invalid[] = { "-9223372036854775809", "9223372036854775808", "junk", "", NULL };
  const char *name = "Damson";

  int rc;
  long VarDamson;
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    cs_str_native_set(cs, name, -42, NULL);

    TEST_MSG("Setting %s to %s\n", name, valid[i]);
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

    VarDamson = cs_subset_long(sub, "Damson");
    if (!TEST_CHECK(VarDamson == longs[i]))
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      return false;
    }
    TEST_MSG("%s = %ld, set by '%s'\n", name, VarDamson, valid[i]);
    short_line();
  }

  for (unsigned int i = 0; i < mutt_array_size(invalid); i++)
  {
    TEST_MSG("Setting %s to %s\n", name, NONULL(invalid[i]));
    buf_reset(err);
    rc = cs_str_string_set(cs, name, invalid[i], err);
    if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
    {
      TEST_MSG("Expected error: %s\n", buf_string(err));
    }
    else
    {
      VarDamson = cs_subset_long(sub, "Damson");
      TEST_MSG("%s = %ld, set by '%s'\n", name, VarDamson, NONULL(invalid[i]));
      TEST_MSG("This test should have failed\n");
      return false;
    }
    short_line();
  }

  name = "Elderberry";
  buf_reset(err);
  TEST_MSG("Setting %s to %s\n", name, "-42");
  rc = cs_str_string_set(cs, name, "-42", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", buf_string(err));
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
  buf_reset(err);
  int rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", buf_string(err));
    return false;
  }
  short VarFig = cs_subset_long(sub, "Fig");
  TEST_MSG("%s = %ld, %s\n", name, VarFig, buf_string(err));

  cs_str_native_set(cs, name, -789, NULL);
  buf_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", buf_string(err));
    return false;
  }
  VarFig = cs_subset_long(sub, "Fig");
  TEST_MSG("%s = %ld, %s\n", name, VarFig, buf_string(err));

  log_line(__func__);
  return true;
}

static bool test_string_plus_equals(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *valid[] = { "-123", "0", "-42", "456" };
  int numbers[] = { -165, -42, -84, 414 };
  const char *invalid[] = { "-9223372036854775809", "9223372036854775808", "junk", "", NULL };
  const char *name = "Damson";

  int rc;
  long VarDamson;
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    cs_str_native_set(cs, name, -42, NULL);

    VarDamson = cs_subset_long(sub, "Damson");
    TEST_MSG("Increasing %s with initial value %d by %s\n", name, VarDamson, valid[i]);
    buf_reset(err);
    rc = cs_str_string_plus_equals(cs, name, valid[i], err);
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

    VarDamson = cs_subset_long(sub, "Damson");
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
    VarDamson = cs_subset_long(sub, "Damson");
    TEST_MSG("Increasing %s with initial value %d by %s\n", name, VarDamson,
             NONULL(invalid[i]));
    buf_reset(err);
    rc = cs_str_string_plus_equals(cs, name, invalid[i], err);
    if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
    {
      TEST_MSG("Expected error: %s\n", buf_string(err));
    }
    else
    {
      VarDamson = cs_subset_long(sub, "Damson");
      TEST_MSG("%s = %d, set by '%s'\n", name, VarDamson, invalid[i]);
      TEST_MSG("This test should have failed\n");
      return false;
    }
    short_line();
  }

  name = "Elderberry";
  buf_reset(err);
  TEST_MSG("Increasing %s by %s\n", name, "-42");
  rc = cs_str_string_plus_equals(cs, name, "-42", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", buf_string(err));
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
  const char *invalid[] = { "-9223372036854775809", "9223372036854775808", "junk", "", NULL };
  const char *name = "Damson";

  int rc;
  long VarDamson;
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    cs_str_native_set(cs, name, -42, NULL);

    VarDamson = cs_subset_long(sub, "Damson");
    TEST_MSG("Decreasing %s with initial value %d by %s\n", name, VarDamson, valid[i]);
    buf_reset(err);
    rc = cs_str_string_minus_equals(cs, name, valid[i], err);
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

    VarDamson = cs_subset_long(sub, "Damson");
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
    VarDamson = cs_subset_long(sub, "Damson");
    TEST_MSG("Decreasing %s with initial value %d by %s\n", name, VarDamson,
             NONULL(invalid[i]));
    buf_reset(err);
    rc = cs_str_string_minus_equals(cs, name, invalid[i], err);
    if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
    {
      TEST_MSG("Expected error: %s\n", buf_string(err));
    }
    else
    {
      VarDamson = cs_subset_long(sub, "Damson");
      TEST_MSG("%s = %d, decreased by '%s'\n", name, VarDamson, invalid[i]);
      TEST_MSG("This test should have failed\n");
      return false;
    }
    short_line();
  }

  name = "Elderberry";
  buf_reset(err);
  TEST_MSG("Increasing %s by %s\n", name, "42");
  rc = cs_str_string_minus_equals(cs, name, "42", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", buf_string(err));
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return false;
  }

  log_line(__func__);
  return true;
}

static bool test_native_set(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;
  const char *name = "Guava";
  long value = 12345;

  TEST_MSG("Setting %s to %ld\n", name, value);
  cs_str_native_set(cs, name, 0, NULL);
  buf_reset(err);
  int rc = cs_str_native_set(cs, name, value, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", buf_string(err));
    return false;
  }

  long VarGuava = cs_subset_long(sub, "Guava");
  if (!TEST_CHECK(VarGuava == value))
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    return false;
  }

  TEST_MSG("%s = %ld, set to '%ld'\n", name, VarGuava, value);

  short_line();
  TEST_MSG("Setting %s to %ld\n", name, value);
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
  TEST_MSG("Setting %s to %ld\n", name, value);
  rc = cs_str_native_set(cs, name, value, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", buf_string(err));
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return false;
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
  buf_reset(err);
  intptr_t value = cs_str_native_get(cs, name, err);
  if (!TEST_CHECK(value != INT_MIN))
  {
    TEST_MSG("Get failed: %s\n", buf_string(err));
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

  const char *name = "Jackfruit";
  cs_str_native_set(cs, name, 345, NULL);
  buf_reset(err);

  long VarJackfruit = cs_subset_long(sub, "Jackfruit");
  TEST_MSG("%s = %ld\n", name, VarJackfruit);
  int rc = cs_str_reset(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", buf_string(err));
    return false;
  }

  VarJackfruit = cs_subset_long(sub, "Jackfruit");
  if (!TEST_CHECK(VarJackfruit != 345))
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = %ld\n", name, VarJackfruit);

  short_line();
  name = "Kumquat";
  buf_reset(err);

  long VarKumquat = cs_subset_long(sub, "Kumquat");
  TEST_MSG("Initial: %s = %ld\n", name, VarKumquat);
  dont_fail = true;
  rc = cs_str_string_set(cs, name, "99", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  VarKumquat = cs_subset_long(sub, "Kumquat");
  TEST_MSG("Set: %s = %ld\n", name, VarKumquat);
  dont_fail = false;

  rc = cs_str_reset(cs, name, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", buf_string(err));
  }
  else
  {
    TEST_MSG("%s\n", buf_string(err));
    return false;
  }

  VarKumquat = cs_subset_long(sub, "Kumquat");
  if (!TEST_CHECK(VarKumquat == 99))
  {
    TEST_MSG("Value of %s changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = %ld\n", name, VarKumquat);

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
    TEST_MSG("%s\n", buf_string(err));
  }
  else
  {
    TEST_MSG("%s\n", buf_string(err));
    return false;
  }
  long VarLemon = cs_subset_long(sub, "Lemon");
  TEST_MSG("String: %s = %ld\n", name, VarLemon);
  short_line();

  cs_str_native_set(cs, name, 456, NULL);
  buf_reset(err);
  rc = cs_str_native_set(cs, name, 123, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", buf_string(err));
  }
  else
  {
    TEST_MSG("%s\n", buf_string(err));
    return false;
  }
  VarLemon = cs_subset_long(sub, "Lemon");
  TEST_MSG("Native: %s = %ld\n", name, VarLemon);
  short_line();

  cs_str_native_set(cs, name, 456, NULL);
  buf_reset(err);
  rc = cs_str_string_plus_equals(cs, name, "123", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", buf_string(err));
  }
  else
  {
    TEST_MSG("%s\n", buf_string(err));
    return false;
  }
  VarLemon = cs_subset_long(sub, "Lemon");
  TEST_CHECK(VarLemon == 579);
  TEST_MSG("PlusEquals: %s = %d\n", name, VarLemon);
  short_line();

  cs_str_native_set(cs, name, 456, NULL);
  buf_reset(err);
  rc = cs_str_string_minus_equals(cs, name, "123", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", buf_string(err));
  }
  else
  {
    TEST_MSG("%s\n", buf_string(err));
    return false;
  }
  VarLemon = cs_subset_long(sub, "Lemon");
  TEST_CHECK(VarLemon == 333);
  TEST_MSG("MinusEquals: %s = %d\n", name, VarLemon);
  short_line();

  name = "Mango";
  cs_str_native_set(cs, name, 123, NULL);
  buf_reset(err);
  rc = cs_str_string_set(cs, name, "456", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", buf_string(err));
  }
  else
  {
    TEST_MSG("%s\n", buf_string(err));
    return false;
  }
  long VarMango = cs_subset_long(sub, "Mango");
  TEST_MSG("String: %s = %ld\n", name, VarMango);
  short_line();

  cs_str_native_set(cs, name, 456, NULL);
  buf_reset(err);
  rc = cs_str_native_set(cs, name, 123, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", buf_string(err));
  }
  else
  {
    TEST_MSG("%s\n", buf_string(err));
    return false;
  }
  VarMango = cs_subset_long(sub, "Mango");
  TEST_MSG("Native: %s = %ld\n", name, VarMango);
  short_line();

  name = "Nectarine";
  dont_fail = true;
  cs_str_native_set(cs, name, 123, NULL);
  dont_fail = false;
  buf_reset(err);
  rc = cs_str_string_set(cs, name, "456", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", buf_string(err));
  }
  else
  {
    TEST_MSG("%s\n", buf_string(err));
    return false;
  }
  long VarNectarine = cs_subset_long(sub, "Nectarine");
  TEST_MSG("String: %s = %ld\n", name, VarNectarine);
  short_line();

  dont_fail = true;
  cs_str_native_set(cs, name, 456, NULL);
  dont_fail = false;
  buf_reset(err);
  rc = cs_str_native_set(cs, name, 123, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", buf_string(err));
  }
  else
  {
    TEST_MSG("%s\n", buf_string(err));
    return false;
  }
  VarNectarine = cs_subset_long(sub, "Nectarine");
  TEST_MSG("Native: %s = %ld\n", name, VarNectarine);
  short_line();

  dont_fail = true;
  cs_str_native_set(cs, name, 456, NULL);
  dont_fail = false;
  buf_reset(err);
  rc = cs_str_string_plus_equals(cs, name, "123", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", buf_string(err));
  }
  else
  {
    TEST_MSG("%s\n", buf_string(err));
    return false;
  }
  VarNectarine = cs_subset_long(sub, "Nectarine");
  TEST_CHECK(VarNectarine == 456);
  TEST_MSG("PlusEquals: %s = %d\n", name, VarNectarine);
  short_line();

  dont_fail = true;
  cs_str_native_set(cs, name, 456, NULL);
  dont_fail = false;
  buf_reset(err);
  rc = cs_str_string_minus_equals(cs, name, "123", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", buf_string(err));
  }
  else
  {
    TEST_MSG("%s\n", buf_string(err));
    return false;
  }
  VarNectarine = cs_subset_long(sub, "Nectarine");
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
    TEST_MSG("Error: %s\n", buf_string(err));
    goto ti_out;
  }

  // set parent
  cs_str_native_set(cs, parent, 123, NULL);
  buf_reset(err);
  int rc = cs_str_string_set(cs, parent, "456", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", buf_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);
  short_line();

  // set child
  buf_reset(err);
  rc = cs_str_string_set(cs, child, "-99", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", buf_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);
  short_line();

  // reset child
  buf_reset(err);
  rc = cs_str_reset(cs, child, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", buf_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);
  short_line();

  // reset parent
  buf_reset(err);
  rc = cs_str_reset(cs, parent, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", buf_string(err));
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

void test_config_long(void)
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
  TEST_CHECK(test_string_plus_equals(sub, err));
  TEST_CHECK(test_string_minus_equals(sub, err));
  TEST_CHECK(test_native_set(sub, err));
  TEST_CHECK(test_native_get(sub, err));
  TEST_CHECK(test_reset(sub, err));
  TEST_CHECK(test_validator(sub, err));
  TEST_CHECK(test_inherit(cs, err));
  buf_pool_release(&err);

  test_neomutt_destroy();
}
