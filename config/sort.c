/**
 * @file
 * Type representing a sort option
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
 * @page config-sort Type: Sorting
 *
 * Type representing a sort option.
 */

#include "config.h"
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include "mutt/buffer.h"
#include "mutt/logging.h"
#include "mutt/mapping.h"
#include "mutt/string2.h"
#include "sort.h"
#include "set.h"
#include "types.h"

// clang-format off
/**
 * SortAliasMethods - Sort methods for email aliases
 */
const struct Mapping SortAliasMethods[] = {
  { "address",  SORT_ADDRESS },
  { "alias",    SORT_ALIAS },
  { "unsorted", SORT_ORDER },
  { NULL,       0 },
};

/**
 * SortAuxMethods - Sort methods for '$sort_aux' for the index
 */
const struct Mapping SortAuxMethods[] = {
  { "date",          SORT_DATE },
  { "date-received", SORT_RECEIVED },
  { "date-sent",     SORT_DATE },
  { "from",          SORT_FROM },
  { "label",         SORT_LABEL },
  { "mailbox-order", SORT_ORDER },
  { "score",         SORT_SCORE },
  { "size",          SORT_SIZE },
  { "spam",          SORT_SPAM },
  { "subject",       SORT_SUBJECT },
  { "threads",       SORT_DATE },
  { "to",            SORT_TO },
  { NULL,            0 },
};

/**
 * SortBrowserMethods - Sort methods for the folder/dir browser
 */
const struct Mapping SortBrowserMethods[] = {
  { "alpha",    SORT_SUBJECT },
  { "count",    SORT_COUNT },
  { "date",     SORT_DATE },
  { "desc",     SORT_DESC },
  { "new",      SORT_UNREAD },
  { "unread",   SORT_UNREAD },
  { "size",     SORT_SIZE },
  { "unsorted", SORT_ORDER },
  { NULL,       0 },
};

/**
 * SortKeyMethods - Sort methods for encryption keys
 */
const struct Mapping SortKeyMethods[] = {
  { "address", SORT_ADDRESS },
  { "date",    SORT_DATE },
  { "keyid",   SORT_KEYID },
  { "trust",   SORT_TRUST },
  { NULL,      0 },
};

/**
 * SortMethods - Sort methods for '$sort' for the index
 */
const struct Mapping SortMethods[] = {
  { "date",          SORT_DATE },
  { "date-received", SORT_RECEIVED },
  { "date-sent",     SORT_DATE },
  { "from",          SORT_FROM },
  { "label",         SORT_LABEL },
  { "mailbox-order", SORT_ORDER },
  { "score",         SORT_SCORE },
  { "size",          SORT_SIZE },
  { "spam",          SORT_SPAM },
  { "subject",       SORT_SUBJECT },
  { "threads",       SORT_THREADS },
  { "to",            SORT_TO },
  { NULL,            0 },
};

/**
 * SortSidebarMethods - Sort methods for the sidebar
 */
const struct Mapping SortSidebarMethods[] = {
  { "alpha",         SORT_PATH },
  { "count",         SORT_COUNT },
  { "desc",          SORT_DESC },
  { "flagged",       SORT_FLAGGED },
  { "mailbox-order", SORT_ORDER },
  { "name",          SORT_PATH },
  { "new",           SORT_UNREAD },
  { "path",          SORT_PATH },
  { "unread",        SORT_UNREAD },
  { "unsorted",      SORT_ORDER },
  { NULL,            0 },
};
// clang-format on

/**
 * sort_string_set - Set a Sort by string
 * @param cs    Config items
 * @param var   Variable to set
 * @param cdef  Variable definition
 * @param value Value to set
 * @param err   Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 *
 * If var is NULL, then the config item's initial value will be set.
 */
