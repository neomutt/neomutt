/**
 * @file
 * Test code for the Mbtable object
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/buffer.h"
#include "mutt/memory.h"
#include "mutt/string2.h"
#include "config/account.h"
#include "config/mbtable.h"
#include "config/set.h"
#include "config/types.h"
#include "config/common.h"

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
  { "Apple",      DT_MBTABLE, 0, &VarApple,      IP "apple",      NULL              }, /* test_initial_values */
  { "Banana",     DT_MBTABLE, 0, &VarBanana,     IP "banana",     NULL              },
  { "Cherry",     DT_MBTABLE, 0, &VarCherry,     IP "cherry",     NULL              },
  { "Damson",     DT_MBTABLE, 0, &VarDamson,     0,               NULL              }, /* test_mbtable_set */
  { "Elderberry", DT_MBTABLE, 0, &VarElderberry, IP "elderberry", NULL              },
  { "Fig",        DT_MBTABLE, 0, &VarFig,        0,               NULL              }, /* test_mbtable_get */
  { "Guava",      DT_MBTABLE, 0, &VarGuava,      IP "guava",      NULL              },
  { "Hawthorn",   DT_MBTABLE, 0, &VarHawthorn,   0,               NULL              },
  { "Ilama",      DT_MBTABLE, 0, &VarIlama,      0,               NULL              }, /* test_native_set */
  { "Jackfruit",  DT_MBTABLE, 0, &VarJackfruit,  IP "jackfruit",  NULL              },
  { "Kumquat",    DT_MBTABLE, 0, &VarKumquat,    0,               NULL              }, /* test_native_get */
  { "Lemon",      DT_MBTABLE, 0, &VarLemon,      IP "lemon",      NULL              }, /* test_reset */
  { "Mango",      DT_MBTABLE, 0, &VarMango,      IP "mango",      validator_fail    },
  { "Nectarine",  DT_MBTABLE, 0, &VarNectarine,  IP "nectarine",  validator_succeed }, /* test_validator */
  { "Olive",      DT_MBTABLE, 0, &VarOlive,      IP "olive",      validator_warn    },
  { "Papaya",     DT_MBTABLE, 0, &VarPapaya,     IP "papaya",     validator_fail    },
  { "Quince",     DT_MBTABLE, 0, &VarQuince,     0,               NULL              }, /* test_inherit */
  { NULL },
};
// clang-format on

static bool test_initial_values(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  printf("Apple = %s\n", VarApple->orig_str);
  printf("Banana = %s\n", VarBanana->orig_str);

  if ((mutt_str_strcmp(VarApple->orig_str, "apple") != 0) ||
      (mutt_str_strcmp(VarBanana->orig_str, "banana") != 0))
  {
    printf("Error: initial values were wrong\n");
    return false;
  }

  cs_str_string_set(cs, "Apple", "car", err);
  cs_str_string_set(cs, "Banana", "train", err);

  struct Buffer value;
  mutt_buffer_init(&value);
  value.data = mutt_mem_calloc(1, STRING);
  value.dsize = STRING;
  mutt_buffer_reset(&value);

  int rc;

  mutt_buffer_reset(&value);
  rc = cs_str_initial_get(cs, "Apple", &value);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  if (mutt_str_strcmp(value.data, "apple") != 0)
  {
    printf("Apple's initial value is wrong: '%s'\n", value.data);
    FREE(&value.data);
    return false;
  }
  printf("Apple = '%s'\n", VarApple ? VarApple->orig_str : "");
  printf("Apple's initial value is %s\n", value.data);

  mutt_buffer_reset(&value);
  rc = cs_str_initial_get(cs, "Banana", &value);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  if (mutt_str_strcmp(value.data, "banana") != 0)
  {
    printf("Banana's initial value is wrong: %s\n", value.data);
    FREE(&value.data);
    return false;
  }
  printf("Banana = '%s'\n", VarBanana ? VarBanana->orig_str : "");
  printf("Banana's initial value is %s\n", NONULL(value.data));

  mutt_buffer_reset(&value);
  rc = cs_str_initial_set(cs, "Cherry", "config.*", &value);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  mutt_buffer_reset(&value);
  rc = cs_str_initial_set(cs, "Cherry", "file.*", &value);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  mutt_buffer_reset(&value);
  rc = cs_str_initial_get(cs, "Cherry", &value);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  printf("Cherry = '%s'\n", VarCherry->orig_str);
  printf("Cherry's initial value is '%s'\n", NONULL(value.data));

  FREE(&value.data);
  return true;
}

