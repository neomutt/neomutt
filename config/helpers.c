/**
 * @file
 * Helper functions to get config values
 *
 * @authors
 * Copyright (C) 2020-2021 Richard Russon <rich@flatcap.org>
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
 * @page config_helpers Helper functions to get config values
 *
 * Helper functions to get config values
 */

#include "config.h"
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "mutt/lib.h"
#include "helpers.h"
#include "quad.h"
#include "set.h"
#include "subset.h"
#include "types.h"

/**
 * cs_subset_bool - Get a boolean config item by name
 * @param sub   Config Subset
 * @param name  Name of config item
 * @retval bool  Boolean value
 */
bool cs_subset_bool(const struct ConfigSubset *sub, const char *name)
{
  ASSERT(sub && name);

  struct HashElem *he = cs_subset_create_inheritance(sub, name);
  ASSERT(he);

#ifndef NDEBUG
  struct HashElem *he_base = cs_get_base(he);
  ASSERT(DTYPE(he_base->type) == DT_BOOL);
#endif

  intptr_t value = cs_subset_he_native_get(sub, he, NULL);
  ASSERT(value != INT_MIN);

  return (bool) value;
}

/**
 * cs_subset_enum - Get a enumeration config item by name
 * @param sub   Config Subset
 * @param name  Name of config item
 * @retval num  Enumeration
 */
unsigned char cs_subset_enum(const struct ConfigSubset *sub, const char *name)
{
  ASSERT(sub && name);

  struct HashElem *he = cs_subset_create_inheritance(sub, name);
  ASSERT(he);

#ifndef NDEBUG
  struct HashElem *he_base = cs_get_base(he);
  ASSERT(DTYPE(he_base->type) == DT_ENUM);
#endif

  intptr_t value = cs_subset_he_native_get(sub, he, NULL);
  ASSERT(value != INT_MIN);

  return (unsigned char) value;
}

/**
 * cs_subset_long - Get a long config item by name
 * @param sub   Config Subset
 * @param name  Name of config item
 * @retval num Long value
 */
long cs_subset_long(const struct ConfigSubset *sub, const char *name)
{
  ASSERT(sub && name);

  struct HashElem *he = cs_subset_create_inheritance(sub, name);
  ASSERT(he);

#ifndef NDEBUG
  struct HashElem *he_base = cs_get_base(he);
  ASSERT(DTYPE(he_base->type) == DT_LONG);
#endif

  intptr_t value = cs_subset_he_native_get(sub, he, NULL);
  ASSERT(value != INT_MIN);

  return (long) value;
}

/**
 * cs_subset_mbtable - Get a Multibyte table config item by name
 * @param sub   Config Subset
 * @param name  Name of config item
 * @retval ptr Multibyte table
 */
struct MbTable *cs_subset_mbtable(const struct ConfigSubset *sub, const char *name)
{
  ASSERT(sub && name);

  struct HashElem *he = cs_subset_create_inheritance(sub, name);
  ASSERT(he);

#ifndef NDEBUG
  struct HashElem *he_base = cs_get_base(he);
  ASSERT(DTYPE(he_base->type) == DT_MBTABLE);
#endif

  intptr_t value = cs_subset_he_native_get(sub, he, NULL);
  ASSERT(value != INT_MIN);

  return (struct MbTable *) value;
}

/**
 * cs_subset_number - Get a number config item by name
 * @param sub   Config Subset
 * @param name  Name of config item
 * @retval num Number
 */
short cs_subset_number(const struct ConfigSubset *sub, const char *name)
{
  ASSERT(sub && name);

  struct HashElem *he = cs_subset_create_inheritance(sub, name);
  ASSERT(he);

#ifndef NDEBUG
  struct HashElem *he_base = cs_get_base(he);
  ASSERT(DTYPE(he_base->type) == DT_NUMBER);
#endif

  intptr_t value = cs_subset_he_native_get(sub, he, NULL);
  ASSERT(value != INT_MIN);

  return (short) value;
}

/**
 * cs_subset_path - Get a path config item by name
 * @param sub   Config Subset
 * @param name  Name of config item
 * @retval ptr  Path
 * @retval NULL Empty path
 */
