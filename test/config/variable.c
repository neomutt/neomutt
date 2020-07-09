/**
 * @file
 * Test code for the ConfigSet object
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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

// clang-format off
static struct ConfigDef Vars[] = {
  { "Apple",  DT_STRING, NULL, IP "hello", 0, NULL },
  { "Banana", DT_NUMBER, NULL, 42,         0, NULL },
  { NULL },
};
// clang-format on

void test_config_variable(void)
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

  number_init(cs);
  string_init(cs);

  if (!TEST_CHECK(cs_register_variables(cs, Vars, DT_NO_VARIABLE)))
    return;

  const char *name = "Apple";
  int result = cs_str_string_set(cs, name, "world", &err);
  if (!TEST_CHECK(CSR_RESULT(result) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", err.data);
    return;
  }

  mutt_buffer_reset(&err);
  result = cs_str_reset(cs, name, &err);
  if (!TEST_CHECK(CSR_RESULT(result) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", err.data);
    return;
  }

  struct HashElem *he = cs_get_elem(cs, name);
  if (!TEST_CHECK(he != NULL))
    return;

  mutt_buffer_reset(&err);
  result = cs_he_string_get(cs, he, &err);
  if (!TEST_CHECK(CSR_RESULT(result) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", err.data);
    return;
  }

  mutt_buffer_reset(&err);
  result = cs_he_native_set(cs, he, IP "foo", &err);
  if (!TEST_CHECK(CSR_RESULT(result) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", err.data);
    return;
  }

  mutt_buffer_reset(&err);
  result = cs_str_native_set(cs, name, IP "bar", &err);
  if (!TEST_CHECK(CSR_RESULT(result) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", err.data);
    return;
  }

  mutt_buffer_reset(&err);
  intptr_t value = cs_he_native_get(cs, he, &err);
  if (!TEST_CHECK(mutt_str_equal((const char *) value, "bar")))
  {
    TEST_MSG("Error: %s\n", err.data);
    return;
  }

  name = "Banana";
  he = cs_get_elem(cs, name);
  if (!TEST_CHECK(he != NULL))
    return;

  result = cs_he_string_plus_equals(cs, he, "23", &err);
  if (!TEST_CHECK(CSR_RESULT(result) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", err.data);
    return;
  }

  result = cs_he_string_minus_equals(cs, he, "56", &err);
  if (!TEST_CHECK(CSR_RESULT(result) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", err.data);
    return;
  }

  cs_free(&cs);
  FREE(&err.data);
  log_line(__func__);
}
