/**
 * @file
 * Type representing an enumeration
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
 * @page config_enum Type: Enumeration
 *
 * Type representing an enumeration.
 */

#include "config.h"
#include <stddef.h>
#include <limits.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "enum.h"
#include "set.h"
#include "types.h"

/**
 * enum_string_set - Set a Enumeration by string
 * @param cs    Config items
 * @param var   Variable to set
 * @param cdef  Variable definition
 * @param value Value to set
 * @param err   Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 *
 * If var is NULL, then the config item's initial value will be set.
 */
static int enum_string_set(const struct ConfigSet *cs, void *var, struct ConfigDef *cdef,
                           const char *value, struct Buffer *err)
{
  if (!cs || !cdef || !value)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  struct EnumDef *ed = (struct EnumDef *) cdef->data;
  if (!ed || !ed->lookup)
    return CSR_ERR_CODE;

  int num = mutt_map_get_value(value, ed->lookup);
  if (num < 0)
  {
    mutt_buffer_printf(err, _("Invalid enum value: %s"), value);
    return (CSR_ERR_INVALID | CSR_INV_TYPE);
  }

  if (var)
  {
    if (num == (*(unsigned char *) var))
      return (CSR_SUCCESS | CSR_SUC_NO_CHANGE);

    if (cdef->validator)
    {
      int rc = cdef->validator(cs, cdef, (intptr_t) num, err);

      if (CSR_RESULT(rc) != CSR_SUCCESS)
        return (rc | CSR_INV_VALIDATOR);
    }

    *(unsigned char *) var = num;
  }
  else
  {
    cdef->initial = num;
  }

  return CSR_SUCCESS;
}

/**
 * enum_string_get - Get a Enumeration as a string
 * @param cs     Config items
 * @param var    Variable to get
 * @param cdef   Variable definition
 * @param result Buffer for results or error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 *
 * If var is NULL, then the config item's initial value will be returned.
 */
static int enum_string_get(const struct ConfigSet *cs, void *var,
                           const struct ConfigDef *cdef, struct Buffer *result)
{
  if (!cs || !cdef)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  unsigned int value;

  if (var)
    value = *(unsigned char *) var;
  else
    value = (int) cdef->initial;

  struct EnumDef *ed = (struct EnumDef *) cdef->data;
  if (!ed || !ed->lookup)
    return CSR_ERR_CODE;

  const char *name = mutt_map_get_name(value, ed->lookup);
  if (!name)
  {
    mutt_debug(LL_DEBUG1, "Variable has an invalid value: %d\n", value);
    return (CSR_ERR_INVALID | CSR_INV_TYPE);
  }

  mutt_buffer_addstr(result, name);
  return CSR_SUCCESS;
}

/**
 * enum_native_set - Set a Enumeration config item by int
 * @param cs    Config items
 * @param var   Variable to set
 * @param cdef  Variable definition
 * @param value Enumeration value
 * @param err   Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
static int enum_native_set(const struct ConfigSet *cs, void *var,
                           const struct ConfigDef *cdef, intptr_t value, struct Buffer *err)
{
  if (!cs || !var || !cdef)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  struct EnumDef *ed = (struct EnumDef *) cdef->data;
  if (!ed || !ed->lookup)
    return CSR_ERR_CODE;

  const char *name = mutt_map_get_name(value, ed->lookup);
  if (!name)
  {
    mutt_buffer_printf(err, _("Invalid enum value: %ld"), value);
    return (CSR_ERR_INVALID | CSR_INV_TYPE);
  }

  if (value == (*(unsigned char *) var))
    return (CSR_SUCCESS | CSR_SUC_NO_CHANGE);

  if (cdef->validator)
  {
    int rc = cdef->validator(cs, cdef, value, err);

    if (CSR_RESULT(rc) != CSR_SUCCESS)
      return (rc | CSR_INV_VALIDATOR);
  }

  *(unsigned char *) var = value;
  return CSR_SUCCESS;
}

/**
 * enum_native_get - Get an int object from a Enumeration config item
 * @param cs   Config items
 * @param var  Variable to get
 * @param cdef Variable definition
 * @param err  Buffer for error messages
 * @retval intptr_t Enumeration value
 */
static intptr_t enum_native_get(const struct ConfigSet *cs, void *var,
                                const struct ConfigDef *cdef, struct Buffer *err)
{
  if (!cs || !var || !cdef)
    return INT_MIN; /* LCOV_EXCL_LINE */

  return *(unsigned char *) var;
}

/**
 * enum_reset - Reset a Enumeration to its initial value
 * @param cs   Config items
 * @param var  Variable to reset
 * @param cdef Variable definition
 * @param err  Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
static int enum_reset(const struct ConfigSet *cs, void *var,
                      const struct ConfigDef *cdef, struct Buffer *err)
{
  if (!cs || !var || !cdef)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  if (cdef->initial == (*(unsigned char *) var))
    return (CSR_SUCCESS | CSR_SUC_NO_CHANGE);

  if (cdef->validator)
  {
    int rc = cdef->validator(cs, cdef, cdef->initial, err);

    if (CSR_RESULT(rc) != CSR_SUCCESS)
      return (rc | CSR_INV_VALIDATOR);
  }

  *(unsigned char *) var = cdef->initial;
  return CSR_SUCCESS;
}

/**
 * enum_init - Register the Enumeration config type
 * @param cs Config items
 */
void enum_init(struct ConfigSet *cs)
{
  const struct ConfigSetType cst_enum = {
    "enum",
    enum_string_set,
    enum_string_get,
    enum_native_set,
    enum_native_get,
    NULL, // string_plus_equals
    NULL, // string_minus_equals
    enum_reset,
    NULL, // destroy
  };
  cs_register_type(cs, DT_ENUM, &cst_enum);
}