static int sort_string_set(const struct ConfigSet *cs, void *var, struct ConfigDef *cdef,
                           const char *value, struct Buffer *err)
{
  if (!cs || !cdef || !value)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  intptr_t id = -1;
  int flags = 0;

  if (mutt_str_strncmp("reverse-", value, 8) == 0)
  {
    flags |= SORT_REVERSE;
    value += 8;
  }

  if (mutt_str_strncmp("last-", value, 5) == 0)
  {
    flags |= SORT_LAST;
    value += 5;
  }

  switch (cdef->type & DT_SUBTYPE_MASK)
  {
    case DT_SORT_INDEX:
      id = mutt_map_get_value(value, SortMethods);
      break;
    case DT_SORT_ALIAS:
      id = mutt_map_get_value(value, SortAliasMethods);
      break;
    case DT_SORT_AUX:
      id = mutt_map_get_value(value, SortAuxMethods);
      break;
    case DT_SORT_BROWSER:
      id = mutt_map_get_value(value, SortBrowserMethods);
      break;
    case DT_SORT_KEYS:
      id = mutt_map_get_value(value, SortKeyMethods);
      break;
    case DT_SORT_SIDEBAR:
      id = mutt_map_get_value(value, SortSidebarMethods);
      break;
    default:
      mutt_debug(1, "Invalid sort type: %u\n", cdef->type & DT_SUBTYPE_MASK);
      return CSR_ERR_CODE;
      break;
  }

  if (id < 0)
  {
    mutt_buffer_printf(err, "Invalid sort name: %s", value);
    return (CSR_ERR_INVALID | CSR_INV_TYPE);
  }

  id |= flags;

  if (var)
  {
    if (id == (*(short *) var))
      return (CSR_SUCCESS | CSR_SUC_NO_CHANGE);

    if (cdef->validator)
    {
      int rc = cdef->validator(cs, cdef, (intptr_t) id, err);

      if (CSR_RESULT(rc) != CSR_SUCCESS)
        return (rc | CSR_INV_VALIDATOR);
    }

    *(short *) var = id;
  }
  else
  {
    cdef->initial = id;
  }

  return CSR_SUCCESS;
}

/**
 * sort_string_get - Get a Sort as a string
 * @param cs     Config items
 * @param var    Variable to get
 * @param cdef   Variable definition
 * @param result Buffer for results or error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 *
 * If var is NULL, then the config item's initial value will be returned.
 */
static int sort_string_get(const struct ConfigSet *cs, void *var,
                           const struct ConfigDef *cdef, struct Buffer *result)
{
  if (!cs || !cdef)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  int sort;

  if (var)
    sort = *(short *) var;
  else
    sort = (int) cdef->initial;

  if (sort & SORT_REVERSE)
    mutt_buffer_addstr(result, "reverse-");
  if (sort & SORT_LAST)
    mutt_buffer_addstr(result, "last-");

  sort &= SORT_MASK;

  const char *str = NULL;

  switch (cdef->type & DT_SUBTYPE_MASK)
  {
    case DT_SORT_INDEX:
      str = mutt_map_get_name(sort, SortMethods);
      break;
    case DT_SORT_ALIAS:
      str = mutt_map_get_name(sort, SortAliasMethods);
      break;
    case DT_SORT_AUX:
      str = mutt_map_get_name(sort, SortAuxMethods);
      break;
    case DT_SORT_BROWSER:
      str = mutt_map_get_name(sort, SortBrowserMethods);
      break;
    case DT_SORT_KEYS:
      str = mutt_map_get_name(sort, SortKeyMethods);
      break;
    case DT_SORT_SIDEBAR:
      str = mutt_map_get_name(sort, SortSidebarMethods);
      break;
    default:
      mutt_debug(1, "Invalid sort type: %u\n", cdef->type & DT_SUBTYPE_MASK);
      return CSR_ERR_CODE;
      break;
  }

  if (!str)
  {
    mutt_debug(1, "Variable has an invalid value: %d/%d\n",
               cdef->type & DT_SUBTYPE_MASK, sort);
    return (CSR_ERR_INVALID | CSR_INV_TYPE);
  }

  mutt_buffer_addstr(result, str);
  return CSR_SUCCESS;
}

