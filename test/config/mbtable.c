/**
 * @file
 * Test code for the Mbtable object
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
#include "config/lib.h"
#include "core/lib.h"
#include "common.h" // IWYU pragma: keep
#include "test_common.h"

// clang-format off
static struct ConfigDef Vars[] = {
  { "Apple",      DT_MBTABLE, IP "apple",      0, NULL,              }, /* test_initial_values */
  { "Banana",     DT_MBTABLE, IP "banana",     0, NULL,              },
  { "Cherry",     DT_MBTABLE, IP "cherry",     0, NULL,              },
  { "Damson",     DT_MBTABLE, 0,               0, NULL,              }, /* test_mbtable_set */
  { "Elderberry", DT_MBTABLE, IP "elderberry", 0, NULL,              },
  { "Fig",        DT_MBTABLE, 0,               0, NULL,              }, /* test_mbtable_get */
  { "Guava",      DT_MBTABLE, IP "guava",      0, NULL,              },
  { "Hawthorn",   DT_MBTABLE, 0,               0, NULL,              },
  { "Ilama",      DT_MBTABLE, 0,               0, NULL,              }, /* test_native_set */
  { "Jackfruit",  DT_MBTABLE, IP "jackfruit",  0, NULL,              },
  { "Kumquat",    DT_MBTABLE, 0,               0, NULL,              }, /* test_native_get */
  { "Lemon",      DT_MBTABLE, IP "lemon",      0, NULL,              }, /* test_reset */
  { "Mango",      DT_MBTABLE, IP "mango",      0, validator_fail,    },
  { "Nectarine",  DT_MBTABLE, IP "nectarine",  0, validator_succeed, }, /* test_validator */
  { "Olive",      DT_MBTABLE, IP "olive",      0, validator_warn,    },
  { "Papaya",     DT_MBTABLE, IP "papaya",     0, validator_fail,    },
  { "Quince",     DT_MBTABLE, 0,               0, NULL,              }, /* test_inherit */
  { NULL },
};
// clang-format on

static bool test_initial_values(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  struct MbTable *VarApple = cs_subset_mbtable(sub, "Apple");
  struct MbTable *VarBanana = cs_subset_mbtable(sub, "Banana");

  TEST_MSG("Apple = %s\n", VarApple->orig_str);
  TEST_MSG("Banana = %s\n", VarBanana->orig_str);

  if (!TEST_CHECK(mutt_str_equal(VarApple->orig_str, "apple")))
  {
    TEST_MSG("Error: initial values were wrong\n");
    return false;
  }

  if (!TEST_CHECK(mutt_str_equal(VarBanana->orig_str, "banana")))
  {
    TEST_MSG("Error: initial values were wrong\n");
    return false;
  }

  cs_str_string_set(cs, "Apple", "car", err);
  cs_str_string_set(cs, "Banana", "train", err);

  VarApple = cs_subset_mbtable(sub, "Apple");
  VarBanana = cs_subset_mbtable(sub, "Banana");

  struct Buffer *value = mutt_buffer_pool_get();

  int rc;

  mutt_buffer_reset(value);
  rc = cs_str_initial_get(cs, "Apple", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(value));
    return false;
  }

  if (!TEST_CHECK(mutt_str_equal(mutt_buffer_string(value), "apple")))
  {
    TEST_MSG("Apple's initial value is wrong: '%s'\n", mutt_buffer_string(value));
    return false;
  }
  VarApple = cs_subset_mbtable(sub, "Apple");
  TEST_MSG("Apple = '%s'\n", VarApple ? VarApple->orig_str : "");
  TEST_MSG("Apple's initial value is %s\n", mutt_buffer_string(value));

  mutt_buffer_reset(value);
  rc = cs_str_initial_get(cs, "Banana", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(value));
    return false;
  }

  if (!TEST_CHECK(mutt_str_equal(mutt_buffer_string(value), "banana")))
  {
    TEST_MSG("Banana's initial value is wrong: %s\n", mutt_buffer_string(value));
    return false;
  }
  VarBanana = cs_subset_mbtable(sub, "Banana");
  TEST_MSG("Banana = '%s'\n", VarBanana ? VarBanana->orig_str : "");
  TEST_MSG("Banana's initial value is %s\n", NONULL(mutt_buffer_string(value)));

  mutt_buffer_reset(value);
  rc = cs_str_initial_set(cs, "Cherry", "config.*", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(value));
    return false;
  }

  mutt_buffer_reset(value);
  rc = cs_str_initial_set(cs, "Cherry", "file.*", value);
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

  struct MbTable *VarCherry = cs_subset_mbtable(sub, "Cherry");
  TEST_MSG("Cherry = '%s'\n", VarCherry->orig_str);
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
  char *mb = NULL;

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

    struct MbTable *VarDamson = cs_subset_mbtable(sub, "Damson");
    mb = VarDamson ? VarDamson->orig_str : NULL;
    if (!TEST_CHECK(mutt_str_equal(mb, valid[i])))
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      return false;
    }
    TEST_MSG("%s = '%s', set by '%s'\n", name, NONULL(mb), NONULL(valid[i]));
  }

  name = "Elderberry";
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

    struct MbTable *VarElderberry = cs_subset_mbtable(sub, "Elderberry");
    const char *orig_str = VarElderberry ? VarElderberry->orig_str : NULL;
    if (!TEST_CHECK(mutt_str_equal(orig_str, valid[i])))
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      return false;
    }
    TEST_MSG("%s = '%s', set by '%s'\n", name, NONULL(orig_str), NONULL(valid[i]));
  }

  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "\377", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
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
  char *mb = NULL;

  mutt_buffer_reset(err);
  int rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", mutt_buffer_string(err));
    return false;
  }
  struct MbTable *VarFig = cs_subset_mbtable(sub, "Fig");
  mb = VarFig ? VarFig->orig_str : NULL;
  TEST_MSG("%s = '%s', '%s'\n", name, NONULL(mb), mutt_buffer_string(err));

  name = "Guava";
  mutt_buffer_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", mutt_buffer_string(err));
    return false;
  }
  struct MbTable *VarGuava = cs_subset_mbtable(sub, "Guava");
  mb = VarGuava ? VarGuava->orig_str : NULL;
  TEST_MSG("%s = '%s', '%s'\n", name, NONULL(mb), mutt_buffer_string(err));

  name = "Hawthorn";
  rc = cs_str_string_set(cs, name, "hawthorn", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;

  mutt_buffer_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", mutt_buffer_string(err));
    return false;
  }
  struct MbTable *VarHawthorn = cs_subset_mbtable(sub, "Hawthorn");
  mb = VarHawthorn ? VarHawthorn->orig_str : NULL;
  TEST_MSG("%s = '%s', '%s'\n", name, NONULL(mb), mutt_buffer_string(err));

  log_line(__func__);
  return true;
}

