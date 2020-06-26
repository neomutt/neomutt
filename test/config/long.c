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
#include "config/common.h"
#include "config/lib.h"
#include "core/lib.h"

static long VarApple;
static long VarBanana;
static long VarCherry;
static long VarDamson;
static long VarElderberry;
static long VarFig;
static long VarGuava;
static long VarHawthorn;
static long VarIlama;
static long VarJackfruit;
static long VarKumquat;
static long VarLemon;
static long VarMango;
static long VarNectarine;
static long VarOlive;

// clang-format off
static struct ConfigDef Vars[] = {
  { "Apple",      DT_LONG,                 &VarApple,      -42, 0, NULL              }, /* test_initial_values */
  { "Banana",     DT_LONG,                 &VarBanana,     99,  0, NULL              },
  { "Cherry",     DT_LONG,                 &VarCherry,     33,  0, NULL              },
  { "Damson",     DT_LONG,                 &VarDamson,     0,   0, NULL              }, /* test_string_set */
  { "Elderberry", DT_LONG|DT_NOT_NEGATIVE, &VarElderberry, 0,   0, NULL              },
  { "Fig",        DT_LONG,                 &VarFig,        0,   0, NULL              }, /* test_string_get */
  { "Guava",      DT_LONG,                 &VarGuava,      0,   0, NULL              }, /* test_native_set */
  { "Hawthorn",   DT_LONG|DT_NOT_NEGATIVE, &VarHawthorn,   0,   0, NULL              },
  { "Ilama",      DT_LONG,                 &VarIlama,      0,   0, NULL              }, /* test_native_get */
  { "Jackfruit",  DT_LONG,                 &VarJackfruit,  99,  0, NULL              }, /* test_reset */
  { "Kumquat",    DT_LONG,                 &VarKumquat,    33,  0, validator_fail    },
  { "Lemon",      DT_LONG,                 &VarLemon,      0,   0, validator_succeed }, /* test_validator */
  { "Mango",      DT_LONG,                 &VarMango,      0,   0, validator_warn    },
  { "Nectarine",  DT_LONG,                 &VarNectarine,  0,   0, validator_fail    },
  { "Olive",      DT_LONG,                 &VarOlive,      0,   0, NULL              }, /* test_inherit */
  { NULL },
};
// clang-format on

