/**
 * @file
 * Test code for pre-setting initial values
 *
 * @authors
 * Copyright (C) 2018-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
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
#include "config/lib.h"
#include "core/lib.h"
#include "common.h"      // IWYU pragma: keep
#include "test_common.h" // IWYU pragma: keep

// clang-format off
static struct ConfigDef Vars[] = {
  { "Apple",  DT_STRING, IP "apple", 0, NULL, },
  { "Banana", DT_STRING, 0,          0, NULL, },
  { "Cherry", DT_STRING, 0,          0, NULL, },
  { NULL },
};
// clang-format on

static bool test_set_initial(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  const char *name = NULL;
  struct ConfigSet *cs = sub->cs;

  name = "Apple";
  struct HashElem *he_a = cs_get_elem(cs, name);
  if (!TEST_CHECK(he_a != NULL))
    return false;

  const char *aval = "pie";
  int rc = cs_he_initial_set(cs, he_a, aval, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    TEST_MSG("Expected error: %s", buf_string(err));

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

  const char *VarApple = cs_subset_string(sub, "Apple");
  const char *VarBanana = cs_subset_string(sub, "Banana");
  const char *VarCherry = cs_subset_string(sub, "Cherry");

  TEST_MSG("Apple = %s", VarApple);
  TEST_MSG("Banana = %s", VarBanana);
  TEST_MSG("Cherry = %s", VarCherry);

  log_line(__func__);
  return (!mutt_str_equal(VarApple, aval) && !mutt_str_equal(VarBanana, bval) &&
          !mutt_str_equal(VarCherry, cval));
}

void test_config_initial(void)
{
  log_line(__func__);

  struct ConfigSubset *sub = NeoMutt->sub;
  struct ConfigSet *cs = sub->cs;

  if (!TEST_CHECK(cs_register_variables(cs, Vars)))
    return;

  notify_observer_add(NeoMutt->notify, NT_CONFIG, log_observer, 0);

  set_list(cs);

  struct Buffer *err = buf_pool_get();
  if (!TEST_CHECK(test_set_initial(sub, err)))
  {
    buf_pool_release(&err);
    return;
  }

  buf_pool_release(&err);
}
