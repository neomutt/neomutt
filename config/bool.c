/**
 * @file
 * Type representing a boolean
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
 * @page config-bool Type: Boolean
 *
 * Type representing a boolean.
 */

#include "config.h"
#include <stddef.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include "mutt/buffer.h"
#include "mutt/hash.h"
#include "mutt/logging.h"
#include "mutt/memory.h"
#include "mutt/string2.h"
#include "set.h"
#include "types.h"

/**
 * bool_values - Valid strings for creating a Bool
 *
 * These strings are case-insensitive.
 */
const char *bool_values[] = {
  "no", "yes", "n", "y", "false", "true", "0", "1", "off", "on", NULL,
};

/**
 * bool_string_set - Set a Bool by string
 * @param cs    Config items
 * @param var   Variable to set
 * @param cdef  Variable definition
 * @param value Value to set
 * @param err   Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 *
 * If var is NULL, then the config item's initial value will be set.
 */
static int bool_string_set(const struct ConfigSet *cs, void *var, struct ConfigDef *cdef,
                           const char *value, struct Buffer *err)
{
  if (!cs || !cdef || !value)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  int num = -1;
  for (size_t i = 0; bool_values[i]; i++)
  {
    if (mutt_str_strcasecmp(bool_values[i], value) == 0)
    {
      num = i % 2;
      break;
    }
  }

  if (num < 0)
  {
    mutt_buffer_printf(err, "Invalid boolean value: %s", value);
    return (CSR_ERR_INVALID | CSR_INV_TYPE);
  }

  if (var)
  {
    if (num == (*(bool *) var))
      return (CSR_SUCCESS | CSR_SUC_NO_CHANGE);

    if (cdef->validator)
    {
      int rc = cdef->validator(cs, cdef, (intptr_t) num, err);

      if (CSR_RESULT(rc) != CSR_SUCCESS)
        return (rc | CSR_INV_VALIDATOR);
    }

    *(bool *) var = num;
  }
  else
  {
    cdef->initial = num;
  }

  return CSR_SUCCESS;
}

/**
 * bool_string_get - Get a Bool as a string
 * @param cs     Config items
 * @param var    Variable to get
 * @param cdef   Variable definition
 * @param result Buffer for results or error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 *
 * If var is NULL, then the config item's initial value will be returned.
 */
static int bool_string_get(const struct ConfigSet *cs, void *var,
                           const struct ConfigDef *cdef, struct Buffer *result)
{
  if (!cs || !cdef)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  int index;

  if (var)
    index = *(bool *) var;
  else
    index = (int) cdef->initial;

  if (index > 1)
  {
    mutt_debug(1, "Variable has an invalid value: %d\n", index);
    return (CSR_ERR_INVALID | CSR_INV_TYPE);
  }

  mutt_buffer_addstr(result, bool_values[index]);
  return CSR_SUCCESS;
}

/**
 * bool_native_set - Set a Bool config item by bool
 * @param cs    Config items
 * @param var   Variable to set
 * @param cdef  Variable definition
 * @param value Bool value
 * @param err   Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
static int bool_native_set(const struct ConfigSet *cs, void *var,
                           const struct ConfigDef *cdef, intptr_t value, struct Buffer *err)
{
  if (!cs || !var || !cdef)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  if ((value < 0) || (value > 1))
  {
    mutt_buffer_printf(err, "Invalid boolean value: %ld", value);
    return (CSR_ERR_INVALID | CSR_INV_TYPE);
  }

  if (value == (*(bool *) var))
    return (CSR_SUCCESS | CSR_SUC_NO_CHANGE);

  if (cdef->validator)
  {
    int rc = cdef->validator(cs, cdef, value, err);

    if (CSR_RESULT(rc) != CSR_SUCCESS)
      return (rc | CSR_INV_VALIDATOR);
  }

  *(bool *) var = value;
  return CSR_SUCCESS;
}

/**
 * bool_native_get - Get a bool from a Bool config item
 * @param cs   Config items
 * @param var  Variable to get
 * @param cdef Variable definition
 * @param err  Buffer for error messages
 * @retval intptr_t Bool
 */
static intptr_t bool_native_get(const struct ConfigSet *cs, void *var,
                                const struct ConfigDef *cdef, struct Buffer *err)
{
  if (!cs || !var || !cdef)
    return INT_MIN; /* LCOV_EXCL_LINE */

  return *(bool *) var;
}

/**
 * bool_reset - Reset a Bool to its initial value
 * @param cs   Config items
 * @param var  Variable to reset
 * @param cdef Variable definition
 * @param err  Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
static int bool_reset(const struct ConfigSet *cs, void *var,
                      const struct ConfigDef *cdef, struct Buffer *err)
{
  if (!cs || !var || !cdef)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  if (cdef->initial == (*(bool *) var))
    return (CSR_SUCCESS | CSR_SUC_NO_CHANGE);

  if (cdef->validator)
  {
    int rc = cdef->validator(cs, cdef, cdef->initial, err);

    if (CSR_RESULT(rc) != CSR_SUCCESS)
      return (rc | CSR_INV_VALIDATOR);
  }

  *(bool *) var = cdef->initial;
  return CSR_SUCCESS;
}

/**
 * bool_init - Register the Bool config type
 * @param cs Config items
 */
void bool_init(struct ConfigSet *cs)
{
  const struct ConfigSetType cst_bool = {
    "boolean",
    bool_string_set,
    bool_string_get,
    bool_native_set,
    bool_native_get,
    bool_reset,
    NULL,
  };
  cs_register_type(cs, DT_BOOL, &cst_bool);
}

/**
 * bool_he_toggle - Toggle the value of a bool
 * @param cs  Config items
 * @param he  HashElem representing config item
 * @param err Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
int bool_he_toggle(struct ConfigSet *cs, struct HashElem *he, struct Buffer *err)
{
  if (!cs || !he || !he->data)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  if (DTYPE(he->type) != DT_BOOL)
    return CSR_ERR_CODE;

  const struct ConfigDef *cdef = he->data;
  char *var = cdef->var;

  char value = *var;
  if ((value < 0) || (value > 1))
  {
    mutt_buffer_printf(err, "Invalid boolean value: %ld", value);
    return (CSR_ERR_INVALID | CSR_INV_TYPE);
  }

  *(char *) var = !value;

  cs_notify_listeners(cs, he, he->key.strkey, CE_SET);
  return CSR_SUCCESS;
}

/**
 * bool_str_toggle - Toggle the value of a bool
 * @param cs   Config items
 * @param name Name of config item
 * @param err  Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
int bool_str_toggle(struct ConfigSet *cs, const char *name, struct Buffer *err)
{
  if (!cs || !name)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  struct HashElem *he = cs_get_elem(cs, name);
  if (!he)
  {
    mutt_buffer_printf(err, "Unknown var '%s'", name);
    return CSR_ERR_UNKNOWN;
  }

  return bool_he_toggle(cs, he, err);
}
