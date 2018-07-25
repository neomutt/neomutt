/**
 * @file
 * Test code for the Regex object
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
#include "mutt/regex3.h"
#include "mutt/string2.h"
#include "config/account.h"
#include "config/regex2.h"
#include "config/set.h"
#include "config/types.h"
#include "config/common.h"

static struct Regex *VarApple;
static struct Regex *VarBanana;
static struct Regex *VarCherry;
static struct Regex *VarDamson;
static struct Regex *VarElderberry;
static struct Regex *VarFig;
static struct Regex *VarGuava;
static struct Regex *VarHawthorn;
static struct Regex *VarIlama;
static struct Regex *VarJackfruit;
static struct Regex *VarKumquat;
static struct Regex *VarLemon;
static struct Regex *VarMango;
static struct Regex *VarNectarine;
static struct Regex *VarOlive;
static struct Regex *VarPapaya;
static struct Regex *VarQuince;
static struct Regex *VarRaspberry;
static struct Regex *VarStrawberry;

// clang-format off
static struct ConfigDef Vars[] = {
  { "Apple",      DT_REGEX, 0,                  &VarApple,      IP "apple.*",      NULL              }, /* test_initial_values */
  { "Banana",     DT_REGEX, 0,                  &VarBanana,     IP "banana.*",     NULL              },
  { "Cherry",     DT_REGEX, 0,                  &VarCherry,     IP "cherry.*",     NULL              },
  { "Damson",     DT_REGEX, 0,                  &VarDamson,     0,                 NULL              }, /* test_regex_set */
  { "Elderberry", DT_REGEX, DT_REGEX_NOSUB,     &VarElderberry, IP "elderberry.*", NULL              },
  { "Fig",        DT_REGEX, 0,                  &VarFig,        0,                 NULL              }, /* test_regex_get */
  { "Guava",      DT_REGEX, 0,                  &VarGuava,      IP "guava.*",      NULL              },
  { "Hawthorn",   DT_REGEX, 0,                  &VarHawthorn,   0,                 NULL              },
  { "Ilama",      DT_REGEX, DT_REGEX_ALLOW_NOT, &VarIlama,      0,                 NULL              }, /* test_native_set */
  { "Jackfruit",  DT_REGEX, 0,                  &VarJackfruit,  IP "jackfruit.*",  NULL              },
  { "Kumquat",    DT_REGEX, 0,                  &VarKumquat,    IP "kumquat.*",    NULL              },
  { "Lemon",      DT_REGEX, 0,                  &VarLemon,      0,                 NULL              }, /* test_native_get */
  { "Mango",      DT_REGEX, 0,                  &VarMango,      IP "mango.*",      NULL              }, /* test_reset */
  { "Nectarine",  DT_REGEX, 0,                  &VarNectarine,  IP "\\1",          NULL              },
  { "Olive",      DT_REGEX, 0,                  &VarOlive,      IP "olive.*",      validator_fail    },
  { "Papaya",     DT_REGEX, 0,                  &VarPapaya,     IP "papaya.*",     validator_succeed }, /* test_validator */
  { "Quince",     DT_REGEX, 0,                  &VarQuince,     IP "quince.*",     validator_warn    },
  { "Raspberry",  DT_REGEX, 0,                  &VarRaspberry,  IP "raspberry.*",  validator_fail    },
  { "Strawberry", DT_REGEX, 0,                  &VarStrawberry, 0,                 NULL              }, /* test_inherit */
  { NULL },
};
// clang-format on

static bool test_initial_values(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  printf("Apple = %s\n", VarApple->pattern);
  printf("Banana = %s\n", VarBanana->pattern);

  if ((mutt_str_strcmp(VarApple->pattern, "apple.*") != 0) ||
      (mutt_str_strcmp(VarBanana->pattern, "banana.*") != 0))
  {
    printf("Error: initial values were wrong\n");
    return false;
  }

  cs_str_string_set(cs, "Apple", "car*", err);
  cs_str_string_set(cs, "Banana", "train*", err);

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

  if (mutt_str_strcmp(value.data, "apple.*") != 0)
  {
    printf("Apple's initial value is wrong: '%s'\n", value.data);
    FREE(&value.data);
    return false;
  }
  printf("Apple = '%s'\n", VarApple ? VarApple->pattern : "");
  printf("Apple's initial value is %s\n", value.data);

  mutt_buffer_reset(&value);
  rc = cs_str_initial_get(cs, "Banana", &value);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  if (mutt_str_strcmp(value.data, "banana.*") != 0)
  {
    printf("Banana's initial value is wrong: %s\n", value.data);
    FREE(&value.data);
    return false;
  }
  printf("Banana = '%s'\n", VarBanana ? VarBanana->pattern : "");
  printf("Banana's initial value is %s\n", NONULL(value.data));

  mutt_buffer_reset(&value);
  rc = cs_str_initial_set(cs, "Cherry", "up.*", &value);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  mutt_buffer_reset(&value);
  rc = cs_str_initial_set(cs, "Cherry", "down.*", &value);
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

  printf("Cherry = '%s'\n", VarCherry->pattern);
  printf("Cherry's initial value is '%s'\n", NONULL(value.data));

  FREE(&value.data);
  return true;
}

