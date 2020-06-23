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
#include "config/common.h"
#include "config/lib.h"
#include "core/lib.h"

static struct MbTable *VarApple;
static struct MbTable *VarBanana;
static struct MbTable *VarCherry;
static struct MbTable *VarDamson;
static struct MbTable *VarElderberry;
static struct MbTable *VarFig;
static struct MbTable *VarGuava;
static struct MbTable *VarHawthorn;
static struct MbTable *VarIlama;
static struct MbTable *VarJackfruit;
static struct MbTable *VarKumquat;
static struct MbTable *VarLemon;
static struct MbTable *VarMango;
static struct MbTable *VarNectarine;
static struct MbTable *VarOlive;
static struct MbTable *VarPapaya;
static struct MbTable *VarQuince;

// clang-format off
static struct ConfigDef Vars[] = {
  { "Apple",      DT_MBTABLE, &VarApple,      IP "apple",      0, NULL              }, /* test_initial_values */
  { "Banana",     DT_MBTABLE, &VarBanana,     IP "banana",     0, NULL              },
  { "Cherry",     DT_MBTABLE, &VarCherry,     IP "cherry",     0, NULL              },
  { "Damson",     DT_MBTABLE, &VarDamson,     0,               0, NULL              }, /* test_mbtable_set */
  { "Elderberry", DT_MBTABLE, &VarElderberry, IP "elderberry", 0, NULL              },
  { "Fig",        DT_MBTABLE, &VarFig,        0,               0, NULL              }, /* test_mbtable_get */
  { "Guava",      DT_MBTABLE, &VarGuava,      IP "guava",      0, NULL              },
  { "Hawthorn",   DT_MBTABLE, &VarHawthorn,   0,               0, NULL              },
  { "Ilama",      DT_MBTABLE, &VarIlama,      0,               0, NULL              }, /* test_native_set */
  { "Jackfruit",  DT_MBTABLE, &VarJackfruit,  IP "jackfruit",  0, NULL              },
  { "Kumquat",    DT_MBTABLE, &VarKumquat,    0,               0, NULL              }, /* test_native_get */
  { "Lemon",      DT_MBTABLE, &VarLemon,      IP "lemon",      0, NULL              }, /* test_reset */
  { "Mango",      DT_MBTABLE, &VarMango,      IP "mango",      0, validator_fail    },
  { "Nectarine",  DT_MBTABLE, &VarNectarine,  IP "nectarine",  0, validator_succeed }, /* test_validator */
  { "Olive",      DT_MBTABLE, &VarOlive,      IP "olive",      0, validator_warn    },
  { "Papaya",     DT_MBTABLE, &VarPapaya,     IP "papaya",     0, validator_fail    },
  { "Quince",     DT_MBTABLE, &VarQuince,     0,               0, NULL              }, /* test_inherit */
  { NULL },
};
// clang-format on

static bool test_initial_values(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
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

  if (!TEST_CHECK(mutt_str_equal(value.data, "apple")))
  {
    TEST_MSG("Apple's initial value is wrong: '%s'\n", value.data);
    FREE(&value.data);
    return false;
  }
  TEST_MSG("Apple = '%s'\n", VarApple ? VarApple->orig_str : "");
  TEST_MSG("Apple's initial value is %s\n", value.data);

  mutt_buffer_reset(&value);
  rc = cs_str_initial_get(cs, "Banana", &value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  if (!TEST_CHECK(mutt_str_equal(value.data, "banana")))
  {
    TEST_MSG("Banana's initial value is wrong: %s\n", value.data);
    FREE(&value.data);
    return false;
  }
  TEST_MSG("Banana = '%s'\n", VarBanana ? VarBanana->orig_str : "");
  TEST_MSG("Banana's initial value is %s\n", NONULL(value.data));

  mutt_buffer_reset(&value);
  rc = cs_str_initial_set(cs, "Cherry", "config.*", &value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  mutt_buffer_reset(&value);
  rc = cs_str_initial_set(cs, "Cherry", "file.*", &value);
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

  TEST_MSG("Cherry = '%s'\n", VarCherry->orig_str);
  TEST_MSG("Cherry's initial value is '%s'\n", NONULL(value.data));

  FREE(&value.data);
  log_line(__func__);
  return true;
}

static bool test_string_set(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

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
      TEST_MSG("%s\n", err->data);
      return false;
    }

    if (rc & CSR_SUC_NO_CHANGE)
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      continue;
    }

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
      TEST_MSG("%s\n", err->data);
      return false;
    }

    if (rc & CSR_SUC_NO_CHANGE)
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      continue;
    }

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
    TEST_MSG("%s\n", err->data);
    return false;
  }

  log_line(__func__);
  return true;
}

static bool test_string_get(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  const char *name = "Fig";
  char *mb = NULL;

  mutt_buffer_reset(err);
  int rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", err->data);
    return false;
  }
  mb = VarFig ? VarFig->orig_str : NULL;
  TEST_MSG("%s = '%s', '%s'\n", name, NONULL(mb), err->data);

  name = "Guava";
  mutt_buffer_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", err->data);
    return false;
  }
  mb = VarGuava ? VarGuava->orig_str : NULL;
  TEST_MSG("%s = '%s', '%s'\n", name, NONULL(mb), err->data);

  name = "Hawthorn";
  rc = cs_str_string_set(cs, name, "hawthorn", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;

  mutt_buffer_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", err->data);
    return false;
  }
  mb = VarHawthorn ? VarHawthorn->orig_str : NULL;
  TEST_MSG("%s = '%s', '%s'\n", name, NONULL(mb), err->data);

  log_line(__func__);
  return true;
}

