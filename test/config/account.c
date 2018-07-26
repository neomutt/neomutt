/**
 * @file
 * Test code for the Account object
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
#include "acutest.h"
#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/buffer.h"
#include "mutt/hash.h"
#include "mutt/logging.h"
#include "mutt/memory.h"
#include "mutt/string2.h"
#include "config/account.h"
#include "config/common.h"
#include "config/inheritance.h"
#include "config/number.h"
#include "config/set.h"
#include "config/types.h"

static short VarApple;
static short VarBanana;
static short VarCherry;

// clang-format off
static struct ConfigDef Vars[] = {
  { "Apple",  DT_NUMBER, 0, &VarApple,  0, NULL },
  { "Banana", DT_NUMBER, 0, &VarBanana, 0, NULL },
  { "Cherry", DT_NUMBER, 0, &VarCherry, 0, NULL },
  { NULL },
};
// clang-format on

/**
 * ac_create - Create an Account
 * @param cs        Config items
 * @param name      Name of Account
 * @param var_names List of config items (NULL terminated)
 * @retval ptr New Account object
 */
struct Account *ac_create(const struct ConfigSet *cs, const char *name,
                          const char *var_names[])
{
  if (!cs || !name || !var_names)
    return NULL; /* LCOV_EXCL_LINE */

  int count = 0;
  for (; var_names[count]; count++)
    ;

  struct Account *ac = mutt_mem_calloc(1, sizeof(*ac));
  ac->name = mutt_str_strdup(name);
  ac->cs = cs;
  ac->var_names = var_names;
  ac->vars = mutt_mem_calloc(count, sizeof(struct HashElem *));
  ac->num_vars = count;

  bool success = true;
  char acname[64];

  for (size_t i = 0; i < ac->num_vars; i++)
  {
    struct HashElem *parent = cs_get_elem(cs, ac->var_names[i]);
    if (!parent)
    {
      mutt_debug(1, "%s doesn't exist\n", ac->var_names[i]);
      success = false;
      break;
    }

    snprintf(acname, sizeof(acname), "%s:%s", name, ac->var_names[i]);
    ac->vars[i] = cs_inherit_variable(cs, parent, acname);
    if (!ac->vars[i])
    {
      mutt_debug(1, "failed to create %s\n", acname);
      success = false;
      break;
    }
  }

  if (success)
    return ac;

  ac_free(cs, &ac);
  return NULL;
}

/**
 * ac_free - Free an Account object
 * @param cs Config items
 * @param ac Account to free
 */
void ac_free(const struct ConfigSet *cs, struct Account **ac)
{
  if (!cs || !ac || !*ac)
    return; /* LCOV_EXCL_LINE */

  char child[128];
  struct Buffer err;
  mutt_buffer_init(&err);
  err.data = mutt_mem_calloc(1, STRING);
  err.dsize = STRING;

  for (size_t i = 0; i < (*ac)->num_vars; i++)
  {
    snprintf(child, sizeof(child), "%s:%s", (*ac)->name, (*ac)->var_names[i]);
    mutt_buffer_reset(&err);
    int result = cs_str_reset(cs, child, &err);
    if (CSR_RESULT(result) != CSR_SUCCESS)
      mutt_debug(1, "reset failed for %s: %s\n", child, err.data);
    mutt_hash_delete(cs->hash, child, NULL);
  }

  FREE(&err.data);
  FREE(&(*ac)->name);
  FREE(&(*ac)->vars);
  FREE(ac);
}

/**
 * ac_set_value - Set an Account-specific config item
 * @param ac    Account-specific config items
 * @param vid   Value ID (index into Account's HashElem's)
 * @param value Native pointer/value to set
 * @param err   Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
int ac_set_value(const struct Account *ac, size_t vid, intptr_t value, struct Buffer *err)
{
  if (!ac)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */
  if (vid >= ac->num_vars)
    return CSR_ERR_UNKNOWN;

  struct HashElem *he = ac->vars[vid];
  return cs_he_native_set(ac->cs, he, value, err);
}

