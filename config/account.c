/**
 * @file
 * A collection of account-specific config items
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

/**
 * @page config-account Account-specific config items
 *
 * A collection of account-specific config items.
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include "mutt/buffer.h"
#include "mutt/hash.h"
#include "mutt/logging.h"
#include "mutt/memory.h"
#include "mutt/string2.h"
#include "account.h"
#include "inheritance.h"
#include "set.h"
#include "types.h"

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
