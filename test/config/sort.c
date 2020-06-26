/**
 * @file
 * Test code for the Sort object
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

static short VarApple;
static short VarBanana;
static short VarCherry;
static short VarDamson;
static short VarElderberry;
static short VarFig;
static short VarGuava;
static short VarHawthorn;
static short VarIlama;
static short VarJackfruit;
static short VarKumquat;
static short VarLemon;
static short VarMango;
static short VarNectarine;
static short VarOlive;
static short VarPapaya;
static short VarQuince;
static short VarRaspberry;
static short VarStrawberry;

// clang-format off
static struct ConfigDef Vars[] = {
  { "Apple",      DT_SORT,                 &VarApple,      1,  0, NULL              }, /* test_initial_values */
  { "Banana",     DT_SORT,                 &VarBanana,     2,  0, NULL              },
  { "Cherry",     DT_SORT,                 &VarCherry,     1,  0, NULL              },
  { "Damson",     DT_SORT|DT_SORT_INDEX,   &VarDamson,     1,  0, NULL              }, /* test_string_set */
  { "Elderberry", DT_SORT|DT_SORT_ALIAS,   &VarElderberry, 11, 0, NULL              },
  { "Fig",        DT_SORT|DT_SORT_BROWSER, &VarFig,        1,  0, NULL              },
  { "Guava",      DT_SORT|DT_SORT_KEYS,    &VarGuava,      1,  0, NULL              },
  { "Hawthorn",   DT_SORT|DT_SORT_AUX,     &VarHawthorn,   1,  0, NULL              },
  { "Ilama",      DT_SORT|DT_SORT_SIDEBAR, &VarIlama,      17, 0, NULL              },
  { "Jackfruit",  DT_SORT,                 &VarJackfruit,  1,  0, NULL              }, /* test_string_get */
  { "Kumquat",    DT_SORT,                 &VarKumquat,    1,  0, NULL              }, /* test_native_set */
  { "Lemon",      DT_SORT,                 &VarLemon,      1,  0, NULL              }, /* test_native_get */
  { "Mango",      DT_SORT,                 &VarMango,      1,  0, NULL              }, /* test_reset */
  { "Nectarine",  DT_SORT,                 &VarNectarine,  1,  0, validator_fail    },
  { "Olive",      DT_SORT,                 &VarOlive,      1,  0, validator_succeed }, /* test_validator */
  { "Papaya",     DT_SORT,                 &VarPapaya,     1,  0, validator_warn    },
  { "Quince",     DT_SORT,                 &VarQuince,     1,  0, validator_fail    },
  { "Strawberry", DT_SORT,                 &VarStrawberry, 1,  0, NULL              }, /* test_inherit */
  { NULL },
};

static struct ConfigDef Vars2[] = {
  { "Raspberry", DT_SORT|DT_SORT_AUX|DT_SORT_ALIAS, &VarRaspberry, 1, 0, NULL }, /* test_sort_type */
  { NULL },
};
// clang-format on

const char *name_list[] = {
  "Damson", "Elderberry", "Fig", "Guava", "Hawthorn", "Ilama",
};

short *var_list[] = {
  &VarDamson, &VarElderberry, &VarFig, &VarGuava, &VarHawthorn, &VarIlama,
};

const struct Mapping *sort_maps[] = {
  SortMethods,    SortAliasMethods, SortBrowserMethods,
  SortKeyMethods, SortAuxMethods,   SortSidebarMethods,
};