/**
 * ac_get_value - Get an Account-specific config item
 * @param ac     Account-specific config items
 * @param vid    Value ID (index into Account's HashElem's)
 * @param result Buffer for results or error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
int ac_get_value(const struct Account *ac, size_t vid, struct Buffer *result)
{
  if (!ac)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */
  if (vid >= ac->num_vars)
    return CSR_ERR_UNKNOWN;

  struct HashElem *he = ac->vars[vid];

  if ((he->type & DT_INHERITED) && (DTYPE(he->type) == 0))
  {
    struct Inheritance *i = he->data;
    he = i->parent;
  }

  return cs_he_string_get(ac->cs, he, result);
}

void config_account(void)
{
  log_line(__func__);

  struct Buffer err;
  mutt_buffer_init(&err);
  err.data = mutt_mem_calloc(1, STRING);
  err.dsize = STRING;
  mutt_buffer_reset(&err);

  struct ConfigSet *cs = cs_create(30);

  number_init(cs);
  if (!cs_register_variables(cs, Vars, 0))
    return;

  set_list(cs);

  cs_add_listener(cs, log_listener);

  const char *account = "damaged";
  const char *BrokenVarStr[] = {
    "Pineapple", NULL,
  };

  struct Account *ac = ac_create(cs, account, BrokenVarStr);
  if (!ac)
  {
    TEST_MSG("Expected error:\n");
  }
  else
  {
    ac_free(cs, &ac);
    TEST_MSG("This test should have failed\n");
    return;
  }

  const char *AccountVarStr2[] = {
    "Apple", "Apple", NULL,
  };

  TEST_MSG("Expect error for next test\n");
  ac = ac_create(cs, account, AccountVarStr2);
  if (ac)
  {
    ac_free(cs, &ac);
    TEST_MSG("This test should have failed\n");
    return;
  }

  account = "fruit";
  const char *AccountVarStr[] = {
    "Apple", "Cherry", NULL,
  };

  ac = ac_create(cs, account, AccountVarStr);
  if (!ac)
    return;

  size_t index = 0;
  mutt_buffer_reset(&err);
  int rc = ac_set_value(ac, index, 33, &err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err.data);
  }

  mutt_buffer_reset(&err);
  rc = ac_set_value(ac, 99, 42, &err);
  if (CSR_RESULT(rc) == CSR_ERR_UNKNOWN)
  {
    TEST_MSG("Expected error: %s\n", err.data);
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return;
  }

  mutt_buffer_reset(&err);
  rc = ac_get_value(ac, index, &err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err.data);
  }
  else
  {
    TEST_MSG("%s = %s\n", AccountVarStr[index], err.data);
  }

  index++;
  mutt_buffer_reset(&err);
  rc = ac_get_value(ac, index, &err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err.data);
  }
  else
  {
    TEST_MSG("%s = %s\n", AccountVarStr[index], err.data);
  }

  mutt_buffer_reset(&err);
  rc = ac_get_value(ac, 99, &err);
  if (CSR_RESULT(rc) == CSR_ERR_UNKNOWN)
  {
    TEST_MSG("Expected error\n");
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return;
  }

  const char *name = "fruit:Apple";
  mutt_buffer_reset(&err);
  int result = cs_str_string_get(cs, name, &err);
  if (CSR_RESULT(result) == CSR_SUCCESS)
  {
    TEST_MSG("%s = '%s'\n", name, err.data);
  }
  else
  {
    TEST_MSG("%s\n", err.data);
    return;
  }

  mutt_buffer_reset(&err);
  result = cs_str_native_set(cs, name, 42, &err);
  if (CSR_RESULT(result) == CSR_SUCCESS)
  {
    TEST_MSG("Set %s\n", name);
  }
  else
  {
    TEST_MSG("%s\n", err.data);
    return;
  }

  struct HashElem *he = cs_get_elem(cs, name);
  if (!he)
    return;

  mutt_buffer_reset(&err);
  result = cs_str_initial_set(cs, name, "42", &err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Expected error\n");
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return;
  }

  mutt_buffer_reset(&err);
  result = cs_str_initial_get(cs, name, &err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("Expected error\n");
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return;
  }

  name = "Apple";
  he = cs_get_elem(cs, name);
  if (!he)
    return;

  mutt_buffer_reset(&err);
  result = cs_he_native_set(cs, he, 42, &err);
  if (CSR_RESULT(result) == CSR_SUCCESS)
  {
    TEST_MSG("Set %s\n", name);
  }
  else
  {
    TEST_MSG("%s\n", err.data);
    return;
  }

  ac_free(cs, &ac);
  cs_free(&cs);
  FREE(&err.data);
}
