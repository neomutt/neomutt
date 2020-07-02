/**
 * @file
 * Type representing a multibyte character table
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
 * @page config_mbtable Type: Multi-byte character table
 *
 * Config type representing a multibyte character table.
 *
 * - Backed by `struct MbTable`
 * - Empty multibyte character table is stored as `NULL`
 * - Validator is passed `struct MbTable`, which may be `NULL`
 * - Data is freed when `ConfigSet` is freed
 */

#include "config.h"
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include "mutt/lib.h"
#include "mbtable.h"
#include "set.h"
#include "types.h"

/**
 * mbtable_parse - Parse a multibyte string into a table
 * @param s String of multibyte characters
 * @retval ptr New MbTable object
 */
struct MbTable *mbtable_parse(const char *s)
{
  struct MbTable *t = NULL;
  size_t slen, k;
  mbstate_t mbstate;
  char *d = NULL;

  slen = mutt_str_len(s);
  if (!slen)
    return NULL;

  t = mutt_mem_calloc(1, sizeof(struct MbTable));

  t->orig_str = mutt_str_dup(s);
  /* This could be more space efficient.  However, being used on tiny
   * strings (C_ToChars and C_StatusChars), the overhead is not great. */
  t->chars = mutt_mem_calloc(slen, sizeof(char *));
  t->segmented_str = mutt_mem_calloc(slen * 2, sizeof(char));
  d = t->segmented_str;

  memset(&mbstate, 0, sizeof(mbstate));
  while (slen && (k = mbrtowc(NULL, s, slen, &mbstate)))
  {
    if ((k == (size_t)(-1)) || (k == (size_t)(-2)))
    {
      /* XXX put message in err buffer; fail? warning? */
      mutt_debug(LL_DEBUG1, "mbrtowc returned %d converting %s in %s\n",
                 (k == (size_t)(-1)) ? -1 : -2, s, t->orig_str);
      if (k == (size_t)(-1))
        memset(&mbstate, 0, sizeof(mbstate));
      k = (k == (size_t)(-1)) ? 1 : slen;
    }

    slen -= k;
    t->chars[t->len++] = d;
    while (k--)
      *d++ = *s++;
    *d++ = '\0';
  }

  return t;
}

/**
 * mbtable_destroy - Destroy an MbTable object - Implements ConfigSetType::destroy()
 */
static void mbtable_destroy(const struct ConfigSet *cs, void *var, const struct ConfigDef *cdef)
{
  struct MbTable **m = var;
  if (!*m)
    return;

  mbtable_free(m);
}

/**
 * mbtable_string_set - Set an MbTable by string - Implements ConfigSetType::string_set()
 */
