/**
 * @file
 * Type representing an email address
 *
 * @authors
 * Copyright (C) 2017-2018 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
 * @page config_address Type: Email address
 *
 * Config type representing an email address.
 *
 * - Backed by `struct Address`
 * - Empty address is stored as `NULL`
 * - Validator is passed `struct Address *`, which may be `NULL`
 * - Data is freed when `ConfigSet` is freed
 */

#include "config.h"
#include <stddef.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "address.h"
#include "set.h"
#include "types.h"

/**
 * address_destroy - Destroy an Address object - Implements ConfigSetType::destroy()
 */
static void address_destroy(const struct ConfigSet *cs, void *var, const struct ConfigDef *cdef)
{
  struct Address **a = var;
  if (!*a)
    return;

  address_free(a);
}

/**
 * address_string_set - Set an Address by string - Implements ConfigSetType::string_set()
 */
static int address_string_set(const struct ConfigSet *cs, void *var, struct ConfigDef *cdef,
                              const char *value, struct Buffer *err)
{
  struct Address *addr = NULL;

  /* An empty address "" will be stored as NULL */
  if (var && value && (value[0] != '\0'))
  {
    // TODO - config can only store one
    struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
    mutt_addrlist_parse(&al, value);
    addr = mutt_addr_copy(TAILQ_FIRST(&al));
    mutt_addrlist_clear(&al);
  }

  int rc = CSR_SUCCESS;

  if (var)
  {
    if (cdef->validator)
    {
      rc = cdef->validator(cs, cdef, (intptr_t) addr, err);

      if (CSR_RESULT(rc) != CSR_SUCCESS)
      {
        address_destroy(cs, &addr, cdef);
        return rc | CSR_INV_VALIDATOR;
      }
    }

    /* ordinary variable setting */
    address_destroy(cs, var, cdef);

    *(struct Address **) var = addr;

    if (!addr)
      rc |= CSR_SUC_EMPTY;
  }
  else
  {
    /* set the default/initial value */
    if (cdef->type & DT_INITIAL_SET)
      FREE(&cdef->initial);

    cdef->type |= DT_INITIAL_SET;
    cdef->initial = IP mutt_str_dup(value);
  }

  return rc;
}

/**
 * address_string_get - Get an Address as a string - Implements ConfigSetType::string_get()
 */
static int address_string_get(const struct ConfigSet *cs, void *var,
                              const struct ConfigDef *cdef, struct Buffer *result)
{
  char tmp[8192] = { 0 };
  const char *str = NULL;

  if (var)
  {
    struct Address *a = *(struct Address **) var;
    if (a)
    {
      mutt_addr_write(tmp, sizeof(tmp), a, false);
      str = tmp;
    }
  }
  else
  {
    str = (char *) cdef->initial;
  }

  if (!str)
    return CSR_SUCCESS | CSR_SUC_EMPTY; /* empty string */

  mutt_buffer_addstr(result, str);
  return CSR_SUCCESS;
}

/**
 * address_dup - Create a copy of an Address object
 * @param addr Address to duplicate
 * @retval ptr New Address object
 */
static struct Address *address_dup(struct Address *addr)
{
  if (!addr)
    return NULL; /* LCOV_EXCL_LINE */

  struct Address *a = mutt_mem_calloc(1, sizeof(*a));
  a->personal = mutt_str_dup(addr->personal);
  a->mailbox = mutt_str_dup(addr->mailbox);
  return a;
}

/**
 * address_native_set - Set an Address config item by Address object - Implements ConfigSetType::native_set()
 */
static int address_native_set(const struct ConfigSet *cs, void *var,
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

  address_free(var);

  struct Address *addr = address_dup((struct Address *) value);

  rc = CSR_SUCCESS;
  if (!addr)
    rc |= CSR_SUC_EMPTY;

  *(struct Address **) var = addr;
  return rc;
}

/**
 * address_native_get - Get an Address object from an Address config item - Implements ConfigSetType::native_get()
 */
static intptr_t address_native_get(const struct ConfigSet *cs, void *var,
                                   const struct ConfigDef *cdef, struct Buffer *err)
{
  struct Address *addr = *(struct Address **) var;

  return (intptr_t) addr;
}

/**
 * address_reset - Reset an Address to its initial value - Implements ConfigSetType::reset()
 */
static int address_reset(const struct ConfigSet *cs, void *var,
                         const struct ConfigDef *cdef, struct Buffer *err)
{
  struct Address *a = NULL;
  const char *initial = (const char *) cdef->initial;

  if (initial)
    a = address_new(initial);

  int rc = CSR_SUCCESS;

  if (cdef->validator)
  {
    rc = cdef->validator(cs, cdef, (intptr_t) a, err);

    if (CSR_RESULT(rc) != CSR_SUCCESS)
    {
      address_destroy(cs, &a, cdef);
      return rc | CSR_INV_VALIDATOR;
    }
  }

  if (!a)
    rc |= CSR_SUC_EMPTY;

  address_destroy(cs, var, cdef);

  *(struct Address **) var = a;
  return rc;
}

/**
 * address_init - Register the Address config type
 * @param cs Config items
 */
void address_init(struct ConfigSet *cs)
{
  const struct ConfigSetType cst_address = {
    "address",
    address_string_set,
    address_string_get,
    address_native_set,
    address_native_get,
    NULL, // string_plus_equals
    NULL, // string_minus_equals
    address_reset,
    address_destroy,
  };
  cs_register_type(cs, DT_ADDRESS, &cst_address);
}

/**
 * address_new - Create an Address from a string
 * @param addr Email address to parse
 * @retval ptr New Address object
 */
struct Address *address_new(const char *addr)
{
  struct Address *a = mutt_mem_calloc(1, sizeof(*a));
  // a->personal = mutt_str_dup(addr);
  a->mailbox = mutt_str_dup(addr);
  return a;
}

/**
 * address_free - Free an Address object
 * @param[out] addr Address to free
 */
void address_free(struct Address **addr)
{
  if (!addr || !*addr)
    return;

  FREE(&(*addr)->personal);
  FREE(&(*addr)->mailbox);
  FREE(addr);
}
