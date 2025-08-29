/**
 * @file
 * Test code for the Regex object
 *
 * @authors
 * Copyright (C) 2018-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Pietro Cerutti <gahr@gahr.ch>
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
static struct ConfigDef Vars[] = {
  { "Apple",      DT_REGEX,                    IP "apple.*",      0, NULL,              }, /* test_initial_values */
  { "Banana",     DT_REGEX,                    IP "banana.*",     0, NULL,              },
  { "Cherry",     DT_REGEX,                    IP "cherry.*",     0, NULL,              },
  { "Damson",     DT_REGEX,                    0,                 0, NULL,              }, /* test_regex_set */
  { "Elderberry", DT_REGEX|D_REGEX_NOSUB,      IP "elderberry.*", 0, NULL,              },
  { "Fig",        DT_REGEX,                    0,                 0, NULL,              }, /* test_regex_get */
  { "Guava",      DT_REGEX,                    IP "guava.*",      0, NULL,              },
  { "Hawthorn",   DT_REGEX,                    0,                 0, NULL,              },
  { "Ilama",      DT_REGEX|D_REGEX_ALLOW_NOT,  0,                 0, NULL,              }, /* test_native_set */
  { "Jackfruit",  DT_REGEX,                    IP "jackfruit.*",  0, NULL,              },
  { "Kumquat",    DT_REGEX,                    IP "kumquat.*",    0, NULL,              },
  { "Lemon",      DT_REGEX,                    0,                 0, NULL,              }, /* test_native_get */
  { "Mango",      DT_REGEX,                    IP "mango.*",      0, NULL,              }, /* test_reset */
  { "Nectarine",  DT_REGEX,                    IP "[a-b",         0, NULL,              },
  { "Olive",      DT_REGEX,                    IP "olive.*",      0, validator_fail,    },
  { "Papaya",     DT_REGEX,                    IP "papaya.*",     0, validator_succeed, }, /* test_validator */
  { "Quince",     DT_REGEX,                    IP "quince.*",     0, validator_warn,    },
  { "Raspberry",  DT_REGEX,                    IP "raspberry.*",  0, validator_fail,    },
  { "Strawberry", DT_REGEX,                    0,                 0, NULL,              }, /* test_inherit */
  { "Tangerine",  DT_REGEX|D_ON_STARTUP,       IP "tangerine.*",  0, NULL,              }, /* startup */
  { NULL },
};
// clang-format on

static bool test_initial_values(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const struct Regex *VarApple = cs_subset_regex(sub, "Apple");
  const struct Regex *VarBanana = cs_subset_regex(sub, "Banana");

  TEST_MSG("Apple = %s", VarApple->pattern);
  TEST_MSG("Banana = %s", VarBanana->pattern);

  if (!TEST_CHECK_STR_EQ(VarApple->pattern, "apple.*"))
  {
    TEST_MSG("Error: initial values were wrong");
    return false;
  }

  if (!TEST_CHECK_STR_EQ(VarBanana->pattern, "banana.*"))
  {
    TEST_MSG("Error: initial values were wrong");
    return false;
  }

  cs_str_string_set(cs, "Apple", "car*", err);
  cs_str_string_set(cs, "Banana", "train*", err);

  VarApple = cs_subset_regex(sub, "Apple");
  VarBanana = cs_subset_regex(sub, "Banana");

  struct Buffer *value = buf_pool_get();

  int rc;

  buf_reset(value);
  rc = cs_str_initial_get(cs, "Apple", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(value));
    return false;
  }

  if (!TEST_CHECK_STR_EQ(buf_string(value), "apple.*"))
  {
    TEST_MSG("Apple's initial value is wrong: '%s'", buf_string(value));
    return false;
  }
  VarApple = cs_subset_regex(sub, "Apple");
  TEST_MSG("Apple = '%s'", VarApple ? VarApple->pattern : "");
  TEST_MSG("Apple's initial value is %s", buf_string(value));

  buf_reset(value);
  rc = cs_str_initial_get(cs, "Banana", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(value));
    return false;
  }

  if (!TEST_CHECK_STR_EQ(buf_string(value), "banana.*"))
  {
    TEST_MSG("Banana's initial value is wrong: %s", buf_string(value));
    return false;
  }
  VarBanana = cs_subset_regex(sub, "Banana");
  TEST_MSG("Banana = '%s'", VarBanana ? VarBanana->pattern : "");
  TEST_MSG("Banana's initial value is %s", NONULL(buf_string(value)));

  buf_reset(value);
  rc = cs_str_initial_set(cs, "Cherry", "up.*", value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(value));
    return false;
  }

  buf_reset(value);
  rc = cs_str_initial_set(cs, "Cherry", "down.*", value);
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

  const struct Regex *VarCherry = cs_subset_regex(sub, "Cherry");
  TEST_MSG("Cherry = '%s'", VarCherry->pattern);
  TEST_MSG("Cherry's initial value is '%s'", NONULL(buf_string(value)));

  buf_pool_release(&value);
  log_line(__func__);
  return true;
}