static bool test_initial_values(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  TEST_MSG("Apple = %d\n", VarApple);
  TEST_MSG("Banana = %d\n", VarBanana);

  if (!TEST_CHECK(VarApple == 1))
  {
    TEST_MSG("Expected: %d\n", 1);
    TEST_MSG("Actual  : %d\n", VarApple);
  }

  if (!TEST_CHECK(VarBanana == 2))
  {
    TEST_MSG("Expected: %d\n", 2);
    TEST_MSG("Actual  : %d\n", VarBanana);
  }

  cs_str_string_set(cs, "Apple", "threads", err);
  cs_str_string_set(cs, "Banana", "score", err);

  struct Buffer value;
  mutt_buffer_init(&value);
  value.dsize = 256;
  value.data = mutt_mem_calloc(1, value.dsize);
  mutt_buffer_reset(&value);

  int rc;

  mutt_buffer_reset(&value);
  rc = cs_str_initial_get(cs, "Apple", &value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  if (!TEST_CHECK(mutt_str_equal(value.data, "date")))
  {
    TEST_MSG("Apple's initial value is wrong: '%s'\n", value.data);
    FREE(&value.data);
    return false;
  }
  TEST_MSG("Apple = %d\n", VarApple);
  TEST_MSG("Apple's initial value is '%s'\n", value.data);

  mutt_buffer_reset(&value);
  rc = cs_str_initial_get(cs, "Banana", &value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  if (!TEST_CHECK(mutt_str_equal(value.data, "size")))
  {
    TEST_MSG("Banana's initial value is wrong: '%s'\n", value.data);
    FREE(&value.data);
    return false;
  }
  TEST_MSG("Banana = %d\n", VarBanana);
  TEST_MSG("Banana's initial value is '%s'\n", NONULL(value.data));

  mutt_buffer_reset(&value);
  rc = cs_str_initial_set(cs, "Cherry", "size", &value);
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

  TEST_MSG("Cherry = %s\n", mutt_map_get_name(VarCherry, SortMethods));
  TEST_MSG("Cherry's initial value is %s\n", value.data);

  FREE(&value.data);
  log_line(__func__);
  return true;
}

static bool test_string_set(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  for (unsigned int i = 0; i < mutt_array_size(var_list); i++)
  {
    short *var = var_list[i];

    *var = -1;

    const struct Mapping *map = sort_maps[i];

    int rc;
    for (int j = 0; map[j].name; j++)
    {
      mutt_buffer_reset(err);
      rc = cs_str_string_set(cs, name_list[i], map[j].name, err);
      if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
      {
        TEST_MSG("%s\n", err->data);
        return false;
      }

      if (rc & CSR_SUC_NO_CHANGE)
      {
        TEST_MSG("Value of %s wasn't changed\n", map[j].name);
        continue;
      }

      if (!TEST_CHECK(*var == map[j].value))
      {
        TEST_MSG("Value of %s wasn't changed\n", map[j].name);
        return false;
      }
      TEST_MSG("%s = %d, set by '%s'\n", name_list[i], *var, map[j].name);
    }

    const char *invalid[] = {
      "-1",
      "999",
      "junk",
      NULL,
    };

    for (unsigned int j = 0; j < mutt_array_size(invalid); j++)
    {
      mutt_buffer_reset(err);
      rc = cs_str_string_set(cs, name_list[i], invalid[j], err);
      if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
      {
        TEST_MSG("Expected error: %s\n", err->data);
      }
      else
      {
        TEST_MSG("%s = %d, set by '%s'\n", name_list[i], *var, invalid[j]);
        TEST_MSG("This test should have failed\n");
        return false;
      }
    }
  }

  const char *name = "Damson";
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, name, "last-date-sent", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  if (!TEST_CHECK(VarDamson == (SORT_DATE | SORT_LAST)))
  {
    TEST_MSG("Expected %d, got %d\n", (SORT_DATE | SORT_LAST), VarDamson);
    return false;
  }

  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "reverse-score", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  if (!TEST_CHECK(VarDamson == (SORT_SCORE | SORT_REVERSE)))
  {
    TEST_MSG("Expected %d, got %d\n", (SORT_DATE | SORT_LAST), VarDamson);
    return false;
  }

  log_line(__func__);
  return true;
}

static bool test_string_get(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  const char *name = "Jackfruit";

  VarJackfruit = SORT_SUBJECT;
  mutt_buffer_reset(err);
  int rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", err->data);
    return false;
  }
  TEST_MSG("%s = %d, %s\n", name, VarJackfruit, err->data);

  VarJackfruit = SORT_THREADS;
  mutt_buffer_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", err->data);
    return false;
  }
  TEST_MSG("%s = %d, %s\n", name, VarJackfruit, err->data);

  VarJackfruit = -1;
  mutt_buffer_reset(err);
  TEST_MSG("Expect error for next test\n");
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  mutt_buffer_reset(err);
  name = "Raspberry";
  TEST_MSG("Expect error for next test\n");
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  log_line(__func__);
  return true;
}

static bool test_native_set(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  int rc;
  for (unsigned int i = 0; i < mutt_array_size(var_list); i++)
  {
    short *var = var_list[i];

    *var = -1;

    const struct Mapping *map = sort_maps[i];

    for (int j = 0; map[j].name; j++)
    {
      mutt_buffer_reset(err);
      rc = cs_str_native_set(cs, name_list[i], map[j].value, err);
      if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
      {
        TEST_MSG("%s\n", err->data);
        return false;
      }

      if (rc & CSR_SUC_NO_CHANGE)
      {
        TEST_MSG("Value of %s wasn't changed\n", map[j].name);
        continue;
      }

      if (!TEST_CHECK(*var == map[j].value))
      {
        TEST_MSG("Value of %s wasn't changed\n", map[j].name);
        return false;
      }
      TEST_MSG("%s = %d, set by '%s'\n", name_list[i], *var, map[j].name);
    }
  }

  const char *name = "Kumquat";
  short value = SORT_THREADS;
  VarKumquat = -1;
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, value, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  if (!TEST_CHECK(VarKumquat == value))
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    return false;
  }

  TEST_MSG("%s = %d, set to '%d'\n", name, VarKumquat, value);

  int invalid[] = { -1, 999 };
  for (unsigned int i = 0; i < mutt_array_size(invalid); i++)
  {
    VarKumquat = -1;
    mutt_buffer_reset(err);
    rc = cs_str_native_set(cs, name, invalid[i], err);
    if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
    {
      TEST_MSG("Expected error: %s\n", err->data);
    }
    else
    {
      TEST_MSG("%s = %d, set by '%d'\n", name, VarKumquat, invalid[i]);
      TEST_MSG("This test should have failed\n");
      return false;
    }
  }

  name = "Damson";
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, (SORT_DATE | SORT_LAST), err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  if (!TEST_CHECK(VarDamson == (SORT_DATE | SORT_LAST)))
  {
    TEST_MSG("Expected %d, got %d\n", (SORT_DATE | SORT_LAST), VarDamson);
    return false;
  }

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, (SORT_SCORE | SORT_REVERSE), err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  if (!TEST_CHECK(VarDamson == (SORT_SCORE | SORT_REVERSE)))
  {
    TEST_MSG("Expected %d, got %d\n", (SORT_DATE | SORT_LAST), VarDamson);
    return false;
  }

  log_line(__func__);
  return true;
}