static int mbtable_string_set(const struct ConfigSet *cs, void *var, struct ConfigDef *cdef,
                              const char *value, struct Buffer *err)
{
  /* Store empty mbtables as NULL */
  if (value && (value[0] == '\0'))
    value = NULL;

  struct MbTable *table = NULL;

  int rc = CSR_SUCCESS;

  if (var)
  {
    struct MbTable *curval = *(struct MbTable **) var;
    if (curval && mutt_str_equal(value, curval->orig_str))
      return CSR_SUCCESS | CSR_SUC_NO_CHANGE;

    table = mbtable_parse(value);

    if (cdef->validator)
    {
      rc = cdef->validator(cs, cdef, (intptr_t) table, err);

      if (CSR_RESULT(rc) != CSR_SUCCESS)
      {
        mbtable_free(&table);
        return rc | CSR_INV_VALIDATOR;
      }
    }

    mbtable_destroy(cs, var, cdef);

    *(struct MbTable **) var = table;

    if (!table)
      rc |= CSR_SUC_EMPTY;
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
 * mbtable_string_get - Get a MbTable as a string - Implements ConfigSetType::string_get()
 */
static int mbtable_string_get(const struct ConfigSet *cs, void *var,
                              const struct ConfigDef *cdef, struct Buffer *result)
{
  const char *str = NULL;

  if (var)
  {
    struct MbTable *table = *(struct MbTable **) var;
    if (!table || !table->orig_str)
      return CSR_SUCCESS | CSR_SUC_EMPTY; /* empty string */
    str = table->orig_str;
  }
  else
  {
    str = (char *) cdef->initial;
  }

  mutt_buffer_addstr(result, str);
  return CSR_SUCCESS;
}

/**
 * mbtable_dup - Create a copy of an MbTable object
 * @param table MbTable to duplicate
 * @retval ptr New MbTable object
 */
static struct MbTable *mbtable_dup(struct MbTable *table)
{
  if (!table)
    return NULL; /* LCOV_EXCL_LINE */

  struct MbTable *m = mutt_mem_calloc(1, sizeof(*m));
  m->orig_str = mutt_str_dup(table->orig_str);
  return m;
}

/**
 * mbtable_native_set - Set an MbTable config item by MbTable object - Implements ConfigSetType::native_set()
 */
static int mbtable_native_set(const struct ConfigSet *cs, void *var,
                              const struct ConfigDef *cdef, intptr_t value,
                              struct Buffer *err)
{
  int rc;

  if (cdef->validator)
  {
    rc = cdef->validator(cs, cdef, value, err);

    if (CSR_RESULT(rc) != CSR_SUCCESS)
      return rc | CSR_INV_VALIDATOR;
  }

  mbtable_free(var);

  struct MbTable *table = mbtable_dup((struct MbTable *) value);

  rc = CSR_SUCCESS;
  if (!table)
    rc |= CSR_SUC_EMPTY;

  *(struct MbTable **) var = table;
  return rc;
}

/**
 * mbtable_native_get - Get an MbTable object from a MbTable config item - Implements ConfigSetType::native_get()
 */
static intptr_t mbtable_native_get(const struct ConfigSet *cs, void *var,
                                   const struct ConfigDef *cdef, struct Buffer *err)
{
  struct MbTable *table = *(struct MbTable **) var;

  return (intptr_t) table;
}

/**
 * mbtable_reset - Reset an MbTable to its initial value - Implements ConfigSetType::reset()
 */
static int mbtable_reset(const struct ConfigSet *cs, void *var,
                         const struct ConfigDef *cdef, struct Buffer *err)
{
  struct MbTable *table = NULL;
  const char *initial = (const char *) cdef->initial;

  struct MbTable *curtable = *(struct MbTable **) var;
  const char *curval = curtable ? curtable->orig_str : NULL;

  int rc = CSR_SUCCESS;
  if (!curtable)
    rc |= CSR_SUC_EMPTY;

  if (mutt_str_equal(initial, curval))
    return rc | CSR_SUC_NO_CHANGE;

  if (initial)
    table = mbtable_parse(initial);

  if (cdef->validator)
  {
    rc = cdef->validator(cs, cdef, (intptr_t) table, err);

    if (CSR_RESULT(rc) != CSR_SUCCESS)
    {
      mbtable_destroy(cs, &table, cdef);
      return rc | CSR_INV_VALIDATOR;
    }
  }

  if (!table)
    rc |= CSR_SUC_EMPTY;

  mbtable_destroy(cs, var, cdef);

  *(struct MbTable **) var = table;
  return rc;
}

/**
 * mbtable_init - Register the MbTable config type
 * @param cs Config items
 */
void mbtable_init(struct ConfigSet *cs)
{
  const struct ConfigSetType cst_mbtable = {
    "mbtable",
    mbtable_string_set,
    mbtable_string_get,
    mbtable_native_set,
    mbtable_native_get,
    NULL, // string_plus_equals
    NULL, // string_minus_equals
    mbtable_reset,
    mbtable_destroy,
  };
  cs_register_type(cs, DT_MBTABLE, &cst_mbtable);
}

/**
 * mbtable_free - Free an MbTable object
 * @param[out] table MbTable to free
 */
void mbtable_free(struct MbTable **table)
{
  if (!table || !*table)
    return;

  FREE(&(*table)->orig_str);
  FREE(&(*table)->chars);
  FREE(&(*table)->segmented_str);
  FREE(table);
}
