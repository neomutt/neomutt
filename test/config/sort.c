/**
 * @file
 * Test code for the Sort object
 *
 * @authors
 * Copyright (C) 2018-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 Aditya De Saha <adityadesaha@gmail.com>
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
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
/**
 * Test Lookup Table, used by everything
 */
static const struct Mapping SortTestMethods[] = {
  { "date",          SORT_DATE },
  { "date-sent",     SORT_DATE },
  { "date-received", SORT_RECEIVED },
  { "from",          SORT_FROM },
  { "label",         SORT_LABEL },
  { "unsorted",      SORT_ORDER },
  { "mailbox-order", SORT_ORDER },
  { "score",         SORT_SCORE },
  { "size",          SORT_SIZE },
  { "spam",          SORT_SPAM },
  { "subject",       SORT_SUBJECT },
  { "threads",       SORT_THREADS },
  { "to",            SORT_TO },
  { NULL,            0 },
};

static struct ConfigDef Vars[] = {
  { "Apple",      DT_SORT,                              1,  IP SortTestMethods, NULL,              }, /* test_initial_values */
  { "Banana",     DT_SORT,                              2,  IP SortTestMethods, NULL,              },
  { "Cherry",     DT_SORT,                              1,  IP SortTestMethods, NULL,              },
  { "Damson",     DT_SORT|D_SORT_REVERSE|D_SORT_LAST,   1,  IP SortTestMethods, NULL,              }, /* test_string_set */
  { "Elderberry", DT_SORT,                              11, IP SortTestMethods, NULL,              },
  { "Fig",        DT_SORT,                              1,  IP SortTestMethods, NULL,              },
  { "Guava",      DT_SORT,                              1,  IP SortTestMethods, NULL,              },
  { "Hawthorn",   DT_SORT,                              1,  IP SortTestMethods, NULL,              },
  { "Ilama",      DT_SORT,                              17, IP SortTestMethods, NULL,              },
  { "Jackfruit",  DT_SORT,                              1,  IP SortTestMethods, NULL,              }, /* test_string_get */
  { "Kumquat",    DT_SORT,                              1,  IP SortTestMethods, NULL,              }, /* test_native_set */
  { "Lemon",      DT_SORT,                              1,  IP SortTestMethods, NULL,              }, /* test_native_get */
  { "Mango",      DT_SORT,                              1,  IP SortTestMethods, NULL,              }, /* test_reset */
  { "Nectarine",  DT_SORT,                              1,  IP SortTestMethods, validator_fail,    },
  { "Olive",      DT_SORT,                              1,  IP SortTestMethods, validator_succeed, }, /* test_validator */
  { "Papaya",     DT_SORT,                              1,  IP SortTestMethods, validator_warn,    },
  { "Quince",     DT_SORT,                              1,  IP SortTestMethods, validator_fail,    },
  { "Strawberry", DT_SORT,                              1,  IP SortTestMethods, NULL,              }, /* test_inherit */
  { "Tangerine",  DT_SORT | D_ON_STARTUP,               1,  IP SortTestMethods, NULL,              }, /* startup */
  { NULL },
};

static struct ConfigDef Vars2[] = {
  { "Raspberry", DT_SORT, 1, 0, NULL, }, /* test_sort_type */
  { NULL },
};
// clang-format on

static const char *name_list[] = {
  "Damson", "Elderberry", "Fig", "Guava", "Hawthorn", "Ilama",
};

static bool test_initial_values(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  short VarApple = cs_subset_sort(sub, "Apple");
  short VarBanana = cs_subset_sort(sub, "Banana");

  TEST_MSG("Apple = %d", VarApple);
  TEST_MSG("Banana = %d", VarBanana);

  if (!TEST_CHECK(VarApple == 1))
  {
    TEST_MSG("Expected: %d", 1);
    TEST_MSG("Actual  : %d", VarApple);
  }

  if (!TEST_CHECK(VarBanana == 2))
  {
    TEST_MSG("Expected: %d", 2);
    TEST_MSG("Actual  : %d", VarBanana);
  }

  cs_str_string_set(cs, "Apple", "threads", err);
  cs_str_string_set(cs, "Banana", "score", err);

  VarApple = cs_subset_sort(sub, "Apple");
  VarBanana = cs_subset_sort(sub, "Banana");

  struct Buffer *value = buf_pool_get();

  int rc;

  buf_reset(value);
  rc = cs_str_initial_get(cs, "Apple", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(value));
    return false;
  }

  if (!TEST_CHECK_STR_EQ(buf_string(value), "date"))
  {
    TEST_MSG("Apple's initial value is wrong: '%s'", buf_string(value));
    return false;
  }
  VarApple = cs_subset_sort(sub, "Apple");
  TEST_MSG("Apple = %d", VarApple);
  TEST_MSG("Apple's initial value is '%s'", buf_string(value));

  buf_reset(value);
  rc = cs_str_initial_get(cs, "Banana", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(value));
    return false;
  }

  if (!TEST_CHECK_STR_EQ(buf_string(value), "size"))
  {
    TEST_MSG("Banana's initial value is wrong: '%s'", buf_string(value));
    return false;
  }
  VarBanana = cs_subset_sort(sub, "Banana");
  TEST_MSG("Banana = %d", VarBanana);
  TEST_MSG("Banana's initial value is '%s'", NONULL(buf_string(value)));

  buf_reset(value);
  rc = cs_str_initial_set(cs, "Cherry", "size", value);
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

  short VarCherry = cs_subset_sort(sub, "Cherry");
  TEST_MSG("Cherry = %s", mutt_map_get_name(VarCherry, SortTestMethods));
  TEST_MSG("Cherry's initial value is %s", buf_string(value));

  buf_pool_release(&value);
  log_line(__func__);
  return true;
}