static bool test_string_set(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  const char *valid[] = { "hello.*", "world.*", "world.*", "", NULL };
  char *name = "Damson";
  char *regex = NULL;

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

    regex = VarDamson ? VarDamson->pattern : NULL;
    if (mutt_str_strcmp(regex, valid[i]) != 0)
    {
      printf("Value of %s wasn't changed\n", name);
      return false;
    }
    printf("%s = '%s', set by '%s'\n", name, NONULL(regex), NONULL(valid[i]));
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

    regex = VarElderberry ? VarElderberry->pattern : NULL;
    if (mutt_str_strcmp(regex, valid[i]) != 0)
    {
      printf("Value of %s wasn't changed\n", name);
      return false;
    }
    printf("%s = '%s', set by '%s'\n", name, NONULL(regex), NONULL(valid[i]));
  }

  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "\\1", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("Expected error: %s\n", err->data);
  }
  else
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
  char *regex = NULL;

  mutt_buffer_reset(err);
  int rc = cs_str_string_get(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("Get failed: %s\n", err->data);
    return false;
  }
  regex = VarFig ? VarFig->pattern : NULL;
  printf("%s = '%s', '%s'\n", name, NONULL(regex), err->data);

  name = "Guava";
  mutt_buffer_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("Get failed: %s\n", err->data);
    return false;
  }
  regex = VarGuava ? VarGuava->pattern : NULL;
  printf("%s = '%s', '%s'\n", name, NONULL(regex), err->data);

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
  regex = VarHawthorn ? VarHawthorn->pattern : NULL;
  printf("%s = '%s', '%s'\n", name, NONULL(regex), err->data);

  return true;
}

static bool test_native_set(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  struct Regex *r = regex_create("hello.*", 0, err);
  char *name = "Ilama";
  char *regex = NULL;
  bool result = false;

  mutt_buffer_reset(err);
  int rc = cs_str_native_set(cs, name, (intptr_t) r, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("%s\n", err->data);
    goto tns_out;
  }

  regex = VarIlama ? VarIlama->pattern : NULL;
  if (mutt_str_strcmp(regex, r->pattern) != 0)
  {
    printf("Value of %s wasn't changed\n", name);
    goto tns_out;
  }
  printf("%s = '%s', set by '%s'\n", name, NONULL(regex), r->pattern);

  regex_free(&r);
  r = regex_create("!world.*", DT_REGEX_ALLOW_NOT, err);
  name = "Ilama";

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, (intptr_t) r, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("%s\n", err->data);
    goto tns_out;
  }
  printf("'%s', not flag set to %d\n", VarIlama->pattern, VarIlama->not);

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
  regex = VarJackfruit ? VarJackfruit->pattern : NULL;
  printf("%s = '%s', set by NULL\n", name, NONULL(regex));

  regex_free(&r);
  r = regex_create("world.*", 0, err);
  r->pattern[0] = '\\';
  r->pattern[1] = '1';
  name = "Kumquat";

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, (intptr_t) r, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("Expected error: %s\n", err->data);
  }
  else
  {
    printf("%s\n", err->data);
    goto tns_out;
  }

  result = true;
tns_out:
  regex_free(&r);
  return result;
}