/**
 * sort_native_set - Set a Sort config item by int
 * @param cs    Config items
 * @param var   Variable to set
 * @param cdef  Variable definition
 * @param value Sort value
 * @param err   Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
static int sort_native_set(const struct ConfigSet *cs, void *var,
                           const struct ConfigDef *cdef, intptr_t value, struct Buffer *err)
{
  if (!cs || !var || !cdef)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  const char *str = NULL;

  switch (cdef->type & DT_SUBTYPE_MASK)
  {
    case DT_SORT_INDEX:
      str = mutt_map_get_name((value & SORT_MASK), SortMethods);
      break;
    case DT_SORT_ALIAS:
      str = mutt_map_get_name((value & SORT_MASK), SortAliasMethods);
      break;
    case DT_SORT_AUX:
      str = mutt_map_get_name((value & SORT_MASK), SortAuxMethods);
      break;
    case DT_SORT_BROWSER:
      str = mutt_map_get_name((value & SORT_MASK), SortBrowserMethods);
      break;
    case DT_SORT_KEYS:
      str = mutt_map_get_name((value & SORT_MASK), SortKeyMethods);
      break;
    case DT_SORT_SIDEBAR:
      str = mutt_map_get_name((value & SORT_MASK), SortSidebarMethods);
      break;
    default:
      mutt_debug(1, "Invalid sort type: %u\n", cdef->type & DT_SUBTYPE_MASK);
      return CSR_ERR_CODE;
      break;
  }

  if (!str)
  {
    mutt_buffer_printf(err, "Invalid sort type: %ld", value);
    return (CSR_ERR_INVALID | CSR_INV_TYPE);
  }

  if (value == (*(short *) var))
    return (CSR_SUCCESS | CSR_SUC_NO_CHANGE);

  if (cdef->validator)
  {
    int rc = cdef->validator(cs, cdef, value, err);

    if (CSR_RESULT(rc) != CSR_SUCCESS)
      return (rc | CSR_INV_VALIDATOR);
  }

  *(short *) var = value;
  return CSR_SUCCESS;
}

/**
 * sort_native_get - Get an int from a Sort config item
 * @param cs   Config items
 * @param var  Variable to get
 * @param cdef Variable definition
 * @param err  Buffer for error messages
 * @retval intptr_t Sort ID
 */
static intptr_t sort_native_get(const struct ConfigSet *cs, void *var,
                                const struct ConfigDef *cdef, struct Buffer *err)
{
  if (!cs || !var || !cdef)
    return INT_MIN; /* LCOV_EXCL_LINE */

  return *(short *) var;
}

/**
 * sort_reset - Reset a Sort to its initial value
 * @param cs   Config items
 * @param var  Variable to reset
 * @param cdef Variable definition
 * @param err  Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
static int sort_reset(const struct ConfigSet *cs, void *var,
                      const struct ConfigDef *cdef, struct Buffer *err)
{
  if (!cs || !var || !cdef)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  if (cdef->initial == (*(short *) var))
    return (CSR_SUCCESS | CSR_SUC_NO_CHANGE);

  if (cdef->validator)
  {
    int rc = cdef->validator(cs, cdef, cdef->initial, err);

    if (CSR_RESULT(rc) != CSR_SUCCESS)
      return (rc | CSR_INV_VALIDATOR);
  }

  *(short *) var = cdef->initial;
  return CSR_SUCCESS;
}

/**
 * sort_init - Register the Sort config type
 * @param cs Config items
 */
void sort_init(struct ConfigSet *cs)
{
  const struct ConfigSetType cst_sort = {
    "sort",
    sort_string_set,
    sort_string_get,
    sort_native_set,
    sort_native_get,
    sort_reset,
    NULL,
  };
  cs_register_type(cs, DT_SORT, &cst_sort);
}