const char *cs_subset_path(const struct ConfigSubset *sub, const char *name)
{
  ASSERT(sub && name);

  struct HashElem *he = cs_subset_create_inheritance(sub, name);
  ASSERT(he);

#ifndef NDEBUG
  struct HashElem *he_base = cs_get_base(he);
  ASSERT(DTYPE(he_base->type) == DT_PATH);
#endif

  intptr_t value = cs_subset_he_native_get(sub, he, NULL);
  ASSERT(value != INT_MIN);

  return (const char *) value;
}

/**
 * cs_subset_quad - Get a quad-value config item by name
 * @param sub   Config Subset
 * @param name  Name of config item
 * @retval num Quad-value
 */
enum QuadOption cs_subset_quad(const struct ConfigSubset *sub, const char *name)
{
  ASSERT(sub && name);

  struct HashElem *he = cs_subset_create_inheritance(sub, name);
  ASSERT(he);

#ifndef NDEBUG
  struct HashElem *he_base = cs_get_base(he);
  ASSERT(DTYPE(he_base->type) == DT_QUAD);
#endif

  intptr_t value = cs_subset_he_native_get(sub, he, NULL);
  ASSERT(value != INT_MIN);

  return (enum QuadOption) value;
}

/**
 * cs_subset_regex - Get a regex config item by name
 * @param sub   Config Subset
 * @param name  Name of config item
 * @retval ptr  Regex
 * @retval NULL Empty regex
 */
const struct Regex *cs_subset_regex(const struct ConfigSubset *sub, const char *name)
{
  ASSERT(sub && name);

  struct HashElem *he = cs_subset_create_inheritance(sub, name);
  ASSERT(he);

#ifndef NDEBUG
  struct HashElem *he_base = cs_get_base(he);
  ASSERT(DTYPE(he_base->type) == DT_REGEX);
#endif

  intptr_t value = cs_subset_he_native_get(sub, he, NULL);
  ASSERT(value != INT_MIN);

  return (const struct Regex *) value;
}

/**
 * cs_subset_slist - Get a string-list config item by name
 * @param sub   Config Subset
 * @param name  Name of config item
 * @retval ptr  String list
 * @retval NULL Empty string list
 */
const struct Slist *cs_subset_slist(const struct ConfigSubset *sub, const char *name)
{
  ASSERT(sub && name);

  struct HashElem *he = cs_subset_create_inheritance(sub, name);
  ASSERT(he);

#ifndef NDEBUG
  struct HashElem *he_base = cs_get_base(he);
  ASSERT(DTYPE(he_base->type) == DT_SLIST);
#endif

  intptr_t value = cs_subset_he_native_get(sub, he, NULL);
  ASSERT(value != INT_MIN);

  return (const struct Slist *) value;
}

/**
 * cs_subset_sort - Get a sort config item by name
 * @param sub   Config Subset
 * @param name  Name of config item
 * @retval num Sort
 */
short cs_subset_sort(const struct ConfigSubset *sub, const char *name)
{
  ASSERT(sub && name);

  struct HashElem *he = cs_subset_create_inheritance(sub, name);
  ASSERT(he);

#ifndef NDEBUG
  struct HashElem *he_base = cs_get_base(he);
  ASSERT(DTYPE(he_base->type) == DT_SORT);
#endif

  intptr_t value = cs_subset_he_native_get(sub, he, NULL);
  ASSERT(value != INT_MIN);

  return (short) value;
}

/**
 * cs_subset_string - Get a string config item by name
 * @param sub   Config Subset
 * @param name  Name of config item
 * @retval ptr  String
 * @retval NULL Empty string
 */
const char *cs_subset_string(const struct ConfigSubset *sub, const char *name)
{
  ASSERT(sub && name);

  struct HashElem *he = cs_subset_create_inheritance(sub, name);
  ASSERT(he);

#ifndef NDEBUG
  struct HashElem *he_base = cs_get_base(he);
  ASSERT(DTYPE(he_base->type) == DT_STRING);
#endif

  intptr_t value = cs_subset_he_native_get(sub, he, NULL);
  ASSERT(value != INT_MIN);

  return (const char *) value;
}