static bool test_string_set(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const struct Mapping *map = SortTestMethods;
  for (unsigned int i = 0; i < mutt_array_size(name_list); i++)
  {
    cs_str_native_set(cs, name_list[i], -1, NULL);

    int rc;
    for (int j = 0; map[j].name; j++)
    {
      buf_reset(err);
      rc = cs_str_string_set(cs, name_list[i], map[j].name, err);
      if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
      {
        TEST_MSG("%s", buf_string(err));
        return false;
      }

      if (rc & CSR_SUC_NO_CHANGE)
      {
        TEST_MSG("Value of %s wasn't changed", map[j].name);
        continue;
      }

      short VarTest = cs_subset_sort(sub, name_list[i]);
      if (!TEST_CHECK(VarTest == map[j].value))
      {
        TEST_MSG("Value of %s wasn't changed", map[j].name);
        return false;
      }
      TEST_MSG("%s = %d, set by '%s'", name_list[i], VarTest, map[j].name);
    }

    const char *invalid[] = {
      "-1",
      "999",
      "junk",
      NULL,
    };

    for (unsigned int j = 0; j < mutt_array_size(invalid); j++)
    {
      buf_reset(err);
      rc = cs_str_string_set(cs, name_list[i], invalid[j], err);
      if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
      {
        TEST_MSG("Expected error: %s", buf_string(err));
      }
      else
      {
        short VarTest = cs_subset_sort(sub, map[j].name);
        TEST_MSG("%s = %d, set by '%s'", name_list[i], VarTest, invalid[j]);
        TEST_MSG("This test should have failed");
        return false;
      }
    }
  }

  const char *name = "Damson";
  buf_reset(err);
  int rc = cs_str_string_set(cs, name, "last-date-sent", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }

  short VarDamson = cs_subset_sort(sub, "Damson");
  if (!TEST_CHECK(VarDamson == (SORT_DATE | SORT_LAST)))
  {
    TEST_MSG("Expected %d, got %d", (SORT_DATE | SORT_LAST), VarDamson);
    return false;
  }

  buf_reset(err);
  rc = cs_str_string_set(cs, name, "reverse-score", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }

  VarDamson = cs_subset_sort(sub, "Damson");
  if (!TEST_CHECK(VarDamson == (SORT_SCORE | SORT_REVERSE)))
  {
    TEST_MSG("Expected %d, got %d", (SORT_DATE | SORT_LAST), VarDamson);
    return false;
  }

  name = "Tangerine";
  rc = cs_str_string_set(cs, name, "date", err);
  TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS);

  rc = cs_str_string_set(cs, name, "size", err);
  TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS);

  log_line(__func__);
  return true;
}

static bool test_string_get(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;
  const char *name = "Jackfruit";

  cs_str_native_set(cs, name, SORT_SUBJECT, NULL);
  buf_reset(err);
  int rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s", buf_string(err));
    return false;
  }
  short VarJackfruit = cs_subset_sort(sub, "Jackfruit");
  TEST_MSG("%s = %d, %s", name, VarJackfruit, buf_string(err));

  cs_str_native_set(cs, name, SORT_THREADS, NULL);
  buf_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s", buf_string(err));
    return false;
  }
  VarJackfruit = cs_subset_sort(sub, "Jackfruit");
  TEST_MSG("%s = %d, %s", name, VarJackfruit, buf_string(err));

  // cs_str_native_set(cs, name, -1, NULL);
  // buf_reset(err);
  // TEST_MSG("Expect error for next test");
  // rc = cs_str_string_get(cs, name, err);
  // if (!TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  // {
  //   TEST_MSG("%s", buf_string(err));
  //   return false;
  // }

  buf_reset(err);
  name = "Raspberry";
  TEST_MSG("Expect error for next test");
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }

  // Test prefixes
  name = "Damson";
  cs_str_native_set(cs, name, SORT_DATE | SORT_REVERSE | SORT_LAST, NULL);
  buf_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s", buf_string(err));
    return false;
  }
  short VarDamson = cs_subset_sort(sub, "Damson");
  TEST_MSG("%s = %d, %s", name, VarDamson, buf_string(err));

  log_line(__func__);
  return true;
}

