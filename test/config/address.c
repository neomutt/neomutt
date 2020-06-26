/**
 * @file
 * Test code for the Address object
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
#include "address/lib.h"
#include "config/common.h"
#include "config/lib.h"
#include "core/lib.h"

static struct Address *VarApple;
static struct Address *VarBanana;
static struct Address *VarCherry;
static struct Address *VarDamson;
static struct Address *VarElderberry;
static struct Address *VarFig;
static struct Address *VarGuava;
static struct Address *VarHawthorn;
static struct Address *VarIlama;
static struct Address *VarJackfruit;
static struct Address *VarKumquat;
static struct Address *VarLemon;
static struct Address *VarMango;
static struct Address *VarNectarine;
static struct Address *VarOlive;
static struct Address *VarPapaya;
static struct Address *VarQuince;

// clang-format off
static struct ConfigDef Vars[] = {
  { "Apple",      DT_ADDRESS, &VarApple,      IP "apple@example.com",      0, NULL              }, /* test_initial_values */
  { "Banana",     DT_ADDRESS, &VarBanana,     IP "banana@example.com",     0, NULL              },
  { "Cherry",     DT_ADDRESS, &VarCherry,     IP "cherry@example.com",     0, NULL              },
  { "Damson",     DT_ADDRESS, &VarDamson,     0,                           0, NULL              }, /* test_address_set */
  { "Elderberry", DT_ADDRESS, &VarElderberry, IP "elderberry@example.com", 0, NULL              },
  { "Fig",        DT_ADDRESS, &VarFig,        0,                           0, NULL              }, /* test_address_get */
  { "Guava",      DT_ADDRESS, &VarGuava,      IP "guava@example.com",      0, NULL              },
  { "Hawthorn",   DT_ADDRESS, &VarHawthorn,   0,                           0, NULL              },
  { "Ilama",      DT_ADDRESS, &VarIlama,      0,                           0, NULL              }, /* test_native_set */
  { "Jackfruit",  DT_ADDRESS, &VarJackfruit,  IP "jackfruit@example.com",  0, NULL              },
  { "Kumquat",    DT_ADDRESS, &VarKumquat,    0,                           0, NULL              }, /* test_native_get */
  { "Lemon",      DT_ADDRESS, &VarLemon,      IP "lemon@example.com",      0, NULL              }, /* test_reset */
  { "Mango",      DT_ADDRESS, &VarMango,      IP "mango@example.com",      0, validator_fail    },
  { "Nectarine",  DT_ADDRESS, &VarNectarine,  IP "nectarine@example.com",  0, validator_succeed }, /* test_validator */
  { "Olive",      DT_ADDRESS, &VarOlive,      IP "olive@example.com",      0, validator_warn    },
  { "Papaya",     DT_ADDRESS, &VarPapaya,     IP "papaya@example.com",     0, validator_fail    },
  { "Quince",     DT_ADDRESS, &VarQuince,     0,                           0, NULL              }, /* test_inherit */
  { NULL },
};
// clang-format on

