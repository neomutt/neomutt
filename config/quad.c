/**
 * @file
 * Type representing a quad-option
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
 * @page config-quad Type: Quad-option
 *
 * Type representing a quad-option.
 */

#include "config.h"
#include <stddef.h>
#include <limits.h>
#include <stdint.h>
#include "mutt/buffer.h"
#include "mutt/hash.h"
#include "mutt/logging.h"
#include "mutt/memory.h"
#include "mutt/string2.h"
#include "set.h"
#include "types.h"

/**
 * quad_values - Valid strings for creating a QuadValue
 *
 * These strings are case-insensitive.
 */
const char *quad_values[] = {
  "no", "yes", "ask-no", "ask-yes", NULL,
};

/**
 * quad_string_set - Set a Quad-option by string
 * @param cs    Config items
 * @param var   Variable to set
 * @param cdef  Variable definition
 * @param value Value to set
 * @param err   Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 *
 * If var is NULL, then the config item's initial value will be set.
 */
static int quad_string_set(const struct ConfigSet *cs, void *var, struct ConfigDef *cdef,
                           const char *value, struct Buffer *err)
{
  if (!cs || !cdef || !value)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  int num = -1;
  for (size_t i = 0; quad_values[i]; i++)
  {
    if (mutt_str_strcasecmp(quad_values[i], value) == 0)
    {
      num = i;
      break;
    }
  }

  if (num < 0)
  {
    mutt_buffer_printf(err, "Invalid quad value: %s", value);
    return (CSR_ERR_INVALID | CSR_INV_TYPE);
  }

  if (var)
  {
    if (num == (*(char *) var))
      return (CSR_SUCCESS | CSR_SUC_NO_CHANGE);

    if (cdef->validator)
    {
      int rc = cdef->validator(cs, cdef, (intptr_t) num, err);

      if (CSR_RESULT(rc) != CSR_SUCCESS)
        return (rc | CSR_INV_VALIDATOR);
    }

    *(char *) var = num;
  }
  else
  {
    cdef->initial = num;
  }

  return CSR_SUCCESS;
}

/**
 * quad_string_get - Get a Quad-option as a string
 * @param cs     Config items
 * @param var    Variable to get
 * @param cdef   Variable definition
 * @param result Buffer for results or error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 *
 * If var is NULL, then the config item's initial value will be returned.
 */
static int quad_string_get(const struct ConfigSet *cs, void *var,
                           const struct ConfigDef *cdef, struct Buffer *result)
{
  if (!cs || !cdef)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  unsigned int value;

  if (var)
    value = *(char *) var;
  else
    value = (int) cdef->initial;

  if (value >= (mutt_array_size(quad_values) - 1))
  {
    mutt_debug(1, "Variable has an invalid value: %d\n", value);
    return (CSR_ERR_INVALID | CSR_INV_TYPE);
  }

  mutt_buffer_addstr(result, quad_values[value]);
  return CSR_SUCCESS;
}

/**
 * quad_native_set - Set a Quad-option config item by int
 * @param cs    Config items
 * @param var   Variable to set
 * @param cdef  Variable definition
 * @param value Quad-option value
 * @param err   Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
static int quad_native_set(const struct ConfigSet *cs, void *var,
                           const struct ConfigDef *cdef, intptr_t value, struct Buffer *err)
{
  if (!cs || !var || !cdef)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  if ((value < 0) || (value >= (mutt_array_size(quad_values) - 1)))
  {
    mutt_buffer_printf(err, "Invalid quad value: %ld", value);
    return (CSR_ERR_INVALID | CSR_INV_TYPE);
  }

  if (value == (*(char *) var))
    return (CSR_SUCCESS | CSR_SUC_NO_CHANGE);

  if (cdef->validator)
  {
    int rc = cdef->validator(cs, cdef, value, err);

    if (CSR_RESULT(rc) != CSR_SUCCESS)
      return (rc | CSR_INV_VALIDATOR);
  }

  *(char *) var = value;
  return CSR_SUCCESS;
}

/**
 * quad_native_get - Get an int object from a Quad-option config item
 * @param cs   Config items
 * @param var  Variable to get
 * @param cdef Variable definition
 * @param err  Buffer for error messages
 * @retval intptr_t Quad-option value
 */
static intptr_t quad_native_get(const struct ConfigSet *cs, void *var,
                                const struct ConfigDef *cdef, struct Buffer *err)
{
  if (!cs || !var || !cdef)
    return INT_MIN; /* LCOV_EXCL_LINE */

  return *(char *) var;
}

/**
 * quad_reset - Reset a Quad-option to its initial value
 * @param cs   Config items
 * @param var  Variable to reset
 * @param cdef Variable definition
 * @param err  Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
static int quad_reset(const struct ConfigSet *cs, void *var,
                      const struct ConfigDef *cdef, struct Buffer *err)
{
  if (!cs || !var || !cdef)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  if (cdef->initial == (*(char *) var))
    return (CSR_SUCCESS | CSR_SUC_NO_CHANGE);

  if (cdef->validator)
  {
    int rc = cdef->validator(cs, cdef, cdef->initial, err);

    if (CSR_RESULT(rc) != CSR_SUCCESS)
      return (rc | CSR_INV_VALIDATOR);
  }

  *(char *) var = cdef->initial;
  return CSR_SUCCESS;
}

/**
 * quad_init - Register the Quad-option config type
 * @param cs Config items
 */
void quad_init(struct ConfigSet *cs)
{
  const struct ConfigSetType cst_quad = {
    "quad",
    quad_string_set,
    quad_string_get,
    quad_native_set,
    quad_native_get,
    quad_reset,
    NULL,
  };
  cs_register_type(cs, DT_QUAD, &cst_quad);
}

/**
 * quad_toggle - Toggle (invert) the value of a quad option
 *
 * By toggling the low bit, the following are swapped:
 * - #MUTT_NO    <--> #MUTT_YES
 * - #MUTT_ASKNO <--> #MUTT_ASKYES
 */
static int quad_toggle(int opt)
{
  return opt ^= 1;
}

/**
 * quad_he_toggle - Toggle the value of a quad
 * @param cs  Config items
 * @param he  HashElem representing config item
 * @param err Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 *
 * @sa quad_toggle()
 */
int quad_he_toggle(struct ConfigSet *cs, struct HashElem *he, struct Buffer *err)
{
  if (!cs || !he || !he->data)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  if (DTYPE(he->type) != DT_QUAD)
    return CSR_ERR_CODE;

  const struct ConfigDef *cdef = he->data;
  char *var = cdef->var;

  char value = *var;
  if ((value < 0) || (value >= (mutt_array_size(quad_values) - 1)))
  {
    mutt_buffer_printf(err, "Invalid quad value: %ld", value);
    return (CSR_ERR_INVALID | CSR_INV_TYPE);
  }

  *(char *) var = quad_toggle(value);

  cs_notify_listeners(cs, he, he->key.strkey, CE_SET);
  return CSR_SUCCESS;
}
