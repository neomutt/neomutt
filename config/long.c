/**
 * @file
 * Type representing a long
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
 * @page config_long Type: Long
 *
 * Config type representing a long.
 *
 * - Backed by `long`
 * - Validator is passed `long`
 */

#include "config.h"
#include <stddef.h>
#include <limits.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "long.h"
#include "set.h"
#include "types.h"

/**
 * long_string_set - Set a Long by string - Implements ConfigSetType::string_set()
 */
static int long_string_set(const struct ConfigSet *cs, void *var, struct ConfigDef *cdef,
                           const char *value, struct Buffer *err)
{
  if (!value || (value[0] == '\0'))
  {
    mutt_buffer_printf(err, _("Option %s may not be empty"), cdef->name);
    return CSR_ERR_INVALID | CSR_INV_TYPE;
  }

  long num = 0;
  if (mutt_str_atol(value, &num) < 0)
  {
    mutt_buffer_printf(err, _("Invalid long: %s"), value);
    return CSR_ERR_INVALID | CSR_INV_TYPE;
  }

  if ((num < 0) && (cdef->type & DT_NOT_NEGATIVE))
  {
    mutt_buffer_printf(err, _("Option %s may not be negative"), cdef->name);
    return CSR_ERR_INVALID | CSR_INV_VALIDATOR;
  }

  if (var)
  {
    if (num == (*(long *) var))
      return CSR_SUCCESS | CSR_SUC_NO_CHANGE;

    if (cdef->validator)
    {
      int rc = cdef->validator(cs, cdef, (intptr_t) num, err);

      if (CSR_RESULT(rc) != CSR_SUCCESS)
        return rc | CSR_INV_VALIDATOR;
    }

    *(long *) var = num;
  }
  else
  {
    cdef->initial = num;
  }

  return CSR_SUCCESS;
}

/**
 * long_string_get - Get a Long as a string - Implements ConfigSetType::string_get()
 */
static int long_string_get(const struct ConfigSet *cs, void *var,
                           const struct ConfigDef *cdef, struct Buffer *result)
{
  int value;

  if (var)
    value = *(long *) var;
  else
    value = (int) cdef->initial;

  mutt_buffer_printf(result, "%d", value);
  return CSR_SUCCESS;
}

/**
 * long_native_set - Set a Long config item by long - Implements ConfigSetType::native_set()
 */
static int long_native_set(const struct ConfigSet *cs, void *var,
                           const struct ConfigDef *cdef, intptr_t value, struct Buffer *err)
{
  if ((value < 0) && (cdef->type & DT_NOT_NEGATIVE))
  {
    mutt_buffer_printf(err, _("Option %s may not be negative"), cdef->name);
    return CSR_ERR_INVALID | CSR_INV_VALIDATOR;
  }

  if (value == (*(long *) var))
    return CSR_SUCCESS | CSR_SUC_NO_CHANGE;

  if (cdef->validator)
  {
    int rc = cdef->validator(cs, cdef, value, err);

    if (CSR_RESULT(rc) != CSR_SUCCESS)
      return rc | CSR_INV_VALIDATOR;
  }

  *(long *) var = value;
  return CSR_SUCCESS;
}

/**
 * long_native_get - Get a long from a Long config item - Implements ConfigSetType::native_get()
 */
static intptr_t long_native_get(const struct ConfigSet *cs, void *var,
                                const struct ConfigDef *cdef, struct Buffer *err)
{
  return *(long *) var;
}

/**
 * long_string_plus_equals - Add to a Long by string - Implements ConfigSetType::string_plus_equals()
 */
static int long_string_plus_equals(const struct ConfigSet *cs, void *var,
                                   const struct ConfigDef *cdef,
                                   const char *value, struct Buffer *err)
{
  long num = 0;
  if (!value || (value[0] == '\0') || (mutt_str_atol(value, &num) < 0))
  {
    mutt_buffer_printf(err, _("Invalid long: %s"), NONULL(value));
    return CSR_ERR_INVALID | CSR_INV_TYPE;
  }

  long result = *((long *) var) + num;
  if ((result < 0) && (cdef->type & DT_NOT_NEGATIVE))
  {
    mutt_buffer_printf(err, _("Option %s may not be negative"), cdef->name);
    return CSR_ERR_INVALID | CSR_INV_VALIDATOR;
  }

  if (result == (*(long *) var))
    return CSR_SUCCESS | CSR_SUC_NO_CHANGE;

  if (cdef->validator)
  {
    int rc = cdef->validator(cs, cdef, (intptr_t) result, err);

    if (CSR_RESULT(rc) != CSR_SUCCESS)
      return rc | CSR_INV_VALIDATOR;
  }

  *(long *) var = result;
  return CSR_SUCCESS;
}

/**
 * long_string_minus_equals - Subtract from a Long by string - Implements ConfigSetType::string_minus_equals()
 */
static int long_string_minus_equals(const struct ConfigSet *cs, void *var,
                                    const struct ConfigDef *cdef,
                                    const char *value, struct Buffer *err)
{
  long num = 0;
  if (!value || (value[0] == '\0') || (mutt_str_atol(value, &num) < 0))
  {
    mutt_buffer_printf(err, _("Invalid long: %s"), value);
    return CSR_ERR_INVALID | CSR_INV_TYPE;
  }

  long result = *((long *) var) - num;
  if ((result < 0) && (cdef->type & DT_NOT_NEGATIVE))
  {
    mutt_buffer_printf(err, _("Option %s may not be negative"), cdef->name);
    return CSR_ERR_INVALID | CSR_INV_VALIDATOR;
  }

  if (result == (*(long *) var))
    return CSR_SUCCESS | CSR_SUC_NO_CHANGE;

  if (cdef->validator)
  {
    int rc = cdef->validator(cs, cdef, (intptr_t) result, err);

    if (CSR_RESULT(rc) != CSR_SUCCESS)
      return rc | CSR_INV_VALIDATOR;
  }

  *(long *) var = result;
  return CSR_SUCCESS;
}

/**
 * long_reset - Reset a Long to its initial value - Implements ConfigSetType::reset()
 */
static int long_reset(const struct ConfigSet *cs, void *var,
                      const struct ConfigDef *cdef, struct Buffer *err)
{
  if (cdef->initial == (*(long *) var))
    return CSR_SUCCESS | CSR_SUC_NO_CHANGE;

  if (cdef->validator)
  {
    int rc = cdef->validator(cs, cdef, cdef->initial, err);

    if (CSR_RESULT(rc) != CSR_SUCCESS)
      return (rc | CSR_INV_VALIDATOR);
  }

  *(long *) var = cdef->initial;
  return CSR_SUCCESS;
}

/**
 * long_init - Register the Long config type
 * @param cs Config items
 */
void long_init(struct ConfigSet *cs)
{
  const struct ConfigSetType cst_long = {
    "long",
    long_string_set,
    long_string_get,
    long_native_set,
    long_native_get,
    long_string_plus_equals,
    long_string_minus_equals,
    long_reset,
    NULL, // destroy
  };
  cs_register_type(cs, DT_LONG, &cst_long);
}