static bool test_initial_values(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
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

  struct Buffer value;
  mutt_buffer_init(&value);
  value.dsize = 256;
  value.data = mutt_mem_calloc(1, value.dsize);

  int rc;

  mutt_buffer_reset(&value);
  rc = cs_str_initial_get(cs, "Apple", &value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  if (!TEST_CHECK(mutt_str_equal(value.data, "-42")))
  {
    TEST_MSG("Apple's initial value is wrong: '%s'\n", value.data);
    FREE(&value.data);
    return false;
  }
  TEST_MSG("Apple = %ld\n", VarApple);
  TEST_MSG("Apple's initial value is '%s'\n", value.data);

  mutt_buffer_reset(&value);
  rc = cs_str_initial_get(cs, "Banana", &value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  if (!TEST_CHECK(mutt_str_equal(value.data, "99")))
  {
    TEST_MSG("Banana's initial value is wrong: '%s'\n", value.data);
    FREE(&value.data);
    return false;
  }
  TEST_MSG("Banana = %ld\n", VarBanana);
  TEST_MSG("Banana's initial value is '%s'\n", NONULL(value.data));

  mutt_buffer_reset(&value);
  rc = cs_str_initial_set(cs, "Cherry", "123", &value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  mutt_buffer_reset(&value);
  rc = cs_str_initial_get(cs, "Cherry", &value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  TEST_MSG("Cherry = %ld\n", VarCherry);
  TEST_MSG("Cherry's initial value is %s\n", value.data);

  FREE(&value.data);
  log_line(__func__);
  return true;
}

static bool test_string_set(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  const char *valid[] = { "-123", "0", "-42", "456" };
  int longs[] = { -123, 0, -42, 456 };
  const char *invalid[] = { "-9223372036854775809", "9223372036854775808", "junk", "", NULL };

  const char *name = "Damson";

  int rc;
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    VarDamson = -42;

    TEST_MSG("Setting %s to %s\n", name, valid[i]);
    mutt_buffer_reset(err);
    rc = cs_str_string_set(cs, name, valid[i], err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("%s\n", err->data);
      return false;
    }

    if (rc & CSR_SUC_NO_CHANGE)
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      continue;
    }

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
    mutt_buffer_reset(err);
    rc = cs_str_string_set(cs, name, invalid[i], err);
    if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
    {
      TEST_MSG("Expected error: %s\n", err->data);
    }
    else
    {
      TEST_MSG("%s = %ld, set by '%s'\n", name, VarDamson, NONULL(invalid[i]));
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
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return false;
  }

  log_line(__func__);
  return true;
}

static bool test_string_get(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  const char *name = "Fig";

  VarFig = 123;
  mutt_buffer_reset(err);
  int rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", err->data);
    return false;
  }
  TEST_MSG("%s = %ld, %s\n", name, VarFig, err->data);

  VarFig = -789;
  mutt_buffer_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", err->data);
    return false;
  }
  TEST_MSG("%s = %ld, %s\n", name, VarFig, err->data);

  log_line(__func__);
  return true;
}

static bool test_string_plus_equals(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  const char *valid[] = { "-123", "0", "-42", "456" };
  int numbers[] = { -165, -42, -84, 414 };
  const char *invalid[] = { "-9223372036854775809", "9223372036854775808", "junk", "", NULL };
  const char *name = "Damson";

  int rc;
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    VarDamson = -42;

    TEST_MSG("Increasing %s with initial value %d by %s\n", name, VarDamson, valid[i]);
    mutt_buffer_reset(err);
    rc = cs_str_string_plus_equals(cs, name, valid[i], err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("%s\n", err->data);
      return false;
    }

    if (rc & CSR_SUC_NO_CHANGE)
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      continue;
    }

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
    TEST_MSG("Increasing %s with initial value %d by %s\n", name, VarDamson,
             NONULL(invalid[i]));
    mutt_buffer_reset(err);
    rc = cs_str_string_plus_equals(cs, name, invalid[i], err);
    if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
    {
      TEST_MSG("Expected error: %s\n", err->data);
    }
    else
    {
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
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return false;
  }

  log_line(__func__);
  return true;
}

static bool test_string_minus_equals(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  const char *valid[] = { "-123", "0", "-42", "456" };
  int numbers[] = { 81, -42, 0, -498 };
  const char *invalid[] = { "-9223372036854775809",
                            "9223372036854775808"
                            "junk",
                            "", NULL };
  const char *name = "Damson";

  int rc;
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    VarDamson = -42;

    TEST_MSG("Decreasing %s with initial value %d by %s\n", name, VarDamson, valid[i]);
    mutt_buffer_reset(err);
    rc = cs_str_string_minus_equals(cs, name, valid[i], err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("%s\n", err->data);
      return false;
    }

    if (rc & CSR_SUC_NO_CHANGE)
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      continue;
    }

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
    TEST_MSG("Decreasing %s with initial value %d by %s\n", name, VarDamson,
             NONULL(invalid[i]));
    mutt_buffer_reset(err);
    rc = cs_str_string_minus_equals(cs, name, invalid[i], err);
    if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
    {
      TEST_MSG("Expected error: %s\n", err->data);
    }
    else
    {
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
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return false;
  }

  log_line(__func__);
  return true;
}

static bool test_native_set(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  const char *name = "Guava";
  long value = 12345;

  TEST_MSG("Setting %s to %ld\n", name, value);
  VarGuava = 0;
  mutt_buffer_reset(err);
  int rc = cs_str_native_set(cs, name, value, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

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
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return false;
  }

  log_line(__func__);
  return true;
}

static bool test_native_get(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  const char *name = "Ilama";

  VarIlama = 3456;
  mutt_buffer_reset(err);
  intptr_t value = cs_str_native_get(cs, name, err);
  if (!TEST_CHECK(value != INT_MIN))
  {
    TEST_MSG("Get failed: %s\n", err->data);
    return false;
  }
  TEST_MSG("%s = %ld\n", name, value);

  log_line(__func__);
  return true;
}