static bool test_native_set(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  struct MbTable *t = mbtable_parse("hello");
  const char *name = "Ilama";
  char *mb = NULL;
  bool result = false;

  mutt_buffer_reset(err);
  int rc = cs_str_native_set(cs, name, (intptr_t) t, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    goto tns_out;
  }

  struct MbTable *VarIlama = cs_subset_mbtable(sub, "Ilama");
  mb = VarIlama ? VarIlama->orig_str : NULL;
  if (!TEST_CHECK(mutt_str_equal(mb, t->orig_str)))
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    goto tns_out;
  }
  TEST_MSG("%s = '%s', set by '%s'\n", name, NONULL(mb), t->orig_str);

  name = "Jackfruit";
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, 0, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    goto tns_out;
  }

  struct MbTable *VarJackfruit = cs_subset_mbtable(sub, "Jackfruit");
  if (!TEST_CHECK(VarJackfruit == NULL))
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    goto tns_out;
  }
  mb = VarJackfruit ? VarJackfruit->orig_str : NULL;
  TEST_MSG("%s = '%s', set by NULL\n", name, NONULL(mb));

  result = true;
tns_out:
  mbtable_free(&t);
  log_line(__func__);
  return result;
}

static bool test_native_get(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;
  const char *name = "Kumquat";

  int rc = cs_str_string_set(cs, name, "kumquat", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;

  mutt_buffer_reset(err);
  intptr_t value = cs_str_native_get(cs, name, err);
  struct MbTable *t = (struct MbTable *) value;

  struct MbTable *VarKumquat = cs_subset_mbtable(sub, "Kumquat");
  if (!TEST_CHECK(VarKumquat == t))
  {
    TEST_MSG("Get failed: %s\n", mutt_buffer_string(err));
    return false;
  }
  char *mb1 = VarKumquat ? VarKumquat->orig_str : NULL;
  char *mb2 = t ? t->orig_str : NULL;
  TEST_MSG("%s = '%s', '%s'\n", name, NONULL(mb1), NONULL(mb2));

  log_line(__func__);
  return true;
}

static bool test_reset(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *name = "Lemon";

  mutt_buffer_reset(err);

  struct MbTable *VarLemon = cs_subset_mbtable(sub, "Lemon");
  char *mb = VarLemon ? VarLemon->orig_str : NULL;
  TEST_MSG("Initial: %s = '%s'\n", name, NONULL(mb));
  int rc = cs_str_string_set(cs, name, "hello", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  VarLemon = cs_subset_mbtable(sub, "Lemon");
  mb = VarLemon ? VarLemon->orig_str : NULL;
  TEST_MSG("Set: %s = '%s'\n", name, NONULL(mb));

  rc = cs_str_reset(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  VarLemon = cs_subset_mbtable(sub, "Lemon");
  mb = VarLemon ? VarLemon->orig_str : NULL;
  if (!TEST_CHECK(mutt_str_equal(mb, "lemon")))
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = '%s'\n", name, NONULL(mb));

  name = "Mango";
  mutt_buffer_reset(err);

  struct MbTable *VarMango = cs_subset_mbtable(sub, "Mango");
  TEST_MSG("Initial: %s = '%s'\n", name, VarMango->orig_str);
  dont_fail = true;
  rc = cs_str_string_set(cs, name, "hello", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  VarMango = cs_subset_mbtable(sub, "Mango");
  TEST_MSG("Set: %s = '%s'\n", name, VarMango->orig_str);
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

  VarMango = cs_subset_mbtable(sub, "Mango");
  if (!TEST_CHECK(mutt_str_equal(VarMango->orig_str, "hello")))
  {
    TEST_MSG("Value of %s changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = '%s'\n", name, VarMango->orig_str);

  log_line(__func__);
  return true;
}

static bool test_validator(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  struct MbTable *t = mbtable_parse("world");
  bool result = false;

  const char *name = "Nectarine";
  char *mb = NULL;
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, name, "hello", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    goto tv_out;
  }
  struct MbTable *VarNectarine = cs_subset_mbtable(sub, "Nectarine");
  mb = VarNectarine ? VarNectarine->orig_str : NULL;
  TEST_MSG("MbTable: %s = %s\n", name, NONULL(mb));

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP t, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    goto tv_out;
  }
  VarNectarine = cs_subset_mbtable(sub, "Nectarine");
  mb = VarNectarine ? VarNectarine->orig_str : NULL;
  TEST_MSG("Native: %s = %s\n", name, NONULL(mb));

  name = "Olive";
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "hello", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    goto tv_out;
  }
  struct MbTable *VarOlive = cs_subset_mbtable(sub, "Olive");
  mb = VarOlive ? VarOlive->orig_str : NULL;
  TEST_MSG("MbTable: %s = %s\n", name, NONULL(mb));

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP t, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    goto tv_out;
  }
  VarOlive = cs_subset_mbtable(sub, "Olive");
  mb = VarOlive ? VarOlive->orig_str : NULL;
  TEST_MSG("Native: %s = %s\n", name, NONULL(mb));

  name = "Papaya";
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "hello", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    goto tv_out;
  }
  struct MbTable *VarPapaya = cs_subset_mbtable(sub, "Papaya");
  mb = VarPapaya ? VarPapaya->orig_str : NULL;
  TEST_MSG("MbTable: %s = %s\n", name, NONULL(mb));

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP t, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    goto tv_out;
  }
  VarPapaya = cs_subset_mbtable(sub, "Papaya");
  mb = VarPapaya ? VarPapaya->orig_str : NULL;
  TEST_MSG("Native: %s = %s\n", name, NONULL(mb));

  log_line(__func__);
  result = true;
