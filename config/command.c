/**
 * @file
 * Type representing a command
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
 * @page config-command Type: Command
 *
 * Type representing a command.
 */

#include "config.h"
#include <stddef.h>
#include <limits.h>
#include <stdint.h>
#include "mutt/buffer.h"
#include "mutt/memory.h"
#include "mutt/string2.h"
#include "set.h"
#include "types.h"

/**
 * command_destroy - Destroy a Command
 * @param cs   Config items
 * @param var  Variable to destroy
 * @param cdef Variable definition
 */
static void command_destroy(const struct ConfigSet *cs, void *var, const struct ConfigDef *cdef)
{
  if (!cs || !var || !cdef)
    return; /* LCOV_EXCL_LINE */

  const char **str = (const char **) var;
  if (!*str)
    return;

  /* Don't free strings from the var definition */
  if (*(char **) var == (char *) cdef->initial)
  {
    *(char **) var = NULL;
    return;
  }

  FREE(var);
}

/**
 * command_string_set - Set a Command by string
 * @param cs    Config items
 * @param var   Variable to set
 * @param cdef  Variable definition
 * @param value Value to set
 * @param err   Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 *
 * If var is NULL, then the config item's initial value will be set.
 */
static int command_string_set(const struct ConfigSet *cs, void *var, struct ConfigDef *cdef,
                              const char *value, struct Buffer *err)
{
  if (!cs || !cdef)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  /* Store empty strings as NULL */
  if (value && (value[0] == '\0'))
    value = NULL;

  if (!value && (cdef->type & DT_NOT_EMPTY))
  {
    mutt_buffer_printf(err, "Option %s may not be empty", cdef->name);
    return (CSR_ERR_INVALID | CSR_INV_VALIDATOR);
  }

  int rc = CSR_SUCCESS;

  if (var)
  {
    if (mutt_str_strcmp(value, (*(char **) var)) == 0)
      return (CSR_SUCCESS | CSR_SUC_NO_CHANGE);

    if (cdef->validator)
    {
      rc = cdef->validator(cs, cdef, (intptr_t) value, err);

      if (CSR_RESULT(rc) != CSR_SUCCESS)
        return (rc | CSR_INV_VALIDATOR);
    }

    command_destroy(cs, var, cdef);

    const char *str = mutt_str_strdup(value);
    if (!str)
      rc |= CSR_SUC_EMPTY;

    *(const char **) var = str;
  }
  else
  {
    /* we're already using the initial value */
    if (*(char **) cdef->var == (char *) cdef->initial)
      *(char **) cdef->var = mutt_str_strdup((char *) cdef->initial);

    if (cdef->type & DT_INITIAL_SET)
      FREE(&cdef->initial);

    cdef->type |= DT_INITIAL_SET;
    cdef->initial = IP mutt_str_strdup(value);
  }

  return rc;
}

/**
 * command_string_get - Get a Command as a string
 * @param cs     Config items
 * @param var    Variable to get
 * @param cdef   Variable definition
 * @param result Buffer for results or error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 *
 * If var is NULL, then the config item's initial value will be returned.
 */
static int command_string_get(const struct ConfigSet *cs, void *var,
                              const struct ConfigDef *cdef, struct Buffer *result)
{
  if (!cs || !cdef)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  const char *str = NULL;

  if (var)
    str = *(const char **) var;
  else
    str = (char *) cdef->initial;

  if (!str)
    return (CSR_SUCCESS | CSR_SUC_EMPTY); /* empty string */

  mutt_buffer_addstr(result, str);
  return CSR_SUCCESS;
}

/**
 * command_native_set - Set a Command config item by string
 * @param cs    Config items
 * @param var   Variable to set
 * @param cdef  Variable definition
 * @param value Native pointer/value to set
 * @param err   Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
static int command_native_set(const struct ConfigSet *cs, void *var,
                              const struct ConfigDef *cdef, intptr_t value,
                              struct Buffer *err)
{
  if (!cs || !var || !cdef)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  const char *str = (const char *) value;

  /* Store empty strings as NULL */
  if (str && (str[0] == '\0'))
    value = 0;

  if ((value == 0) && (cdef->type & DT_NOT_EMPTY))
  {
    mutt_buffer_printf(err, "Option %s may not be empty", cdef->name);
    return (CSR_ERR_INVALID | CSR_INV_VALIDATOR);
  }

  if (mutt_str_strcmp((const char *) value, (*(char **) var)) == 0)
    return (CSR_SUCCESS | CSR_SUC_NO_CHANGE);

  int rc;

  if (cdef->validator)
  {
    rc = cdef->validator(cs, cdef, value, err);

    if (CSR_RESULT(rc) != CSR_SUCCESS)
      return (rc | CSR_INV_VALIDATOR);
  }

  command_destroy(cs, var, cdef);

  str = mutt_str_strdup(str);
  rc = CSR_SUCCESS;
  if (!str)
    rc |= CSR_SUC_EMPTY;

  *(const char **) var = str;
  return rc;
}

/**
 * command_native_get - Get a string from a Command config item
 * @param cs   Config items
 * @param var  Variable to get
 * @param cdef Variable definition
 * @param err  Buffer for error messages
 * @retval intptr_t Command string
 */
static intptr_t command_native_get(const struct ConfigSet *cs, void *var,
                                   const struct ConfigDef *cdef, struct Buffer *err)
{
  if (!cs || !var || !cdef)
    return INT_MIN; /* LCOV_EXCL_LINE */

  const char *str = *(const char **) var;

  return (intptr_t) str;
}

/**
 * command_reset - Reset a Command to its initial value
 * @param cs   Config items
 * @param var  Variable to reset
 * @param cdef Variable definition
 * @param err  Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
static int command_reset(const struct ConfigSet *cs, void *var,
                         const struct ConfigDef *cdef, struct Buffer *err)
{
  if (!cs || !var || !cdef)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  int rc = CSR_SUCCESS;

  const char *command = (const char *) cdef->initial;
  if (!command)
    rc |= CSR_SUC_EMPTY;

  if (mutt_str_strcmp(command, (*(char **) var)) == 0)
    return (rc | CSR_SUC_NO_CHANGE);

  if (cdef->validator)
  {
    rc = cdef->validator(cs, cdef, cdef->initial, err);

    if (CSR_RESULT(rc) != CSR_SUCCESS)
      return (rc | CSR_INV_VALIDATOR);
  }

  command_destroy(cs, var, cdef);

  if (!command)
    rc |= CSR_SUC_EMPTY;

  *(const char **) var = command;
  return rc;
}

/**
 * command_init - Register the Command config type
 * @param cs Config items
 */
void command_init(struct ConfigSet *cs)
{
  const struct ConfigSetType cst_command = {
    "command",          command_string_set, command_string_get,
    command_native_set, command_native_get, command_reset,
    command_destroy,
  };
  cs_register_type(cs, DT_COMMAND, &cst_command);
}