static bool test_native_set(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  struct MbTable *t = mbtable_parse("hello");
  const char *name = "Ilama";
  char *mb = NULL;
  bool result = false;

  mutt_buffer_reset(err);
  int rc = cs_str_native_set(cs, name, (intptr_t) t, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
    goto tns_out;
  }

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
    TEST_MSG("%s\n", err->data);
    goto tns_out;
  }

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

static bool test_native_get(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  const char *name = "Kumquat";

  int rc = cs_str_string_set(cs, name, "kumquat", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;

  mutt_buffer_reset(err);
  intptr_t value = cs_str_native_get(cs, name, err);
  struct MbTable *t = (struct MbTable *) value;

  if (!TEST_CHECK(VarKumquat == t))
  {
    TEST_MSG("Get failed: %s\n", err->data);
    return false;
  }
  char *mb1 = VarKumquat ? VarKumquat->orig_str : NULL;
  char *mb2 = t ? t->orig_str : NULL;
  TEST_MSG("%s = '%s', '%s'\n", name, NONULL(mb1), NONULL(mb2));

  log_line(__func__);
  return true;
}

static bool test_reset(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  const char *name = "Lemon";

  mutt_buffer_reset(err);

  char *mb = VarLemon ? VarLemon->orig_str : NULL;
  TEST_MSG("Initial: %s = '%s'\n", name, NONULL(mb));
  int rc = cs_str_string_set(cs, name, "hello", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  mb = VarLemon ? VarLemon->orig_str : NULL;
  TEST_MSG("Set: %s = '%s'\n", name, NONULL(mb));

  rc = cs_str_reset(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  mb = VarLemon ? VarLemon->orig_str : NULL;
  if (!TEST_CHECK(mutt_str_equal(mb, "lemon")))
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = '%s'\n", name, NONULL(mb));

  name = "Mango";
  mutt_buffer_reset(err);

  TEST_MSG("Initial: %s = '%s'\n", name, VarMango->orig_str);
  dont_fail = true;
  rc = cs_str_string_set(cs, name, "hello", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  TEST_MSG("Set: %s = '%s'\n", name, VarMango->orig_str);
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

  if (!TEST_CHECK(mutt_str_equal(VarMango->orig_str, "hello")))
  {
    TEST_MSG("Value of %s changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = '%s'\n", name, VarMango->orig_str);

  log_line(__func__);
  return true;
}

static bool test_validator(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  struct MbTable *t = mbtable_parse("world");
  bool result = false;

  const char *name = "Nectarine";
  char *mb = NULL;
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, name, "hello", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    goto tv_out;
  }
  mb = VarNectarine ? VarNectarine->orig_str : NULL;
  TEST_MSG("MbTable: %s = %s\n", name, NONULL(mb));

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP t, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    goto tv_out;
  }
  mb = VarNectarine ? VarNectarine->orig_str : NULL;
  TEST_MSG("Native: %s = %s\n", name, NONULL(mb));

  name = "Olive";
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "hello", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    goto tv_out;
  }
  mb = VarOlive ? VarOlive->orig_str : NULL;
  TEST_MSG("MbTable: %s = %s\n", name, NONULL(mb));

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP t, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    goto tv_out;
  }
  mb = VarOlive ? VarOlive->orig_str : NULL;
  TEST_MSG("Native: %s = %s\n", name, NONULL(mb));

  name = "Papaya";
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "hello", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    goto tv_out;
  }
  mb = VarPapaya ? VarPapaya->orig_str : NULL;
  TEST_MSG("MbTable: %s = %s\n", name, NONULL(mb));

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP t, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    goto tv_out;
  }
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
    TEST_MSG("Error: %s\n", err->data);
    goto ti_out;
  }

  // set parent
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, parent, "hello", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // set child
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, child, "world", err);
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

void test_config_mbtable(void)
{
  struct Buffer err;
  mutt_buffer_init(&err);
  err.dsize = 256;
  err.data = mutt_mem_calloc(1, err.dsize);
  mutt_buffer_reset(&err);

  struct ConfigSet *cs = cs_new(30);
  NeoMutt = neomutt_new(cs);

  mbtable_init(cs);
  dont_fail = true;
  if (!cs_register_variables(cs, Vars, 0))
    return;
  dont_fail = false;

  notify_observer_add(NeoMutt->notify, log_observer, 0);

  set_list(cs);

  TEST_CHECK(test_initial_values(cs, &err));
  TEST_CHECK(test_string_set(cs, &err));
  TEST_CHECK(test_string_get(cs, &err));
  TEST_CHECK(test_native_set(cs, &err));
  TEST_CHECK(test_native_get(cs, &err));
  TEST_CHECK(test_reset(cs, &err));
  TEST_CHECK(test_validator(cs, &err));
  TEST_CHECK(test_inherit(cs, &err));

  neomutt_free(&NeoMutt);
  cs_free(&cs);
  FREE(&err.data);
}