static bool test_native_set(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  int rc;
  const struct Mapping *map = SortTestMethods;
  for (unsigned int i = 0; i < mutt_array_size(name_list); i++)
  {
    cs_str_native_set(cs, name_list[i], -1, NULL);

    for (int j = 0; map[j].name; j++)
    {
      buf_reset(err);
      rc = cs_str_native_set(cs, name_list[i], map[j].value, err);
      if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
      {
        TEST_MSG("%s", buf_string(err));
        return false;
      }

      if (rc & CSR_SUC_NO_CHANGE)
      {
        TEST_MSG("Value of %s wasn't changed", map[j].name);
        continue;
      }

      short VarTest = cs_subset_sort(sub, name_list[i]);
      if (!TEST_CHECK(VarTest == map[j].value))
      {
        TEST_MSG("Value of %s wasn't changed", map[j].name);
        return false;
      }
      TEST_MSG("%s = %d, set by '%s'", name_list[i], VarTest, map[j].name);
    }
  }

  const char *name = "Kumquat";
  short value = SORT_THREADS;
  cs_str_native_set(cs, name, -1, NULL);
  buf_reset(err);
  rc = cs_str_native_set(cs, name, value, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }

  short VarKumquat = cs_subset_sort(sub, "Kumquat");
  if (!TEST_CHECK(VarKumquat == value))
  {
    TEST_MSG("Value of %s wasn't changed", name);
    return false;
  }

  TEST_MSG("%s = %d, set to '%d'", name, VarKumquat, value);

  int invalid[] = { -1, 999 };
  for (unsigned int i = 0; i < mutt_array_size(invalid); i++)
  {
    cs_str_native_set(cs, name, -1, NULL);
    buf_reset(err);
    rc = cs_str_native_set(cs, name, invalid[i], err);
    if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
    {
      TEST_MSG("Expected error: %s", buf_string(err));
    }
    else
    {
      VarKumquat = cs_subset_sort(sub, "Kumquat");
      TEST_MSG("%s = %d, set by '%d'", name, VarKumquat, invalid[i]);
      TEST_MSG("This test should have failed");
      return false;
    }
  }

  name = "Damson";
  buf_reset(err);
  rc = cs_str_native_set(cs, name, (SORT_DATE | SORT_LAST), err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }

  short VarDamson = cs_subset_sort(sub, "Damson");
  if (!TEST_CHECK(VarDamson == (SORT_DATE | SORT_LAST)))
  {
    TEST_MSG("Expected %d, got %d", (SORT_DATE | SORT_LAST), VarDamson);
    return false;
  }

  buf_reset(err);
  rc = cs_str_native_set(cs, name, (SORT_SCORE | SORT_REVERSE), err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }

  VarDamson = cs_subset_sort(sub, "Damson");
  if (!TEST_CHECK(VarDamson == (SORT_SCORE | SORT_REVERSE)))
  {
    TEST_MSG("Expected %d, got %d", (SORT_DATE | SORT_LAST), VarDamson);
    return false;
  }

  name = "Tangerine";
  rc = cs_str_native_set(cs, name, SORT_DATE, err);
  TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS);

  rc = cs_str_native_set(cs, name, SORT_SIZE, err);
  TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS);

  log_line(__func__);
  return true;
}

static bool test_native_get(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;
  const char *name = "Lemon";

  cs_str_native_set(cs, name, SORT_THREADS, NULL);
  buf_reset(err);
  intptr_t value = cs_str_native_get(cs, name, err);
  if (!TEST_CHECK(value == SORT_THREADS))
  {
    TEST_MSG("Get failed: %s", buf_string(err));
    return false;
  }
  TEST_MSG("%s = %ld", name, value);

  log_line(__func__);
  return true;
}

