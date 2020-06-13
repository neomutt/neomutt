/**
 * @file
 * Type representing a list of strings
 *
 * @authors
 * Copyright (C) 2018-2019 Richard Russon <rich@flatcap.org>
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
 * @page config_slist Type: List of strings
 *
 * Type representing a list of strings.
 */

#include "config.h"
#include <stddef.h>
#include <limits.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "slist.h"
#include "set.h"
#include "types.h"

/**
 * slist_destroy - Destroy an Slist object
 * @param cs   Config items
 * @param var  Variable to destroy
 * @param cdef Variable definition
 */
static void slist_destroy(const struct ConfigSet *cs, void *var, const struct ConfigDef *cdef)
{
  if (!cs || !var || !cdef)
    return; /* LCOV_EXCL_LINE */

  struct Slist **l = (struct Slist **) var;
  if (!*l)
    return;

  slist_free(l);
}

/**
 * slist_string_set - Set a Slist by string
 * @param cs    Config items
 * @param var   Variable to set
 * @param cdef  Variable definition
 * @param value Value to set
 * @param err   Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 *
 * If var is NULL, then the config item's initial value will be set.
 */
static int slist_string_set(const struct ConfigSet *cs, void *var, struct ConfigDef *cdef,
                            const char *value, struct Buffer *err)
{
  if (!cs || !cdef)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  /* Store empty strings as NULL */
  if (value && (value[0] == '\0'))
    value = NULL;

  struct Slist *list = NULL;

  int rc = CSR_SUCCESS;

  if (var)
  {
    list = slist_parse(value, cdef->type);

    if (cdef->validator)
    {
      rc = cdef->validator(cs, cdef, (intptr_t) list, err);

      if (CSR_RESULT(rc) != CSR_SUCCESS)
      {
        slist_free(&list);
        return (rc | CSR_INV_VALIDATOR);
      }
    }

    slist_destroy(cs, var, cdef);

    *(struct Slist **) var = list;

    if (!list)
      rc |= CSR_SUC_EMPTY;
  }
  else
  {
    if (cdef->type & DT_INITIAL_SET)
      FREE(&cdef->initial);

    cdef->type |= DT_INITIAL_SET;
    cdef->initial = IP mutt_str_strdup(value);
  }

  return rc;
}

/**
 * slist_string_get - Get a Slist as a string
 * @param cs     Config items
 * @param var    Variable to get
 * @param cdef   Variable definition
 * @param result Buffer for results or error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 *
 * If var is NULL, then the config item's initial value will be returned.
 */
static int slist_string_get(const struct ConfigSet *cs, void *var,
                            const struct ConfigDef *cdef, struct Buffer *result)
{
  if (!cs || !cdef)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  if (var)
  {
    struct Slist *list = *(struct Slist **) var;
    if (!list)
      return (CSR_SUCCESS | CSR_SUC_EMPTY); /* empty string */

    struct ListNode *np = NULL;
    STAILQ_FOREACH(np, &list->head, entries)
    {
      mutt_buffer_addstr(result, np->data);
      if (STAILQ_NEXT(np, entries))
      {
        int sep = (cdef->type & SLIST_SEP_MASK);
        if (sep == SLIST_SEP_COMMA)
          mutt_buffer_addch(result, ',');
        else if (sep == SLIST_SEP_COLON)
          mutt_buffer_addch(result, ':');
        else
          mutt_buffer_addch(result, ' ');
      }
    }
  }
  else
  {
    mutt_buffer_addstr(result, (char *) cdef->initial);
  }

  int rc = CSR_SUCCESS;
  if (mutt_buffer_is_empty(result))
    rc |= CSR_SUC_EMPTY;

  return rc;
}

/**
 * slist_native_set - Set a Slist config item by STAILQ
 * @param cs    Config items
 * @param var   Variable to set
 * @param cdef  Variable definition
 * @param value Mailbox slist
 * @param err   Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
static int slist_native_set(const struct ConfigSet *cs, void *var,
                            const struct ConfigDef *cdef, intptr_t value, struct Buffer *err)
{
  if (!cs || !var || !cdef)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  int rc;

  if (cdef->validator)
  {
    rc = cdef->validator(cs, cdef, value, err);

    if (CSR_RESULT(rc) != CSR_SUCCESS)
      return (rc | CSR_INV_VALIDATOR);
  }

  slist_free(var);

  struct Slist *list = slist_dup((struct Slist *) value);

  rc = CSR_SUCCESS;
  if (!list)
    rc |= CSR_SUC_EMPTY;

  *(struct Slist **) var = list;
  return rc;
}

/**
 * slist_native_get - Get a STAILQ from a Slist config item
 * @param cs   Config items
 * @param var  Variable to get
 * @param cdef Variable definition
 * @param err  Buffer for error messages
 * @retval intptr_t Mailbox slist
 */
static intptr_t slist_native_get(const struct ConfigSet *cs, void *var,
                                 const struct ConfigDef *cdef, struct Buffer *err)
{
  if (!cs || !var || !cdef)
    return INT_MIN; /* LCOV_EXCL_LINE */

  struct Slist *list = *(struct Slist **) var;

  return (intptr_t) list;
}

/**
 * slist_reset - Reset a Slist to its initial value
 * @param cs   Config items
 * @param var  Variable to reset
 * @param cdef Variable definition
 * @param err  Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
static int slist_reset(const struct ConfigSet *cs, void *var,
                       const struct ConfigDef *cdef, struct Buffer *err)
{
  if (!cs || !var || !cdef)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  struct Slist *list = NULL;
  const char *initial = (const char *) cdef->initial;

  if (initial)
    list = slist_parse(initial, cdef->type);

  int rc = CSR_SUCCESS;

  if (cdef->validator)
  {
    rc = cdef->validator(cs, cdef, (intptr_t) list, err);

    if (CSR_RESULT(rc) != CSR_SUCCESS)
    {
      slist_destroy(cs, &list, cdef);
      return (rc | CSR_INV_VALIDATOR);
    }
  }

  if (!list)
    rc |= CSR_SUC_EMPTY;

  slist_destroy(cs, var, cdef);

  *(struct Slist **) var = list;
  return rc;
}

/**
 * slist_init - Register the Slist config type
 * @param cs Config items
 */
void slist_init(struct ConfigSet *cs)
{
  const struct ConfigSetType cst_slist = {
    "slist",
    slist_string_set,
    slist_string_get,
    slist_native_set,
    slist_native_get,
    NULL, // string_plus_equals
    NULL, // string_minus_equals
    slist_reset,
    slist_destroy,
  };
  cs_register_type(cs, DT_SLIST, &cst_slist);
}