static bool test_string_set(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  const char *valid[] = { "hello", "world", "world", "", NULL };
  char *name = "Damson";
  char *mb = NULL;

  int rc;
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    mutt_buffer_reset(err);
    rc = cs_str_string_set(cs, name, valid[i], err);
    if (CSR_RESULT(rc) != CSR_SUCCESS)
    {
      printf("%s\n", err->data);
      return false;
    }

    if (rc & CSR_SUC_NO_CHANGE)
    {
      printf("Value of %s wasn't changed\n", name);
      continue;
    }

    mb = VarDamson ? VarDamson->orig_str : NULL;
    if (mutt_str_strcmp(mb, valid[i]) != 0)
    {
      printf("Value of %s wasn't changed\n", name);
      return false;
    }
    printf("%s = '%s', set by '%s'\n", name, NONULL(mb), NONULL(valid[i]));
  }

  name = "Elderberry";
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    mutt_buffer_reset(err);
    rc = cs_str_string_set(cs, name, valid[i], err);
    if (CSR_RESULT(rc) != CSR_SUCCESS)
    {
      printf("%s\n", err->data);
      return false;
    }

    if (rc & CSR_SUC_NO_CHANGE)
    {
      printf("Value of %s wasn't changed\n", name);
      continue;
    }

    const char *orig_str = VarElderberry ? VarElderberry->orig_str : NULL;
    if (mutt_str_strcmp(orig_str, valid[i]) != 0)
    {
      printf("Value of %s wasn't changed\n", name);
      return false;
    }
    printf("%s = '%s', set by '%s'\n", name, NONULL(orig_str), NONULL(valid[i]));
  }

  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "\377", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("%s\n", err->data);
    return false;
  }

  return true;
}

static bool test_string_get(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  const char *name = "Fig";
  char *mb = NULL;

  mutt_buffer_reset(err);
  int rc = cs_str_string_get(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("Get failed: %s\n", err->data);
    return false;
  }
  mb = VarFig ? VarFig->orig_str : NULL;
  printf("%s = '%s', '%s'\n", name, NONULL(mb), err->data);

  name = "Guava";
  mutt_buffer_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("Get failed: %s\n", err->data);
    return false;
  }
  mb = VarGuava ? VarGuava->orig_str : NULL;
  printf("%s = '%s', '%s'\n", name, NONULL(mb), err->data);

  name = "Hawthorn";
  rc = cs_str_string_set(cs, name, "hawthorn", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
    return false;

  mutt_buffer_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("Get failed: %s\n", err->data);
    return false;
  }
  mb = VarHawthorn ? VarHawthorn->orig_str : NULL;
  printf("%s = '%s', '%s'\n", name, NONULL(mb), err->data);

  return true;
}

static bool test_native_set(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  struct MbTable *t = mbtable_parse("hello");
  char *name = "Ilama";
  char *mb = NULL;
  bool result = false;

  mutt_buffer_reset(err);
  int rc = cs_str_native_set(cs, name, (intptr_t) t, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("%s\n", err->data);
    goto tns_out;
  }

  mb = VarIlama ? VarIlama->orig_str : NULL;
  if (mutt_str_strcmp(mb, t->orig_str) != 0)
  {
    printf("Value of %s wasn't changed\n", name);
    goto tns_out;
  }
  printf("%s = '%s', set by '%s'\n", name, NONULL(mb), t->orig_str);

  name = "Jackfruit";
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, 0, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("%s\n", err->data);
    goto tns_out;
  }

  if (VarJackfruit != NULL)
  {
    printf("Value of %s wasn't changed\n", name);
    goto tns_out;
  }
  mb = VarJackfruit ? VarJackfruit->orig_str : NULL;
  printf("%s = '%s', set by NULL\n", name, NONULL(mb));

  result = true;
tns_out:
  mbtable_free(&t);
  return result;
}

