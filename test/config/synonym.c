/**
 * @file
 * Test code for Config Synonyms
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
  { "Apple",      DT_STRING,  0,               0, NULL, },
  { "Banana",     DT_SYNONYM, IP "Apple",      0, NULL, },
  { "Cherry",     DT_STRING,  IP "cherry",     0, NULL, },
  { "Damson",     DT_SYNONYM, IP "Cherry",     0, NULL, },
  { "Elderberry", DT_STRING,  0,               0, NULL, },
  { "Fig",        DT_SYNONYM, IP "Elderberry", 0, NULL, },
  { "Guava",      DT_STRING,  0,               0, NULL, },
  { "Hawthorn",   DT_SYNONYM, IP "Guava",      0, NULL, },
  { "Ilama",      DT_STRING,  IP "iguana",     0, NULL, },
  { "Jackfruit",  DT_SYNONYM, IP "Ilama",      0, NULL, },
  { NULL },
};

static struct ConfigDef Vars2[] = {
  { "Jackfruit",  DT_SYNONYM, IP "Broken",     0, NULL },
  { NULL },
};
// clang-format on

static bool test_string_set(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);

  struct ConfigSet *cs = sub->cs;
  const char *name = "Banana";
  const char *value = "pudding";

  buf_reset(err);
  int rc = cs_str_string_set(cs, name, value, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }

  const char *VarApple = cs_subset_string(sub, "Apple");
  if (!TEST_CHECK_STR_EQ(VarApple, value))
  {
    TEST_MSG("Value of %s wasn't changed", name);
    return false;
  }
  TEST_MSG("%s = %s, set by '%s'", name, NONULL(VarApple), value);

  return true;
}

static bool test_string_get(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  const char *name = "Damson";
  struct ConfigSet *cs = sub->cs;

  buf_reset(err);
  int rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s", buf_string(err));
    return false;
  }
  const char *VarCherry = cs_subset_string(sub, "Cherry");
  TEST_MSG("%s = '%s', '%s'", name, NONULL(VarCherry), buf_string(err));

  return true;
}

static bool test_native_set(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);

  const char *name = "Fig";
  const char *value = "tree";
  struct ConfigSet *cs = sub->cs;

  buf_reset(err);
  int rc = cs_str_native_set(cs, name, (intptr_t) value, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }

  const char *VarElderberry = cs_subset_string(sub, "Elderberry");
  if (!TEST_CHECK_STR_EQ(VarElderberry, value))
  {
    TEST_MSG("Value of %s wasn't changed", name);
    return false;
  }
  TEST_MSG("%s = %s, set by '%s'", name, NONULL(VarElderberry), value);

  return true;
}

static bool test_native_get(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  const char *name = "Hawthorn";
  struct ConfigSet *cs = sub->cs;

  int rc = cs_str_string_set(cs, name, "tree", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;

  buf_reset(err);
  intptr_t value = cs_str_native_get(cs, name, err);
  const char *VarGuava = cs_subset_string(sub, "Guava");
  if (!TEST_CHECK_STR_EQ(VarGuava, (const char *) value))
  {
    TEST_MSG("Get failed: %s", buf_string(err));
    return false;
  }
  TEST_MSG("%s = '%s', '%s'", name, VarGuava, (const char *) value);

  return true;
}

static bool test_reset(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);

  struct ConfigSet *cs = sub->cs;
  const char *name = "Jackfruit";
  buf_reset(err);

  const char *VarIlama = cs_subset_string(sub, "Ilama");
  TEST_MSG("Initial: %s = '%s'", name, NONULL(VarIlama));
  int rc = cs_str_string_set(cs, name, "hello", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  TEST_MSG("Set: %s = '%s'", name, VarIlama);

  buf_reset(err);
  rc = cs_str_reset(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s", buf_string(err));
    return false;
  }

  VarIlama = cs_subset_string(sub, "Ilama");
  if (!TEST_CHECK_STR_EQ(VarIlama, "iguana"))
  {
    TEST_MSG("Value of %s wasn't changed", name);
    return false;
  }

  TEST_MSG("Reset: %s = '%s'", name, VarIlama);

  return true;
}

void test_config_synonym(void)
{
  log_line(__func__);

  struct ConfigSubset *sub = NeoMutt->sub;
  struct ConfigSet *cs = sub->cs;

  if (!TEST_CHECK(cs_register_variables(cs, Vars, DT_NO_FLAGS)))
    return;

  if (cs_register_variables(cs, Vars2, DT_NO_FLAGS))
  {
    TEST_MSG("Test should have failed");
    return;
  }

  TEST_MSG("Expected error");

  notify_observer_add(NeoMutt->notify, NT_CONFIG, log_observer, 0);

  set_list(cs);

  struct Buffer *err = buf_pool_get();
  TEST_CHECK(test_string_set(sub, err));
  TEST_CHECK(test_string_get(sub, err));
  TEST_CHECK(test_native_set(sub, err));
  TEST_CHECK(test_native_get(sub, err));
  TEST_CHECK(test_reset(sub, err));
  buf_pool_release(&err);
}