static bool test_string_set(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *valid[] = { "hello.*", "world.*", "world.*", "", NULL };
  const char *name = "Damson";
  char *regex = NULL;

  int rc;

  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
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

    const struct Regex *VarDamson = cs_subset_regex(sub, "Damson");
    regex = VarDamson ? VarDamson->pattern : NULL;
    if (!TEST_CHECK_STR_EQ(regex, valid[i]))
    {
      TEST_MSG("Value of %s wasn't changed", name);
      return false;
    }
    TEST_MSG("%s = '%s', set by '%s'", name, NONULL(regex), NONULL(valid[i]));
  }

  name = "Elderberry";
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
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

    const struct Regex *VarElderberry = cs_subset_regex(sub, "Elderberry");
    regex = VarElderberry ? VarElderberry->pattern : NULL;
    if (!TEST_CHECK_STR_EQ(regex, valid[i]))
    {
      TEST_MSG("Value of %s wasn't changed", name);
      return false;
    }
    TEST_MSG("%s = '%s', set by '%s'", name, NONULL(regex), NONULL(valid[i]));
  }

  buf_reset(err);
  rc = cs_str_string_set(cs, name, "[a-b", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }

  name = "Tangerine";
  rc = cs_str_string_set(cs, name, "tangerine.*", err);
  TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS);

  rc = cs_str_string_set(cs, name, "apple.*", err);
  TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS);

  log_line(__func__);
  return true;
}

static bool test_string_get(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;
  const char *name = "Fig";
  char *regex = NULL;

  buf_reset(err);
  int rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s", buf_string(err));
    return false;
  }
  const struct Regex *VarFig = cs_subset_regex(sub, "Fig");
  regex = VarFig ? VarFig->pattern : NULL;
  TEST_MSG("%s = '%s', '%s'", name, NONULL(regex), buf_string(err));

  name = "Guava";
  buf_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s", buf_string(err));
    return false;
  }
  const struct Regex *VarGuava = cs_subset_regex(sub, "Guava");
  regex = VarGuava ? VarGuava->pattern : NULL;
  TEST_MSG("%s = '%s', '%s'", name, NONULL(regex), buf_string(err));

  name = "Hawthorn";
  rc = cs_str_string_set(cs, name, "hawthorn", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;

  buf_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s", buf_string(err));
    return false;
  }
  const struct Regex *VarHawthorn = cs_subset_regex(sub, "Hawthorn");
  regex = VarHawthorn ? VarHawthorn->pattern : NULL;
  TEST_MSG("%s = '%s', '%s'", name, NONULL(regex), buf_string(err));

  log_line(__func__);
  return true;
}