tv_out:
  mbtable_free(&t);
  return result;
}

static void dump_native(struct ConfigSet *cs, const char *parent, const char *child)
{
  intptr_t pval = cs_str_native_get(cs, parent, NULL);
  intptr_t cval = cs_str_native_get(cs, child, NULL);

  struct MbTable *pa = (struct MbTable *) pval;
  struct MbTable *ca = (struct MbTable *) cval;

  char *pstr = pa ? pa->orig_str : NULL;
  char *cstr = ca ? ca->orig_str : NULL;

  TEST_MSG("%15s = %s\n", parent, NONULL(pstr));
  TEST_MSG("%15s = %s\n", child, NONULL(cstr));
}

static bool test_inherit(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  bool result = false;

  const char *account = "fruit";
  const char *parent = "Quince";
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
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", mutt_buffer_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // set child
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, child, "world", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", mutt_buffer_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // reset child
  mutt_buffer_reset(err);
  rc = cs_str_reset(cs, child, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", mutt_buffer_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);

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

void test_config_mbtable(void)
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

  struct Buffer *err = mutt_buffer_pool_get();
  TEST_CHECK(test_initial_values(sub, err));
  TEST_CHECK(test_string_set(sub, err));
  TEST_CHECK(test_string_get(sub, err));
  TEST_CHECK(test_native_set(sub, err));
  TEST_CHECK(test_native_get(sub, err));
  TEST_CHECK(test_reset(sub, err));
  TEST_CHECK(test_validator(sub, err));
  TEST_CHECK(test_inherit(cs, err));
  mutt_buffer_pool_release(&err);

  test_neomutt_destroy();
}