static bool test_initial_values(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  TEST_MSG("Apple = '%s'\n", VarApple->mailbox);
  TEST_MSG("Banana = '%s'\n", VarBanana->mailbox);

  const char *apple_orig = "apple@example.com";
  const char *banana_orig = "banana@example.com";

  if (!TEST_CHECK(mutt_str_equal(VarApple->mailbox, apple_orig)))
  {
    TEST_MSG("Error: initial values were wrong\n");
    return false;
  }

  if (!TEST_CHECK(mutt_str_equal(VarBanana->mailbox, banana_orig)))
  {
    TEST_MSG("Error: initial values were wrong\n");
    return false;
  }

  cs_str_string_set(cs, "Apple", "granny@smith.com", err);
  cs_str_string_set(cs, "Banana", NULL, err);

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

  if (!TEST_CHECK(mutt_str_equal(value.data, apple_orig)))
  {
    TEST_MSG("Apple's initial value is wrong: '%s'\n", value.data);
    FREE(&value.data);
    return false;
  }
  TEST_MSG("Apple = '%s'\n", VarApple->mailbox);
  TEST_MSG("Apple's initial value is '%s'\n", value.data);

  mutt_buffer_reset(&value);
  rc = cs_str_initial_get(cs, "Banana", &value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  if (!TEST_CHECK(mutt_str_equal(value.data, banana_orig)))
  {
    TEST_MSG("Banana's initial value is wrong: '%s'\n", value.data);
    FREE(&value.data);
    return false;
  }
  TEST_MSG("Banana = '%s'\n", VarBanana ? VarBanana->mailbox : "");
  TEST_MSG("Banana's initial value is '%s'\n", NONULL(value.data));

  mutt_buffer_reset(&value);
  rc = cs_str_initial_set(cs, "Cherry", "john@doe.com", &value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  mutt_buffer_reset(&value);
  rc = cs_str_initial_set(cs, "Cherry", "jane@doe.com", &value);
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

  TEST_MSG("Cherry = '%s'\n", VarCherry ? VarCherry->mailbox : "");
  TEST_MSG("Cherry's initial value is '%s'\n", NONULL(value.data));

  FREE(&value.data);
  log_line(__func__);
  return true;
}

static bool test_string_set(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  const char *valid[] = { "hello@example.com", "world@example.com", NULL };
  const char *name = "Damson";
  char *addr = NULL;

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

    addr = VarDamson ? VarDamson->mailbox : NULL;
    if (!TEST_CHECK(mutt_str_equal(addr, valid[i])))
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      return false;
    }
    TEST_MSG("%s = '%s', set by '%s'\n", name, NONULL(addr), NONULL(valid[i]));
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

    addr = VarElderberry ? VarElderberry->mailbox : NULL;
    if (!TEST_CHECK(mutt_str_equal(addr, valid[i])))
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      return false;
    }
    TEST_MSG("%s = '%s', set by '%s'\n", name, NONULL(addr), NONULL(valid[i]));
  }

  log_line(__func__);
  return true;
}

static bool test_string_get(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  const char *name = "Fig";
  char *addr = NULL;

  mutt_buffer_reset(err);
  int rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", err->data);
    return false;
  }
  addr = VarFig ? VarFig->mailbox : NULL;
  TEST_MSG("%s = '%s', '%s'\n", name, NONULL(addr), err->data);

  name = "Guava";
  mutt_buffer_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", err->data);
    return false;
  }
  addr = VarGuava ? VarGuava->mailbox : NULL;
  TEST_MSG("%s = '%s', '%s'\n", name, NONULL(addr), err->data);

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
  addr = VarHawthorn ? VarHawthorn->mailbox : NULL;
  TEST_MSG("%s = '%s', '%s'\n", name, NONULL(addr), err->data);

  log_line(__func__);
  return true;
}

static bool test_native_set(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  struct Address *a = address_new("hello@example.com");
  const char *name = "Ilama";
  char *addr = NULL;
  bool result = false;

  mutt_buffer_reset(err);
  int rc = cs_str_native_set(cs, name, (intptr_t) a, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
    goto tbns_out;
  }

  addr = VarIlama ? VarIlama->mailbox : NULL;
  if (!TEST_CHECK(mutt_str_equal(addr, a->mailbox)))
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    goto tbns_out;
  }
  TEST_MSG("%s = '%s', set by '%s'\n", name, NONULL(addr), a->mailbox);

  name = "Jackfruit";
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, 0, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
    goto tbns_out;
  }

  if (!TEST_CHECK(VarJackfruit == NULL))
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    goto tbns_out;
  }
  addr = VarJackfruit ? VarJackfruit->mailbox : NULL;
  TEST_MSG("%s = '%s', set by NULL\n", name, NONULL(addr));

  log_line(__func__);
  result = true;
tbns_out:
  address_free(&a);
  return result;
}