static bool test_native_set(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  struct Regex *r = regex_new(NULL, 0, err);
  if (!TEST_CHECK(r == NULL))
  {
    TEST_MSG("regex_new() succeeded when is shouldn't have");
    return false;
  }

  r = regex_new("hello.*", D_REGEX_NOSUB, err);
  const char *name = "Ilama";
  char *regex = NULL;
  bool result = false;

  buf_reset(err);
  int rc = cs_str_native_set(cs, name, (intptr_t) r, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
    goto tns_out;
  }

  const struct Regex *VarIlama = cs_subset_regex(sub, "Ilama");
  regex = VarIlama ? VarIlama->pattern : NULL;
  if (!TEST_CHECK_STR_EQ(r->pattern, regex))
  {
    TEST_MSG("Value of %s wasn't changed", name);
    goto tns_out;
  }
  TEST_MSG("%s = '%s', set by '%s'", name, NONULL(regex), r->pattern);

  regex_free(&r);
  r = regex_new("!world.*", D_REGEX_ALLOW_NOT, err);
  name = "Ilama";

  buf_reset(err);
  rc = cs_str_native_set(cs, name, (intptr_t) r, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
    goto tns_out;
  }
  VarIlama = cs_subset_regex(sub, "Ilama");
  TEST_MSG("'%s', not flag set to %d", VarIlama->pattern, VarIlama->pat_not);

  name = "Jackfruit";
  buf_reset(err);
  rc = cs_str_native_set(cs, name, 0, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
    goto tns_out;
  }

  const struct Regex *VarJackfruit = cs_subset_regex(sub, "Jackfruit");
  if (!TEST_CHECK(VarJackfruit == NULL))
  {
    TEST_MSG("Value of %s wasn't changed", name);
    goto tns_out;
  }
  regex = VarJackfruit ? VarJackfruit->pattern : NULL;
  TEST_MSG("%s = '%s', set by NULL", name, NONULL(regex));

  regex_free(&r);
  r = regex_new("world.*", 0, err);
  r->pattern[0] = '[';
  r->pattern[1] = 'a';
  r->pattern[2] = '-';
  r->pattern[3] = 'b';
  name = "Kumquat";

  buf_reset(err);
  rc = cs_str_native_set(cs, name, (intptr_t) r, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    goto tns_out;
  }

  regex_free(&r);
  r = regex_new("tangerine.*", D_REGEX_NOSUB, err);

  name = "Tangerine";
  rc = cs_str_native_set(cs, name, (intptr_t) r, err);
  TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS);

  regex_free(&r);
  r = regex_new("apple.*", D_REGEX_NOSUB, err);

  rc = cs_str_native_set(cs, name, (intptr_t) r, err);
  TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS);

  log_line(__func__);
  result = true;
tns_out:
  regex_free(&r);
  return result;
}

