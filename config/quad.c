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
 * @page config_quad Type: Quad-option
 *
 * Config type representing a quad-option.
 *
 * - Backed by `unsigned char`
 * - Validator is passed `unsigned char`
 * - Valid user entry: #QuadValues
 */

#include "config.h"
#include <stddef.h>
#include <limits.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "quad.h"
#include "set.h"
#include "subset.h"
#include "types.h"

/**
 * QuadValues - Valid strings for creating a QuadValue
 *
 * These strings are case-insensitive.
 */
const char *QuadValues[] = {
  "no", "yes", "ask-no", "ask-yes", NULL,
};

/**
 * quad_string_set - Set a Quad-option by string - Implements ConfigSetType::string_set()
 */
static int quad_string_set(const struct ConfigSet *cs, void *var, struct ConfigDef *cdef,
                           const char *value, struct Buffer *err)
{
  if (!value)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  int num = -1;
  for (size_t i = 0; QuadValues[i]; i++)
  {
    if (mutt_istr_equal(QuadValues[i], value))
    {
      num = i;
      break;
    }
  }

  if (num < 0)
  {
    mutt_buffer_printf(err, _("Invalid quad value: %s"), value);
    return CSR_ERR_INVALID | CSR_INV_TYPE;
  }

  if (var)
  {
    if (num == (*(char *) var))
      return CSR_SUCCESS | CSR_SUC_NO_CHANGE;

    if (cdef->validator)
    {
      int rc = cdef->validator(cs, cdef, (intptr_t) num, err);

      if (CSR_RESULT(rc) != CSR_SUCCESS)
        return rc | CSR_INV_VALIDATOR;
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
 * quad_string_get - Get a Quad-option as a string - Implements ConfigSetType::string_get()
 */
static int quad_string_get(const struct ConfigSet *cs, void *var,
                           const struct ConfigDef *cdef, struct Buffer *result)
{
  unsigned int value;

  if (var)
    value = *(char *) var;
  else
    value = (int) cdef->initial;

  if (value >= (mutt_array_size(QuadValues) - 1))
  {
    mutt_debug(LL_DEBUG1, "Variable has an invalid value: %d\n", value);
    return CSR_ERR_INVALID | CSR_INV_TYPE;
  }

  mutt_buffer_addstr(result, QuadValues[value]);
  return CSR_SUCCESS;
}

/**
 * quad_native_set - Set a Quad-option config item by int - Implements ConfigSetType::native_set()
 */
static int quad_native_set(const struct ConfigSet *cs, void *var,
                           const struct ConfigDef *cdef, intptr_t value, struct Buffer *err)
{
  if ((value < 0) || (value >= (mutt_array_size(QuadValues) - 1)))
  {
    mutt_buffer_printf(err, _("Invalid quad value: %ld"), value);
    return CSR_ERR_INVALID | CSR_INV_TYPE;
  }

  if (value == (*(char *) var))
    return CSR_SUCCESS | CSR_SUC_NO_CHANGE;

  if (cdef->validator)
  {
    int rc = cdef->validator(cs, cdef, value, err);

    if (CSR_RESULT(rc) != CSR_SUCCESS)
      return rc | CSR_INV_VALIDATOR;
  }

  *(char *) var = value;
  return CSR_SUCCESS;
}

/**
 * quad_native_get - Get an int object from a Quad-option config item - Implements ConfigSetType::native_get()
 */
static intptr_t quad_native_get(const struct ConfigSet *cs, void *var,
                                const struct ConfigDef *cdef, struct Buffer *err)
{
  return *(char *) var;
}

/**
 * quad_reset - Reset a Quad-option to its initial value - Implements ConfigSetType::reset()
 */
static int quad_reset(const struct ConfigSet *cs, void *var,
                      const struct ConfigDef *cdef, struct Buffer *err)
{
  if (cdef->initial == (*(char *) var))
    return CSR_SUCCESS | CSR_SUC_NO_CHANGE;

  if (cdef->validator)
  {
    int rc = cdef->validator(cs, cdef, cdef->initial, err);

    if (CSR_RESULT(rc) != CSR_SUCCESS)
      return rc | CSR_INV_VALIDATOR;
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
    NULL, // string_plus_equals
    NULL, // string_minus_equals
    quad_reset,
    NULL, // destroy
  };
  cs_register_type(cs, DT_QUAD, &cst_quad);
}

/**
 * quad_toggle - Toggle (invert) the value of a quad option
 * @param opt Value to toggle
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
 * @param sub Config subset
 * @param he  HashElem representing config item
 * @param err Buffer for error messages
 * @retval num Result, e.g. #CSR_SUCCESS
 *
 * @sa quad_toggle()
 */
int quad_he_toggle(struct ConfigSubset *sub, struct HashElem *he, struct Buffer *err)
{
  if (!sub || !he || !he->data)
    return CSR_ERR_CODE;

  struct HashElem *he_base = cs_get_base(he);
  if (DTYPE(he_base->type) != DT_QUAD)
    return CSR_ERR_CODE;

  intptr_t value = cs_he_native_get(sub->cs, he, err);
  if (value == INT_MIN)
    return CSR_ERR_CODE;

  value = quad_toggle(value);
  int rc = cs_he_native_set(sub->cs, he, value, err);

  if ((CSR_RESULT(rc) == CSR_SUCCESS) && !(rc & CSR_SUC_NO_CHANGE))
    cs_subset_notify_observers(sub, he, NT_CONFIG_SET);

  return rc;
}

/**
 * quad_str_toggle - Toggle the value of a quad
 * @param sub  Config subset
 * @param name HashElem representing config item
 * @param err  Buffer for error messages
 * @retval num Result, e.g. #CSR_SUCCESS
 *
 * @sa quad_toggle()
 */
int quad_str_toggle(struct ConfigSubset *sub, const char *name, struct Buffer *err)
{
  struct HashElem *he = cs_subset_create_inheritance(sub, name);

  return quad_he_toggle(sub, he, err);
}