static bool test_native_get(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  const char *name = "Kumquat";

  if (!TEST_CHECK(cs_str_string_set(cs, name, "kumquat@example.com", err) != INT_MIN))
    return false;

  mutt_buffer_reset(err);
  intptr_t value = cs_str_native_get(cs, name, err);
  struct Address *a = (struct Address *) value;

  if (!TEST_CHECK(VarKumquat == a))
  {
    TEST_MSG("Get failed: %s\n", err->data);
    return false;
  }
  char *addr1 = VarKumquat ? VarKumquat->mailbox : NULL;
  char *addr2 = a ? a->mailbox : NULL;
  TEST_MSG("%s = '%s', '%s'\n", name, NONULL(addr1), NONULL(addr2));

  log_line(__func__);
  return true;
}

static bool test_reset(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  const char *name = "Lemon";

  mutt_buffer_reset(err);

  char *addr = VarLemon ? VarLemon->mailbox : NULL;
  TEST_MSG("Initial: %s = '%s'\n", name, NONULL(addr));
  int rc = cs_str_string_set(cs, name, "hello@example.com", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  addr = VarLemon ? VarLemon->mailbox : NULL;
  TEST_MSG("Set: %s = '%s'\n", name, NONULL(addr));

  rc = cs_str_reset(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  addr = VarLemon ? VarLemon->mailbox : NULL;
  if (!TEST_CHECK(mutt_str_equal(addr, "lemon@example.com")))
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = '%s'\n", name, NONULL(addr));

  name = "Mango";
  mutt_buffer_reset(err);

  TEST_MSG("Initial: %s = '%s'\n", name, VarMango->mailbox);
  dont_fail = true;
  rc = cs_str_string_set(cs, name, "john@example.com", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  TEST_MSG("Set: %s = '%s'\n", name, VarMango->mailbox);
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

  if (!TEST_CHECK(mutt_str_equal(VarMango->mailbox, "john@example.com")))
  {
    TEST_MSG("Value of %s changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = '%s'\n", name, VarMango->mailbox);

  log_line(__func__);
  return true;
}

static bool test_validator(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  char *addr = NULL;
  struct Address *a = address_new("world@example.com");
  bool result = false;

  const char *name = "Nectarine";
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, name, "hello@example.com", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    goto tv_out;
  }
  addr = VarNectarine ? VarNectarine->mailbox : NULL;
  TEST_MSG("Address: %s = %s\n", name, NONULL(addr));

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP a, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    goto tv_out;
  }
  addr = VarNectarine ? VarNectarine->mailbox : NULL;
  TEST_MSG("Native: %s = %s\n", name, NONULL(addr));

  name = "Olive";
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "hello@example.com", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    goto tv_out;
  }
  addr = VarOlive ? VarOlive->mailbox : NULL;
  TEST_MSG("Address: %s = %s\n", name, NONULL(addr));

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP a, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    goto tv_out;
  }
  addr = VarOlive ? VarOlive->mailbox : NULL;
  TEST_MSG("Native: %s = %s\n", name, NONULL(addr));

  name = "Papaya";
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "hello@example.com", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    goto tv_out;
  }
  addr = VarPapaya ? VarPapaya->mailbox : NULL;
  TEST_MSG("Address: %s = %s\n", name, NONULL(addr));

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP a, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    goto tv_out;
  }
  addr = VarPapaya ? VarPapaya->mailbox : NULL;
  TEST_MSG("Native: %s = %s\n", name, NONULL(addr));

  result = true;
tv_out:
  address_free(&a);
  log_line(__func__);
  return result;
}

static void dump_native(struct ConfigSet *cs, const char *parent, const char *child)
{
  intptr_t pval = cs_str_native_get(cs, parent, NULL);
  intptr_t cval = cs_str_native_get(cs, child, NULL);

  struct Address *pa = (struct Address *) pval;
  struct Address *ca = (struct Address *) cval;

  char *pstr = pa ? pa->mailbox : NULL;
  char *cstr = ca ? ca->mailbox : NULL;

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
  int rc = cs_str_string_set(cs, parent, "hello@example.com", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // set child
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, child, "world@example.com", err);
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

void test_config_address(void)
{
  struct Buffer err;
  mutt_buffer_init(&err);
  err.dsize = 256;
  err.data = mutt_mem_calloc(1, err.dsize);
  mutt_buffer_reset(&err);

  struct ConfigSet *cs = cs_new(30);
  NeoMutt = neomutt_new(cs);

  address_init(cs);
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
