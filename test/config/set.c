/**
 * @file
 * Test code for the ConfigSet object
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

static short VarApple;
static bool VarBanana;

// clang-format off
static struct ConfigDef Vars[] = {
  { "Apple",  DT_NUMBER,  &VarApple,  0, 0, NULL },
  { "Banana", DT_BOOL,    &VarBanana, 1, 0, NULL },
  { NULL },
};
// clang-format on

static int dummy_string_set(const struct ConfigSet *cs, void *var, struct ConfigDef *cdef,
                            const char *value, struct Buffer *err)
{
  return CSR_ERR_CODE;
}

static int dummy_string_get(const struct ConfigSet *cs, void *var,
                            const struct ConfigDef *cdef, struct Buffer *result)
{
  return CSR_ERR_CODE;
}

static int dummy_native_set(const struct ConfigSet *cs, void *var,
                            const struct ConfigDef *cdef, intptr_t value, struct Buffer *err)
{
  return CSR_ERR_CODE;
}

static intptr_t dummy_native_get(const struct ConfigSet *cs, void *var,
                                 const struct ConfigDef *cdef, struct Buffer *err)
{
  return INT_MIN;
}

int dummy_plus_equals(const struct ConfigSet *cs, void *var, const struct ConfigDef *cdef,
                      const char *value, struct Buffer *err)
{
  return CSR_ERR_CODE;
}

int dummy_minus_equals(const struct ConfigSet *cs, void *var, const struct ConfigDef *cdef,
                       const char *value, struct Buffer *err)
{
  return CSR_ERR_CODE;
}

static int dummy_reset(const struct ConfigSet *cs, void *var,
                       const struct ConfigDef *cdef, struct Buffer *err)
{
  return CSR_ERR_CODE;
}

void dummy_destroy(const struct ConfigSet *cs, void *var, const struct ConfigDef *cdef)
{
}

bool degenerate_tests(struct ConfigSet *cs)
{
  const struct ConfigSetType cst_dummy = {
    "dummy", NULL, NULL, NULL, NULL, NULL, NULL,
  };

  struct HashElem *he = cs_get_elem(cs, "Banana");

  cs_free(NULL);
  TEST_CHECK_(1, "cs_free(NULL)");

  if (!TEST_CHECK(cs_register_type(NULL, DT_NUMBER, &cst_dummy) == false))
    return false;
  if (!TEST_CHECK(cs_register_type(cs, DT_NUMBER, NULL) == false))
    return false;
  if (!TEST_CHECK(cs_register_variables(cs, NULL, 0) == false))
    return false;
  if (!TEST_CHECK(cs_register_variables(NULL, Vars, 0) == false))
    return false;

  if (!TEST_CHECK(cs_str_native_get(NULL, "apple", NULL) == INT_MIN))
    return false;
  if (!TEST_CHECK(cs_str_native_get(cs, NULL, NULL) == INT_MIN))
    return false;

  if (!TEST_CHECK(cs_get_elem(NULL, "apple") == NULL))
    return false;
  if (!TEST_CHECK(cs_get_elem(cs, NULL) == NULL))
    return false;
  if (!TEST_CHECK(cs_get_type_def(NULL, DT_NUMBER) == NULL))
    return false;
  if (!TEST_CHECK(cs_get_type_def(cs, 30) == NULL))
    return false;
  if (!TEST_CHECK(cs_inherit_variable(NULL, he, "apple") == NULL))
    return false;
  if (!TEST_CHECK(cs_inherit_variable(cs, NULL, "apple") == NULL))
    return false;
  struct ConfigSet cs2 = { 0 };
  if (!TEST_CHECK(cs_inherit_variable(&cs2, he, "apple") == NULL))
    return false;

  cs_uninherit_variable(NULL, "apple");
  cs_uninherit_variable(cs, NULL);

  if (!TEST_CHECK(cs_str_native_set(NULL, "apple", IP "hello", NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_str_native_set(cs, NULL, IP "hello", NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_he_reset(NULL, he, NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_he_reset(cs, NULL, NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_str_reset(NULL, "apple", NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_str_reset(cs, NULL, NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_he_initial_set(NULL, he, "42", NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_he_initial_set(cs, NULL, "42", NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_he_initial_set(cs, he, "42", NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_str_initial_set(NULL, "apple", "42", NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_str_initial_set(cs, NULL, "42", NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_str_initial_set(cs, "unknown", "42", NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_str_initial_get(NULL, "apple", NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_str_initial_get(cs, NULL, NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_str_initial_get(cs, "unknown", NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_he_initial_get(NULL, he, NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_he_initial_get(cs, NULL, NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_he_string_set(NULL, he, "42", NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_he_string_set(cs, NULL, "42", NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_str_string_set(NULL, "apple", "42", NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_str_string_set(cs, NULL, "42", NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_he_string_get(NULL, he, NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_he_string_get(cs, NULL, NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_str_string_get(NULL, "apple", NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_str_string_get(cs, NULL, NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_he_native_set(NULL, he, 42, NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_he_native_set(cs, NULL, 42, NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_str_native_set(NULL, "apple", 42, NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_str_native_set(cs, NULL, 42, NULL) != CSR_SUCCESS))
    return false;

  return true;
}

bool invalid_tests(struct ConfigSet *cs)
{
  struct HashElem *he = cs_get_elem(cs, "Banana");
  he->type = 30;

  if (!TEST_CHECK(cs_he_initial_set(cs, he, "42", NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_he_initial_get(cs, he, NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_he_string_set(cs, he, "42", NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_he_string_get(cs, he, NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_he_native_set(cs, he, 42, NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_he_native_get(cs, he, NULL) != CSR_SUCCESS))
    return false;
  if (!TEST_CHECK(cs_str_native_set(cs, "apple", 42, NULL) != CSR_SUCCESS))
    return false;

  return true;
}

void test_config_set(void)
{
  log_line(__func__);

  struct Buffer err;
  mutt_buffer_init(&err);
  err.dsize = 256;
  err.data = mutt_mem_calloc(1, err.dsize);
  mutt_buffer_reset(&err);

  struct ConfigSet *cs = cs_new(30);
  if (!TEST_CHECK(cs != NULL))
    return;

  NeoMutt = neomutt_new(cs);

  const struct ConfigSetType cst_dummy = {
    "dummy", NULL, NULL, NULL, NULL, NULL, NULL,
  };

  if (TEST_CHECK(!cs_register_type(cs, DT_STRING, &cst_dummy)))
  {
    TEST_MSG("Expected error\n");
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return;
  }

  const struct ConfigSetType cst_dummy2 = {
    "dummy2",           dummy_string_set, dummy_string_get,
    dummy_native_set,   dummy_native_get, dummy_plus_equals,
    dummy_minus_equals, dummy_reset,      dummy_destroy,
  };

  if (TEST_CHECK(!cs_register_type(cs, 25, &cst_dummy2)))
  {
    TEST_MSG("Expected error\n");
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return;
  }

  bool_init(cs);
  bool_init(cs); /* second one should fail */

  if (TEST_CHECK(!cs_register_variables(cs, Vars, 0)))
  {
    TEST_MSG("Expected error\n");
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return;
  }

  if (!degenerate_tests(cs))
    return;

  if (!invalid_tests(cs))
    return;

  const char *name = "Unknown";
  int result = cs_str_string_set(cs, name, "hello", &err);
  if (TEST_CHECK(CSR_RESULT(result) == CSR_ERR_UNKNOWN))
  {
    TEST_MSG("Expected error: Unknown var '%s'\n", name);
  }
  else
  {
    TEST_MSG("This should have failed 1\n");
    return;
  }

  result = cs_str_string_get(cs, name, &err);
  if (TEST_CHECK(CSR_RESULT(result) == CSR_ERR_UNKNOWN))
  {
    TEST_MSG("Expected error: Unknown var '%s'\n", name);
  }
  else
  {
    TEST_MSG("This should have failed 2\n");
    return;
  }

  result = cs_str_native_set(cs, name, IP "hello", &err);
  if (TEST_CHECK(CSR_RESULT(result) == CSR_ERR_UNKNOWN))
  {
    TEST_MSG("Expected error: Unknown var '%s'\n", name);
  }
  else
  {
    TEST_MSG("This should have failed 3\n");
    return;
  }

  intptr_t native = cs_str_native_get(cs, name, &err);
  if (TEST_CHECK(native == INT_MIN))
  {
    TEST_MSG("Expected error: Unknown var '%s'\n", name);
  }
  else
  {
    TEST_MSG("This should have failed 4\n");
    return;
  }

  struct HashElem *he = cs_get_elem(cs, "Banana");
  if (!TEST_CHECK(he != NULL))
    return;

  set_list(cs);

  const struct ConfigSetType *cst = cs_get_type_def(cs, 15);
  if (!TEST_CHECK(!cst))
    return;

  neomutt_free(&NeoMutt);
  cs_free(&cs);
  FREE(&err.data);
  log_line(__func__);
}
