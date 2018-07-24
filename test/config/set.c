/**
 * @file
 * Test code for the ConfigSet object
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
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/buffer.h"
#include "mutt/memory.h"
#include "mutt/string2.h"
#include "config/bool.h"
#include "config/set.h"
#include "config/types.h"
#include "config/common.h"

static short VarApple;
static bool VarBanana;

// clang-format off
static struct ConfigDef Vars[] = {
  { "Apple",  DT_NUMBER,  0, &VarApple,  0, NULL },
  { "Banana", DT_BOOL,    0, &VarBanana, 1, NULL },
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

static int dummy_reset(const struct ConfigSet *cs, void *var,
                       const struct ConfigDef *cdef, struct Buffer *err)
{
  return CSR_ERR_CODE;
}

void dummy_destroy(const struct ConfigSet *cs, void *var, const struct ConfigDef *cdef)
{
}

bool set_test(void)
{
  log_line(__func__);

  struct Buffer err;
  mutt_buffer_init(&err);
  err.data = mutt_mem_calloc(1, STRING);
  err.dsize = STRING;
  mutt_buffer_reset(&err);

  struct ConfigSet *cs = cs_create(30);
  if (!cs)
    return false;

  cs_add_listener(cs, log_listener);
  cs_add_listener(cs, log_listener); /* dupe */
  cs_remove_listener(cs, log_listener);
  cs_remove_listener(cs, log_listener); /* non-existant */

  const struct ConfigSetType cst_dummy = {
    "dummy", NULL, NULL, NULL, NULL, NULL, NULL,
  };

  if (!cs_register_type(cs, DT_STRING, &cst_dummy))
  {
    printf("Expected error\n");
  }
  else
  {
    printf("This test should have failed\n");
    return false;
  }

  const struct ConfigSetType cst_dummy2 = {
    "dummy2",         dummy_string_set, dummy_string_get, dummy_native_set,
    dummy_native_get, dummy_reset,      dummy_destroy,
  };

  if (!cs_register_type(cs, 25, &cst_dummy2))
  {
    printf("Expected error\n");
  }
  else
  {
    printf("This test should have failed\n");
    return false;
  }

  bool_init(cs);
  bool_init(cs); /* second one should fail */

  if (!cs_register_variables(cs, Vars, 0))
  {
    printf("Expected error\n");
  }
  else
  {
    printf("This test should have failed\n");
    return false;
  }

  const char *name = "Unknown";
  int result = cs_str_string_set(cs, name, "hello", &err);
  if (CSR_RESULT(result) == CSR_ERR_UNKNOWN)
  {
    printf("Expected error: Unknown var '%s'\n", name);
  }
  else
  {
    printf("This should have failed 1\n");
    return false;
  }

  result = cs_str_string_get(cs, name, &err);
  if (CSR_RESULT(result) == CSR_ERR_UNKNOWN)
  {
    printf("Expected error: Unknown var '%s'\n", name);
  }
  else
  {
    printf("This should have failed 2\n");
    return false;
  }

  result = cs_str_native_set(cs, name, IP "hello", &err);
  if (CSR_RESULT(result) == CSR_ERR_UNKNOWN)
  {
    printf("Expected error: Unknown var '%s'\n", name);
  }
  else
  {
    printf("This should have failed 3\n");
    return false;
  }

  intptr_t native = cs_str_native_get(cs, name, &err);
  if (native == INT_MIN)
  {
    printf("Expected error: Unknown var '%s'\n", name);
  }
  else
  {
    printf("This should have failed 4\n");
    return false;
  }

  struct HashElem *he = cs_get_elem(cs, "Banana");
  if (!he)
    return false;

  set_list(cs);

  const struct ConfigSetType *cst = cs_get_type_def(cs, 15);
  if (cst)
    return false;

  cs_free(&cs);
  FREE(&err.data);

  return true;
}
