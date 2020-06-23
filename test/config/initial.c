/**
 * @file
 * Test code for pre-setting initial values
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
#include <stdio.h>
#include "mutt/lib.h"
#include "config/common.h"
#include "config/lib.h"
#include "core/lib.h"

static char *VarApple;
static char *VarBanana;
static char *VarCherry;

// clang-format off
static struct ConfigDef Vars[] = {
  { "Apple",  DT_STRING, &VarApple,  IP "apple", 0, NULL },
  { "Banana", DT_STRING, &VarBanana, 0,          0, NULL },
  { "Cherry", DT_STRING, &VarCherry, 0,          0, NULL },
  { NULL },
};
// clang-format on

static bool test_set_initial(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  const char *name = NULL;

  name = "Apple";
  struct HashElem *he_a = cs_get_elem(cs, name);
  if (!TEST_CHECK(he_a != NULL))
    return false;

  const char *aval = "pie";
  int rc = cs_he_initial_set(cs, he_a, aval, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    TEST_MSG("Expected error: %s\n", err->data);

  name = "Banana";
  struct HashElem *he_b = cs_get_elem(cs, name);
  if (!TEST_CHECK(he_b != NULL))
    return false;

  const char *bval = "split";
  rc = cs_he_initial_set(cs, he_b, bval, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;

  name = "Cherry";
  struct HashElem *he_c = cs_get_elem(cs, name);
  if (!TEST_CHECK(he_c != NULL))
    return false;

  const char *cval = "blossom";
  rc = cs_str_initial_set(cs, name, cval, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;

  TEST_MSG("Apple = %s\n", VarApple);
  TEST_MSG("Banana = %s\n", VarBanana);
  TEST_MSG("Cherry = %s\n", VarCherry);

  log_line(__func__);
  return (!mutt_str_equal(VarApple, aval) && !mutt_str_equal(VarBanana, bval) &&
          !mutt_str_equal(VarCherry, cval));
}

void test_config_initial(void)
{
  log_line(__func__);

  struct Buffer err;
  mutt_buffer_init(&err);
  err.dsize = 256;
  err.data = mutt_mem_calloc(1, err.dsize);
  mutt_buffer_reset(&err);

  struct ConfigSet *cs = cs_new(30);
  NeoMutt = neomutt_new(cs);

  string_init(cs);
  if (!cs_register_variables(cs, Vars, 0))
    return;

  notify_observer_add(NeoMutt->notify, log_observer, 0);

  set_list(cs);

  if (!TEST_CHECK(test_set_initial(cs, &err)))
  {
    neomutt_free(&NeoMutt);
    cs_free(&cs);
    FREE(&err.data);
    return;
  }

  neomutt_free(&NeoMutt);
  cs_free(&cs);
  FREE(&err.data);
}