static bool test_native_get(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  char *name = "Lemon";

  int rc = cs_str_string_set(cs, name, "lemon.*", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
    return false;

  mutt_buffer_reset(err);
  intptr_t value = cs_str_native_get(cs, name, err);
  struct Regex *r = (struct Regex *) value;

  if (VarLemon != r)
  {
    printf("Get failed: %s\n", err->data);
    return false;
  }
  char *regex1 = VarLemon ? VarLemon->pattern : NULL;
  char *regex2 = r ? r->pattern : NULL;
  printf("%s = '%s', '%s'\n", name, NONULL(regex1), NONULL(regex2));

  return true;
}

static bool test_reset(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  char *name = "Mango";

  mutt_buffer_reset(err);

  char *regex = VarMango ? VarMango->pattern : NULL;
  printf("Initial: %s = '%s'\n", name, NONULL(regex));
  int rc = cs_str_string_set(cs, name, "hello.*", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
    return false;
  regex = VarMango ? VarMango->pattern : NULL;
  printf("Set: %s = '%s'\n", name, NONULL(regex));

  rc = cs_str_reset(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("%s\n", err->data);
    return false;
  }

  regex = VarMango ? VarMango->pattern : NULL;
  if (mutt_str_strcmp(regex, "mango.*") != 0)
  {
    printf("Value of %s wasn't changed\n", name);
    return false;
  }

  printf("Reset: %s = '%s'\n", name, NONULL(regex));

  rc = cs_str_reset(cs, "Nectarine", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("Expected error: %s\n", err->data);
  }
  else
  {
    printf("%s\n", err->data);
    return false;
  }

  name = "Olive";
  mutt_buffer_reset(err);

  printf("Initial: %s = '%s'\n", name, VarOlive->pattern);
  dont_fail = true;
  rc = cs_str_string_set(cs, name, "hel*o", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
    return false;
  printf("Set: %s = '%s'\n", name, VarOlive->pattern);
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

  if (mutt_str_strcmp(VarOlive->pattern, "hel*o") != 0)
  {
    printf("Value of %s changed\n", name);
    return false;
  }

  printf("Reset: %s = '%s'\n", name, VarOlive->pattern);

  return true;
}

static bool test_validator(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  char *regex = NULL;
  struct Regex *r = regex_create("world.*", 0, err);
  bool result = false;

  char *name = "Papaya";
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, name, "hello.*", err);
  if (CSR_RESULT(rc) == CSR_SUCCESS)
  {
    printf("%s\n", err->data);
  }
  else
  {
    printf("%s\n", err->data);
    goto tv_out;
  }
  regex = VarPapaya ? VarPapaya->pattern : NULL;
  printf("Regex: %s = %s\n", name, NONULL(regex));

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP r, err);
  if (CSR_RESULT(rc) == CSR_SUCCESS)
  {
    printf("%s\n", err->data);
  }
  else
  {
    printf("%s\n", err->data);
    goto tv_out;
  }
  regex = VarPapaya ? VarPapaya->pattern : NULL;
  printf("Native: %s = %s\n", name, NONULL(regex));

  name = "Quince";
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "hello.*", err);
  if (CSR_RESULT(rc) == CSR_SUCCESS)
  {
    printf("%s\n", err->data);
  }
  else
  {
    printf("%s\n", err->data);
    goto tv_out;
  }
  regex = VarQuince ? VarQuince->pattern : NULL;
  printf("Regex: %s = %s\n", name, NONULL(regex));

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP r, err);
  if (CSR_RESULT(rc) == CSR_SUCCESS)
  {
    printf("%s\n", err->data);
  }
  else
  {
    printf("%s\n", err->data);
    goto tv_out;
  }
  regex = VarQuince ? VarQuince->pattern : NULL;
  printf("Native: %s = %s\n", name, NONULL(regex));

  name = "Raspberry";
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "hello.*", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("Expected error: %s\n", err->data);
  }
  else
  {
    printf("%s\n", err->data);
    goto tv_out;
  }
  regex = VarRaspberry ? VarRaspberry->pattern : NULL;
  printf("Regex: %s = %s\n", name, NONULL(regex));

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP r, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("Expected error: %s\n", err->data);
  }
  else
  {
    printf("%s\n", err->data);
    goto tv_out;
  }
  regex = VarRaspberry ? VarRaspberry->pattern : NULL;
  printf("Native: %s = %s\n", name, NONULL(regex));

  result = true;
tv_out:
  regex_free(&r);
  return result;
}

static void dump_native(struct ConfigSet *cs, const char *parent, const char *child)
{
  intptr_t pval = cs_str_native_get(cs, parent, NULL);
  intptr_t cval = cs_str_native_get(cs, child, NULL);

  struct Regex *pa = (struct Regex *) pval;
  struct Regex *ca = (struct Regex *) cval;

  char *pstr = pa ? pa->pattern : NULL;
  char *cstr = ca ? ca->pattern : NULL;

  printf("%15s = %s\n", parent, NONULL(pstr));
  printf("%15s = %s\n", child, NONULL(cstr));
}

static bool test_inherit(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  bool result = false;

  const char *account = "fruit";
  const char *parent = "Strawberry";
  char child[128];
  snprintf(child, sizeof(child), "%s:%s", account, parent);

  const char *AccountVarRegex[] = {
    parent, NULL,
  };

  struct Account *ac = ac_create(cs, account, AccountVarRegex);

  // set parent
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, parent, "hello.*", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // set child
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, child, "world.*", err);
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

bool regex_test(void)
{
  log_line(__func__);

  struct Buffer err;
  mutt_buffer_init(&err);
  err.data = mutt_mem_calloc(1, STRING);
  err.dsize = STRING;
  mutt_buffer_reset(&err);

  struct ConfigSet *cs = cs_create(30);

  regex_init(cs);
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