static bool test_native_get(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  const char *name = "Lemon";

  VarLemon = SORT_THREADS;
  mutt_buffer_reset(err);
  intptr_t value = cs_str_native_get(cs, name, err);
  if (!TEST_CHECK(value == SORT_THREADS))
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

  const char *name = "Mango";
  VarMango = SORT_SUBJECT;
  mutt_buffer_reset(err);

  int rc = cs_str_reset(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  if (VarMango == SORT_SUBJECT)
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = %d\n", name, VarMango);

  rc = cs_str_reset(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  name = "Nectarine";
  mutt_buffer_reset(err);

  TEST_MSG("Initial: %s = %d\n", name, VarNectarine);
  dont_fail = true;
  rc = cs_str_string_set(cs, name, "size", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  TEST_MSG("Set: %s = %d\n", name, VarNectarine);
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

  if (!TEST_CHECK(VarNectarine == SORT_SIZE))
  {
    TEST_MSG("Value of %s changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = %d\n", name, VarNectarine);

  log_line(__func__);
  return true;
}

static bool test_validator(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  const char *name = "Olive";
  VarOlive = SORT_SUBJECT;
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, name, "threads", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("String: %s = %d\n", name, VarOlive);

  VarOlive = SORT_SUBJECT;
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, SORT_THREADS, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("Native: %s = %d\n", name, VarOlive);

  name = "Papaya";
  VarPapaya = SORT_SUBJECT;
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "threads", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("String: %s = %d\n", name, VarPapaya);

  VarPapaya = SORT_SUBJECT;
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, SORT_THREADS, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("Native: %s = %d\n", name, VarPapaya);

  name = "Quince";
  VarQuince = SORT_SUBJECT;
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "threads", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("String: %s = %d\n", name, VarQuince);

  VarQuince = SORT_SUBJECT;
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, SORT_THREADS, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("Native: %s = %d\n", name, VarQuince);

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
  const char *parent = "Strawberry";
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
  VarStrawberry = SORT_SUBJECT;
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, parent, "threads", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // set child
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, child, "score", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // reset child
  mutt_buffer_reset(err);
  rc = cs_str_reset(cs, child, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);

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

static bool test_sort_type(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  const char *name = "Raspberry";
  const char *value = "alpha";

  mutt_buffer_reset(err);
  TEST_MSG("Expect error for next test\n");
  int rc = cs_str_string_set(cs, name, value, err);
  if (!TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("%s = %d, set by '%s'\n", name, VarRaspberry, value);
    TEST_MSG("This test should have failed\n");
    return false;
  }

  mutt_buffer_reset(err);
  TEST_MSG("Expect error for next test\n");
  rc = cs_str_native_set(cs, name, SORT_THREADS, err);
  if (!TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("%s = %d, set by %d\n", name, VarRaspberry, SORT_THREADS);
    TEST_MSG("This test should have failed\n");
    return false;
  }

  log_line(__func__);
  return true;
}

void test_config_sort(void)
{
  struct Buffer err;
  mutt_buffer_init(&err);
  err.dsize = 256;
  err.data = mutt_mem_calloc(1, err.dsize);
  mutt_buffer_reset(&err);

  struct ConfigSet *cs = cs_new(30);
  NeoMutt = neomutt_new(cs);

  sort_init(cs);
  dont_fail = true;
  if (!cs_register_variables(cs, Vars, 0))
    return;
  dont_fail = false;

  notify_observer_add(NeoMutt->notify, log_observer, 0);

  set_list(cs);

  /* Register a broken variable separately */
  if (!cs_register_variables(cs, Vars2, 0))
    return;

  TEST_CHECK(test_initial_values(cs, &err));
  TEST_CHECK(test_string_set(cs, &err));
  TEST_CHECK(test_string_get(cs, &err));
  TEST_CHECK(test_native_set(cs, &err));
  TEST_CHECK(test_native_get(cs, &err));
  TEST_CHECK(test_reset(cs, &err));
  TEST_CHECK(test_validator(cs, &err));
  TEST_CHECK(test_inherit(cs, &err));
  TEST_CHECK(test_sort_type(cs, &err));

  neomutt_free(&NeoMutt);
  cs_free(&cs);
  FREE(&err.data);
}