static bool test_reset(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  const char *name = "Jackfruit";
  VarJackfruit = 345;
  mutt_buffer_reset(err);

  TEST_MSG("%s = %ld\n", name, VarJackfruit);
  int rc = cs_str_reset(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  if (!TEST_CHECK(VarJackfruit != 345))
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = %ld\n", name, VarJackfruit);

  short_line();
  name = "Kumquat";
  mutt_buffer_reset(err);

  TEST_MSG("Initial: %s = %ld\n", name, VarKumquat);
  dont_fail = true;
  rc = cs_str_string_set(cs, name, "99", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  TEST_MSG("Set: %s = %ld\n", name, VarKumquat);
  dont_fail = false;

  rc = cs_str_reset(cs, name, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  if (!TEST_CHECK(VarKumquat == 99))
  {
    TEST_MSG("Value of %s changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = %ld\n", name, VarKumquat);

  log_line(__func__);
  return true;
}

static bool test_validator(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  const char *name = "Lemon";
  VarLemon = 123;
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, name, "456", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("String: %s = %ld\n", name, VarLemon);
  short_line();

  VarLemon = 456;
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, 123, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("Native: %s = %ld\n", name, VarLemon);
  short_line();

  VarLemon = 456;
  mutt_buffer_reset(err);
  rc = cs_str_string_plus_equals(cs, name, "123", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_CHECK(VarLemon == 579);
  TEST_MSG("PlusEquals: %s = %d\n", name, VarLemon);
  short_line();

  VarLemon = 456;
  mutt_buffer_reset(err);
  rc = cs_str_string_minus_equals(cs, name, "123", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_CHECK(VarLemon == 333);
  TEST_MSG("MinusEquals: %s = %d\n", name, VarLemon);
  short_line();

  name = "Mango";
  VarMango = 123;
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "456", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("String: %s = %ld\n", name, VarMango);
  short_line();

  VarMango = 456;
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, 123, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("Native: %s = %ld\n", name, VarMango);
  short_line();

  name = "Nectarine";
  VarNectarine = 123;
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "456", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("String: %s = %ld\n", name, VarNectarine);
  short_line();

  VarNectarine = 456;
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, 123, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("Native: %s = %ld\n", name, VarNectarine);
  short_line();

  VarNectarine = 456;
  mutt_buffer_reset(err);
  rc = cs_str_string_plus_equals(cs, name, "123", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_CHECK(VarNectarine == 456);
  TEST_MSG("PlusEquals: %s = %d\n", name, VarNectarine);
  short_line();

  VarNectarine = 456;
  mutt_buffer_reset(err);
  rc = cs_str_string_minus_equals(cs, name, "123", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
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
    TEST_MSG("Error: %s\n", err->data);
    goto ti_out;
  }

  // set parent
  VarOlive = 123;
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, parent, "456", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);
  short_line();

  // set child
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, child, "-99", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);
  short_line();

  // reset child
  mutt_buffer_reset(err);
  rc = cs_str_reset(cs, child, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);
  short_line();

  // reset parent
  mutt_buffer_reset(err);
  rc = cs_str_reset(cs, parent, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", err->data);
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
  struct Buffer err;
  mutt_buffer_init(&err);
  err.dsize = 256;
  err.data = mutt_mem_calloc(1, err.dsize);
  mutt_buffer_reset(&err);

  struct ConfigSet *cs = cs_new(30);
  NeoMutt = neomutt_new(cs);

  long_init(cs);
  dont_fail = true;
  if (!cs_register_variables(cs, Vars, 0))
    return;
  dont_fail = false;

  notify_observer_add(NeoMutt->notify, log_observer, 0);

  set_list(cs);

  TEST_CHECK(test_initial_values(cs, &err));
  TEST_CHECK(test_string_set(cs, &err));
  TEST_CHECK(test_string_get(cs, &err));
  TEST_CHECK(test_string_plus_equals(cs, &err));
  TEST_CHECK(test_string_minus_equals(cs, &err));
  TEST_CHECK(test_native_set(cs, &err));
  TEST_CHECK(test_native_get(cs, &err));
  TEST_CHECK(test_reset(cs, &err));
  TEST_CHECK(test_validator(cs, &err));
  TEST_CHECK(test_inherit(cs, &err));

  neomutt_free(&NeoMutt);
  cs_free(&cs);
  FREE(&err.data);
}
