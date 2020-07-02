/**
 * @file
 * Type representing a path
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page config_path Type: Path
 *
 * Config type representing a path.
 *
 * - Backed by `char *`
 * - Empty path is stored as `NULL`
 * - Validator is passed `char *`, which may be `NULL`
 * - Data is freed when `ConfigSet` is freed
 */

#include "config.h"
#include <stddef.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "path.h"
#include "set.h"
#include "types.h"

extern char *HomeDir;

/**
 * path_tidy - Tidy a path for storage
 * @param path   Path to be tidied
 * @param is_dir Is the path a directory?
 * @retval ptr Tidy path
 *
 * Expand `~` and remove junk like `/./`
 *
 * @note The caller must free the returned string
 */
static char *path_tidy(const char *path, bool is_dir)
{
  if (!path || (*path == '\0'))
    return NULL;

  char buf[PATH_MAX] = { 0 };
  mutt_str_copy(buf, path, sizeof(buf));

  mutt_path_tilde(buf, sizeof(buf), HomeDir);
  mutt_path_tidy(buf, is_dir);

  return mutt_str_dup(buf);
}

/**
 * path_destroy - Destroy a Path - Implements ConfigSetType::destroy()
 */
static void path_destroy(const struct ConfigSet *cs, void *var, const struct ConfigDef *cdef)
{
  const char **str = (const char **) var;
  if (!*str)
    return;

  FREE(var);
}

/**
 * path_string_set - Set a Path by string - Implements ConfigSetType::string_set()
 */
static int path_string_set(const struct ConfigSet *cs, void *var, struct ConfigDef *cdef,
                           const char *value, struct Buffer *err)
{
  /* Store empty paths as NULL */
  if (value && (value[0] == '\0'))
    value = NULL;

  if (!value && (cdef->type & DT_NOT_EMPTY))
  {
    mutt_buffer_printf(err, _("Option %s may not be empty"), cdef->name);
    return CSR_ERR_INVALID | CSR_INV_VALIDATOR;
  }

  int rc = CSR_SUCCESS;

  if (var)
  {
    if (mutt_str_equal(value, (*(char **) var)))
      return CSR_SUCCESS | CSR_SUC_NO_CHANGE;

    if (cdef->validator)
    {
      rc = cdef->validator(cs, cdef, (intptr_t) value, err);

      if (CSR_RESULT(rc) != CSR_SUCCESS)
        return rc | CSR_INV_VALIDATOR;
    }

    path_destroy(cs, var, cdef);

    char *str = path_tidy(value, cdef->type & DT_PATH_DIR);
    if (!str)
      rc |= CSR_SUC_EMPTY;

    *(char **) var = str;
  }
  else
  {
    if (cdef->type & DT_INITIAL_SET)
      FREE(&cdef->initial);

    cdef->type |= DT_INITIAL_SET;
    cdef->initial = IP mutt_str_dup(value);
  }

  return rc;
}

/**
 * path_string_get - Get a Path as a string - Implements ConfigSetType::string_get()
 */
static int path_string_get(const struct ConfigSet *cs, void *var,
                           const struct ConfigDef *cdef, struct Buffer *result)
{
  const char *str = NULL;

  if (var)
    str = *(const char **) var;
  else
    str = (char *) cdef->initial;

  if (!str)
    return CSR_SUCCESS | CSR_SUC_EMPTY; /* empty path */

  mutt_buffer_addstr(result, str);
  return CSR_SUCCESS;
}

/**
 * path_native_set - Set a Path config item by string - Implements ConfigSetType::native_set()
 */
static int path_native_set(const struct ConfigSet *cs, void *var,
                           const struct ConfigDef *cdef, intptr_t value, struct Buffer *err)
{
  char *str = (char *) value;

  /* Store empty paths as NULL */
  if (str && (str[0] == '\0'))
    value = 0;

  if ((value == 0) && (cdef->type & DT_NOT_EMPTY))
  {
    mutt_buffer_printf(err, _("Option %s may not be empty"), cdef->name);
    return CSR_ERR_INVALID | CSR_INV_VALIDATOR;
  }

  if (mutt_str_equal((const char *) value, (*(char **) var)))
    return CSR_SUCCESS | CSR_SUC_NO_CHANGE;

  int rc;

  if (cdef->validator)
  {
    rc = cdef->validator(cs, cdef, value, err);

    if (CSR_RESULT(rc) != CSR_SUCCESS)
      return rc | CSR_INV_VALIDATOR;
  }

  path_destroy(cs, var, cdef);

  str = path_tidy(str, cdef->type & DT_PATH_DIR);
  rc = CSR_SUCCESS;
  if (!str)
    rc |= CSR_SUC_EMPTY;

  *(const char **) var = str;
  return rc;
}

/**
 * path_native_get - Get a string from a Path config item - Implements ConfigSetType::native_get()
 */
static intptr_t path_native_get(const struct ConfigSet *cs, void *var,
                                const struct ConfigDef *cdef, struct Buffer *err)
{
  const char *str = *(const char **) var;

  return (intptr_t) str;
}

/**
 * path_reset - Reset a Path to its initial value - Implements ConfigSetType::reset()
 */
static int path_reset(const struct ConfigSet *cs, void *var,
                      const struct ConfigDef *cdef, struct Buffer *err)
{
  int rc = CSR_SUCCESS;

  const char *str = path_tidy((const char *) cdef->initial, cdef->type & DT_PATH_DIR);
  if (!str)
    rc |= CSR_SUC_EMPTY;

  if (mutt_str_equal(str, (*(char **) var)))
  {
    FREE(&str);
    return rc | CSR_SUC_NO_CHANGE;
  }

  if (cdef->validator)
  {
    rc = cdef->validator(cs, cdef, cdef->initial, err);

    if (CSR_RESULT(rc) != CSR_SUCCESS)
    {
      FREE(&str);
      return rc | CSR_INV_VALIDATOR;
    }
  }

  path_destroy(cs, var, cdef);

  if (!str)
    rc |= CSR_SUC_EMPTY;

  *(const char **) var = str;
  return rc;
}

/**
 * path_init - Register the Path config type
 * @param cs Config items
 */
void path_init(struct ConfigSet *cs)
{
  const struct ConfigSetType cst_path = {
    "path",
    path_string_set,
    path_string_get,
    path_native_set,
    path_native_get,
    NULL, // string_plus_equals
    NULL, // string_minus_equals
    path_reset,
    path_destroy,
  };
  cs_register_type(cs, DT_PATH, &cst_path);
}