static bool test_native_get(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;
  const char *name = "Lemon";

  int rc = cs_str_string_set(cs, name, "lemon.*", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;

  buf_reset(err);
  intptr_t value = cs_str_native_get(cs, name, err);
  struct Regex *r = (struct Regex *) value;

  const struct Regex *VarLemon = cs_subset_regex(sub, "Lemon");
  if (!TEST_CHECK(VarLemon == r))
  {
    TEST_MSG("Get failed: %s", buf_string(err));
    return false;
  }
  char *regex1 = VarLemon ? VarLemon->pattern : NULL;
  char *regex2 = r ? r->pattern : NULL;
  TEST_MSG("%s = '%s', '%s'", name, NONULL(regex1), NONULL(regex2));

  log_line(__func__);
  return true;
}

static bool test_reset(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  struct ConfigSet *cs = sub->cs;

  const char *name = "Mango";

  buf_reset(err);

  const struct Regex *VarMango = cs_subset_regex(sub, "Mango");
  char *regex = VarMango ? VarMango->pattern : NULL;
  TEST_MSG("Initial: %s = '%s'", name, NONULL(regex));
  int rc = cs_str_string_set(cs, name, "hello.*", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  VarMango = cs_subset_regex(sub, "Mango");
  regex = VarMango ? VarMango->pattern : NULL;
  TEST_MSG("Set: %s = '%s'", name, NONULL(regex));

  rc = cs_str_reset(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }

  VarMango = cs_subset_regex(sub, "Mango");
  regex = VarMango ? VarMango->pattern : NULL;
  if (!TEST_CHECK_STR_EQ(regex, "mango.*"))
  {
    TEST_MSG("Value of %s wasn't changed", name);
    return false;
  }

  TEST_MSG("Reset: %s = '%s'", name, NONULL(regex));

  rc = cs_str_reset(cs, "Nectarine", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }

  name = "Olive";
  buf_reset(err);

  const struct Regex *VarOlive = cs_subset_regex(sub, "Olive");
  TEST_MSG("Initial: %s = '%s'", name, VarOlive->pattern);
  dont_fail = true;
  rc = cs_str_string_set(cs, name, "hel*o", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  VarOlive = cs_subset_regex(sub, "Olive");
  TEST_MSG("Set: %s = '%s'", name, VarOlive->pattern);
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

  VarOlive = cs_subset_regex(sub, "Olive");
  if (!TEST_CHECK_STR_EQ(VarOlive->pattern, "hel*o"))
  {
    TEST_MSG("Value of %s changed", name);
    return false;
  }

  TEST_MSG("Reset: %s = '%s'", name, VarOlive->pattern);

  name = "Tangerine";
  rc = cs_str_reset(cs, name, err);
  TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS);

  StartupComplete = false;
  rc = cs_str_string_set(cs, name, "banana", err);
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

  char *regex = NULL;
  struct Regex *r = regex_new("world.*", 0, err);
  bool result = false;

  const char *name = "Papaya";
  buf_reset(err);
  int rc = cs_str_string_set(cs, name, "hello.*", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    goto tv_out;
  }
  const struct Regex *VarPapaya = cs_subset_regex(sub, "Papaya");
  regex = VarPapaya ? VarPapaya->pattern : NULL;
  TEST_MSG("Regex: %s = %s", name, NONULL(regex));

  buf_reset(err);
  rc = cs_str_native_set(cs, name, IP r, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    goto tv_out;
  }
  VarPapaya = cs_subset_regex(sub, "Papaya");
  regex = VarPapaya ? VarPapaya->pattern : NULL;
  TEST_MSG("Native: %s = %s", name, NONULL(regex));

  name = "Quince";
  buf_reset(err);
  rc = cs_str_string_set(cs, name, "hello.*", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    goto tv_out;
  }
  const struct Regex *VarQuince = cs_subset_regex(sub, "Quince");
  regex = VarQuince ? VarQuince->pattern : NULL;
  TEST_MSG("Regex: %s = %s", name, NONULL(regex));

  buf_reset(err);
  rc = cs_str_native_set(cs, name, IP r, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    goto tv_out;
  }
  VarQuince = cs_subset_regex(sub, "Quince");
  regex = VarQuince ? VarQuince->pattern : NULL;
  TEST_MSG("Native: %s = %s", name, NONULL(regex));

  name = "Raspberry";
  buf_reset(err);
  rc = cs_str_string_set(cs, name, "hello.*", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    goto tv_out;
  }
  const struct Regex *VarRaspberry = cs_subset_regex(sub, "Raspberry");
  regex = VarRaspberry ? VarRaspberry->pattern : NULL;
  TEST_MSG("Regex: %s = %s", name, NONULL(regex));

  buf_reset(err);
  rc = cs_str_native_set(cs, name, IP r, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s", buf_string(err));
  }
  else
  {
    TEST_MSG("%s", buf_string(err));
    goto tv_out;
  }
  VarRaspberry = cs_subset_regex(sub, "Raspberry");
  regex = VarRaspberry ? VarRaspberry->pattern : NULL;
  TEST_MSG("Native: %s = %s", name, NONULL(regex));

  result = true;
tv_out:
  regex_free(&r);
  log_line(__func__);
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

  TEST_MSG("%15s = %s", parent, NONULL(pstr));
  TEST_MSG("%15s = %s", child, NONULL(cstr));
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
  buf_reset(err);
  int rc = cs_str_string_set(cs, parent, "hello.*", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s", buf_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // set child
  buf_reset(err);
  rc = cs_str_string_set(cs, child, "world.*", err);
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

void test_config_regex(void)
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

  regex_equal(NULL, NULL);

  struct Buffer *err = buf_pool_get();
  TEST_CHECK(test_initial_values(sub, err));
  TEST_CHECK(test_string_set(sub, err));
  TEST_CHECK(test_string_get(sub, err));
  TEST_CHECK(test_native_set(sub, err));
  TEST_CHECK(test_native_get(sub, err));
  TEST_CHECK(test_reset(sub, err));
  TEST_CHECK(test_validator(sub, err));
  TEST_CHECK(test_inherit(cs, err));
  buf_pool_release(&err);
}