static bool test_reset(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *name = "Mango";
  cs_str_native_set(cs, name, SORT_SUBJECT, NULL);
  buf_reset(err);

  int rc = cs_str_reset(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }

  short VarMango = cs_subset_sort(sub, "Mango");
  if (VarMango == SORT_SUBJECT)
  {
    TEST_MSG("Value of %s wasn't changed", name);
    return false;
  }

  TEST_MSG("Reset: %s = %d", name, VarMango);

  rc = cs_str_reset(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }

  name = "Nectarine";
  buf_reset(err);

  short VarNectarine = cs_subset_sort(sub, "Nectarine");
  TEST_MSG("Initial: %s = %d", name, VarNectarine);
  dont_fail = true;
  rc = cs_str_string_set(cs, name, "size", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  VarNectarine = cs_subset_sort(sub, "Nectarine");
  TEST_MSG("Set: %s = %d", name, VarNectarine);
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

  VarNectarine = cs_subset_sort(sub, "Nectarine");
  if (!TEST_CHECK(VarNectarine == SORT_SIZE))
  {
    TEST_MSG("Value of %s changed", name);
    return false;
  }

  TEST_MSG("Reset: %s = %d", name, VarNectarine);

  name = "Tangerine";
  rc = cs_str_reset(cs, name, err);
  TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS);

  StartupComplete = false;
  rc = cs_str_native_set(cs, name, SORT_SIZE, err);
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

  const char *name = "Olive";
  cs_str_native_set(cs, name, SORT_SUBJECT, NULL);
  buf_reset(err);
  int rc = cs_str_string_set(cs, name, "threads", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }
  short VarOlive = cs_subset_sort(sub, "Olive");
  TEST_MSG("String: %s = %d", name, VarOlive);

  cs_str_native_set(cs, name, SORT_SUBJECT, NULL);
  buf_reset(err);
  rc = cs_str_native_set(cs, name, SORT_THREADS, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }
  VarOlive = cs_subset_sort(sub, "Olive");
  TEST_MSG("Native: %s = %d", name, VarOlive);

  name = "Papaya";
  cs_str_native_set(cs, name, SORT_SUBJECT, NULL);
  buf_reset(err);
  rc = cs_str_string_set(cs, name, "threads", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }
  short VarPapaya = cs_subset_sort(sub, "Papaya");
  TEST_MSG("String: %s = %d", name, VarPapaya);

  cs_str_native_set(cs, name, SORT_SUBJECT, NULL);
  buf_reset(err);
  rc = cs_str_native_set(cs, name, SORT_THREADS, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }
  VarPapaya = cs_subset_sort(sub, "Papaya");
  TEST_MSG("Native: %s = %d", name, VarPapaya);

  name = "Quince";
  cs_str_native_set(cs, name, SORT_SUBJECT, NULL);
  buf_reset(err);
  rc = cs_str_string_set(cs, name, "threads", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }
  short VarQuince = cs_subset_sort(sub, "Quince");
  TEST_MSG("String: %s = %d", name, VarQuince);

  cs_str_native_set(cs, name, SORT_SUBJECT, NULL);
  buf_reset(err);
  rc = cs_str_native_set(cs, name, SORT_THREADS, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }
  VarQuince = cs_subset_sort(sub, "Quince");
  TEST_MSG("Native: %s = %d", name, VarQuince);

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
  cs_str_native_set(cs, parent, SORT_SUBJECT, NULL);
  buf_reset(err);
  int rc = cs_str_string_set(cs, parent, "threads", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s", buf_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // set child
  buf_reset(err);
  rc = cs_str_string_set(cs, child, "score", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s", buf_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // reset child
  buf_reset(err);
  rc = cs_str_reset(cs, child, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s", buf_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // reset parent
  buf_reset(err);
  rc = cs_str_reset(cs, parent, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
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

static bool test_sort_type(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *name = "Raspberry";
  const char *value = "alpha";

  buf_reset(err);
  TEST_MSG("Expect error for next test");
  int rc = cs_str_string_set(cs, name, value, err);
  if (!TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    short VarRaspberry = cs_subset_sort(sub, "Raspberry");
    TEST_MSG("%s = %d, set by '%s'", name, VarRaspberry, value);
    TEST_MSG("This test should have failed");
    return false;
  }

  buf_reset(err);
  TEST_MSG("Expect error for next test");
  rc = cs_str_native_set(cs, name, SORT_THREADS, err);
  if (!TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    short VarRaspberry = cs_subset_sort(sub, "Raspberry");
    TEST_MSG("%s = %d, set by %d", name, VarRaspberry, SORT_THREADS);
    TEST_MSG("This test should have failed");
    return false;
  }

  log_line(__func__);
  return true;
}

void test_config_sort(void)
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

  /* Register a broken variable separately */
  if (!TEST_CHECK(cs_register_variables(cs, Vars2)))
    return;

  struct Buffer *err = buf_pool_get();
  TEST_CHECK(test_initial_values(sub, err));
  TEST_CHECK(test_string_set(sub, err));
  TEST_CHECK(test_string_get(sub, err));
  TEST_CHECK(test_native_set(sub, err));
  TEST_CHECK(test_native_get(sub, err));
  TEST_CHECK(test_reset(sub, err));
  TEST_CHECK(test_validator(sub, err));
  TEST_CHECK(test_inherit(cs, err));
  TEST_CHECK(test_sort_type(sub, err));
  buf_pool_release(&err);
}