static bool test_native_get(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  char *name = "Kumquat";

  int rc = cs_str_string_set(cs, name, "kumquat", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
    return false;

  mutt_buffer_reset(err);
  intptr_t value = cs_str_native_get(cs, name, err);
  struct MbTable *t = (struct MbTable *) value;

  if (VarKumquat != t)
  {
    printf("Get failed: %s\n", err->data);
    return false;
  }
  char *mb1 = VarKumquat ? VarKumquat->orig_str : NULL;
  char *mb2 = t ? t->orig_str : NULL;
  printf("%s = '%s', '%s'\n", name, NONULL(mb1), NONULL(mb2));

  return true;
}

static bool test_reset(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  char *name = "Lemon";
  char *mb = NULL;

  mutt_buffer_reset(err);

  mb = VarLemon ? VarLemon->orig_str : NULL;
  printf("Initial: %s = '%s'\n", name, NONULL(mb));
  int rc = cs_str_string_set(cs, name, "hello", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
    return false;
  mb = VarLemon ? VarLemon->orig_str : NULL;
  printf("Set: %s = '%s'\n", name, NONULL(mb));

  rc = cs_str_reset(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("%s\n", err->data);
    return false;
  }

  mb = VarLemon ? VarLemon->orig_str : NULL;
  if (mutt_str_strcmp(mb, "lemon") != 0)
  {
    printf("Value of %s wasn't changed\n", name);
    return false;
  }

  printf("Reset: %s = '%s'\n", name, NONULL(mb));

  name = "Mango";
  mutt_buffer_reset(err);

  printf("Initial: %s = '%s'\n", name, VarMango->orig_str);
  dont_fail = true;
  rc = cs_str_string_set(cs, name, "hello", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
    return false;
  printf("Set: %s = '%s'\n", name, VarMango->orig_str);
  dont_fail = false;

  rc = cs_str_reset(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("Expected error: %s\n", err->data);
  }
  else
  {
    printf("%s\n", err->data);
    return false;
  }

  if (mutt_str_strcmp(VarMango->orig_str, "hello") != 0)
  {
    printf("Value of %s changed\n", name);
    return false;
  }

  printf("Reset: %s = '%s'\n", name, VarMango->orig_str);

  return true;
}

static bool test_validator(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  struct MbTable *t = mbtable_parse("world");
  bool result = false;

  char *name = "Nectarine";
  char *mb = NULL;
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, name, "hello", err);
  if (CSR_RESULT(rc) == CSR_SUCCESS)
  {
    printf("%s\n", err->data);
  }
  else
  {
    printf("%s\n", err->data);
    goto tv_out;
  }
  mb = VarNectarine ? VarNectarine->orig_str : NULL;
  printf("MbTable: %s = %s\n", name, NONULL(mb));

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP t, err);
  if (CSR_RESULT(rc) == CSR_SUCCESS)
  {
    printf("%s\n", err->data);
  }
  else
  {
    printf("%s\n", err->data);
    goto tv_out;
  }
  mb = VarNectarine ? VarNectarine->orig_str : NULL;
  printf("Native: %s = %s\n", name, NONULL(mb));

  name = "Olive";
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "hello", err);
  if (CSR_RESULT(rc) == CSR_SUCCESS)
  {
    printf("%s\n", err->data);
  }
  else
  {
    printf("%s\n", err->data);
    goto tv_out;
  }
  mb = VarOlive ? VarOlive->orig_str : NULL;
  printf("MbTable: %s = %s\n", name, NONULL(mb));

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP t, err);
  if (CSR_RESULT(rc) == CSR_SUCCESS)
  {
    printf("%s\n", err->data);
  }
  else
  {
    printf("%s\n", err->data);
    goto tv_out;
  }
  mb = VarOlive ? VarOlive->orig_str : NULL;
  printf("Native: %s = %s\n", name, NONULL(mb));

  name = "Papaya";
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "hello", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("Expected error: %s\n", err->data);
  }
  else
  {
    printf("%s\n", err->data);
    goto tv_out;
  }
  mb = VarPapaya ? VarPapaya->orig_str : NULL;
  printf("MbTable: %s = %s\n", name, NONULL(mb));

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP t, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("Expected error: %s\n", err->data);
  }
  else
  {
    printf("%s\n", err->data);
    goto tv_out;
  }
  mb = VarPapaya ? VarPapaya->orig_str : NULL;
  printf("Native: %s = %s\n", name, NONULL(mb));

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

  printf("%15s = %s\n", parent, NONULL(pstr));
  printf("%15s = %s\n", child, NONULL(cstr));
}

static bool test_inherit(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  bool result = false;

  const char *account = "fruit";
  const char *parent = "Quince";
  char child[128];
  snprintf(child, sizeof(child), "%s:%s", account, parent);

  const char *AccountVarMb[] = {
    parent, NULL,
  };

  struct Account *ac = ac_create(cs, account, AccountVarMb);

  // set parent
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, parent, "hello", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // set child
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, child, "world", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // reset child
  mutt_buffer_reset(err);
  rc = cs_str_reset(cs, child, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // reset parent
  mutt_buffer_reset(err);
  rc = cs_str_reset(cs, parent, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);

  result = true;
ti_out:
  ac_free(cs, &ac);
  return result;
}

bool mbtable_test(void)
{
  log_line(__func__);

  struct Buffer err;
  mutt_buffer_init(&err);
  err.data = mutt_mem_calloc(1, STRING);
  err.dsize = STRING;
  mutt_buffer_reset(&err);

  struct ConfigSet *cs = cs_create(30);

  mbtable_init(cs);
  dont_fail = true;
  if (!cs_register_variables(cs, Vars, 0))
    return false;
  dont_fail = false;

  cs_add_listener(cs, log_listener);

  set_list(cs);

  if (!test_initial_values(cs, &err))
    return false;
  if (!test_string_set(cs, &err))
    return false;
  if (!test_string_get(cs, &err))
    return false;
  if (!test_native_set(cs, &err))
    return false;
  if (!test_native_get(cs, &err))
    return false;
  if (!test_reset(cs, &err))
    return false;
  if (!test_validator(cs, &err))
    return false;
  if (!test_inherit(cs, &err))
    return false;

  cs_free(&cs);
  FREE(&err.data);

  return true;
}
