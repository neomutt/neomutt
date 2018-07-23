/**
 * @file
 * A collection of config items
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
 * @page config-set Config Set
 *
 * A collection of config items.
 */

#include "config.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mutt/buffer.h"
#include "mutt/hash.h"
#include "mutt/logging.h"
#include "mutt/memory.h"
#include "mutt/string2.h"
#include "set.h"
#include "inheritance.h"
#include "types.h"

struct ConfigSetType RegisteredTypes[18] = {
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL },
};

/**
 * destroy - Callback function for the Hash Table
 * @param type Object type, e.g. #DT_STRING
 * @param obj  Object to destroy
 * @param data ConfigSet associated with the object
 */
static void destroy(int type, void *obj, intptr_t data)
{
  if (!obj || (data == 0))
    return; /* LCOV_EXCL_LINE */

  struct ConfigSet *cs = (struct ConfigSet *) data;

  const struct ConfigSetType *cst = NULL;

  if (type & DT_INHERITED)
  {
    struct Inheritance *i = obj;
    FREE(&i->name);
    FREE(&i);
  }
  else
  {
    struct ConfigDef *cdef = obj;

    cst = cs_get_type_def(cs, type);
    if (cst && cst->destroy)
      cst->destroy(cs, cdef->var, cdef);

    /* If we allocated the initial value, clean it up */
    if (cdef->type & DT_INITIAL_SET)
      FREE(&cdef->initial);
  }
}

/**
 * create_synonym - Create an alternative name for a config item
 * @param cs   Config items
 * @param cdef Variable definition
 * @param err  Buffer for error messages
 * @retval ptr New HashElem representing the config item synonym
 */
static struct HashElem *create_synonym(const struct ConfigSet *cs,
                                       struct ConfigDef *cdef, struct Buffer *err)
{
  if (!cs || !cdef)
    return NULL; /* LCOV_EXCL_LINE */

  const char *name = (const char *) cdef->initial;
  struct HashElem *parent = cs_get_elem(cs, name);
  if (!parent)
  {
    mutt_buffer_printf(err, "No such variable: %s", name);
    return NULL;
  }

  struct HashElem *child =
      mutt_hash_typed_insert(cs->hash, cdef->name, cdef->type, (void *) cdef);
  if (!child)
    return NULL;

  cdef->var = parent;
  return child;
}

/**
 * reg_one_var - Register one config item
 * @param cs   Config items
 * @param cdef Variable definition
 * @param err  Buffer for error messages
 * @retval ptr New HashElem representing the config item
 */
static struct HashElem *reg_one_var(const struct ConfigSet *cs,
                                    struct ConfigDef *cdef, struct Buffer *err)
{
  if (!cs || !cdef)
    return NULL; /* LCOV_EXCL_LINE */

  if (cdef->type == DT_SYNONYM)
    return create_synonym(cs, cdef, err);

  const struct ConfigSetType *cst = cs_get_type_def(cs, cdef->type);
  if (!cst)
  {
    mutt_buffer_printf(err, "Variable '%s' has an invalid type %d", cdef->name, cdef->type);
    return NULL;
  }

  struct HashElem *he =
      mutt_hash_typed_insert(cs->hash, cdef->name, cdef->type, (void *) cdef);
  if (!he)
    return NULL;

  if (cst && cst->reset)
    cst->reset(cs, cdef->var, cdef, err);

  return he;
}

/**
 * cs_create - Create a new Config Set
 * @param size Number of expected config items
 * @retval ptr New ConfigSet object
 */
struct ConfigSet *cs_create(size_t size)
{
  struct ConfigSet *cs = mutt_mem_malloc(sizeof(*cs));
  cs_init(cs, size);
  return cs;
}

/**
 * cs_init - Initialise a Config Set
 * @param cs   Config items
 * @param size Number of expected config items
 */
void cs_init(struct ConfigSet *cs, size_t size)
{
  if (!cs)
    return; /* LCOV_EXCL_LINE */

  memset(cs, 0, sizeof(*cs));
  cs->hash = mutt_hash_create(size, 0);
  mutt_hash_set_destructor(cs->hash, destroy, (intptr_t) cs);
}

/**
 * cs_free - Free a Config Set
 * @param cs Config items
 */
void cs_free(struct ConfigSet **cs)
{
  if (!cs || !*cs)
    return; /* LCOV_EXCL_LINE */

  mutt_hash_destroy(&(*cs)->hash);
  FREE(cs);
}

/**
 * cs_get_elem - Get the HashElem representing a config item
 * @param cs   Config items
 * @param name Name of config item
 * @retval ptr HashElem representing the config item
 */
struct HashElem *cs_get_elem(const struct ConfigSet *cs, const char *name)
{
  if (!cs || !name)
    return NULL; /* LCOV_EXCL_LINE */

  struct HashElem *he = mutt_hash_find_elem(cs->hash, name);
  if (!he)
    return NULL;

  if (he->type != DT_SYNONYM)
    return he;

  const struct ConfigDef *cdef = he->data;

  return cdef->var;
}

/**
 * cs_get_type_def - Get the definition for a type
 * @param cs   Config items
 * @param type Type to lookup, e.g. #DT_NUMBER
 * @retval ptr ConfigSetType representing the type
 */
const struct ConfigSetType *cs_get_type_def(const struct ConfigSet *cs, unsigned int type)
{
  if (!cs)
    return NULL; /* LCOV_EXCL_LINE */

  type = DTYPE(type);
  if ((type < 1) || (type >= mutt_array_size(cs->types)))
    return NULL;

  if (!cs->types[type].name)
    return NULL;

  return &cs->types[type];
}

/**
 * cs_register_type - Register a type of config item
 * @param cs   Config items
 * @param type Object type, e.g. #DT_BOOL
 * @param cst  Structure defining the type
 * @retval bool True, if type was registered successfully
 */
bool cs_register_type(struct ConfigSet *cs, unsigned int type, const struct ConfigSetType *cst)
{
  if (!cs || !cst)
    return false; /* LCOV_EXCL_LINE */

  if (!cst->name || !cst->string_set || !cst->string_get || !cst->reset ||
      !cst->native_set || !cst->native_get)
    return false;

  if (type >= mutt_array_size(cs->types))
    return false;

  if (cs->types[type].name)
    return false; /* already registered */

  cs->types[type] = *cst;
  return true;
}

/**
 * cs_register_variables - Register a set of config items
 * @param cs    Config items
 * @param vars  Variable definition
 * @param flags Flags, e.g. #CS_REG_DISABLED
 * @retval bool True, if all variables were registered successfully
 */
bool cs_register_variables(const struct ConfigSet *cs, struct ConfigDef vars[], int flags)
{
  if (!cs || !vars)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  struct Buffer err;
  mutt_buffer_init(&err);
  err.data = calloc(1, STRING);
  err.dsize = STRING;

  bool rc = true;

  for (size_t i = 0; vars[i].name; i++)
  {
    if (!reg_one_var(cs, &vars[i], &err))
    {
      mutt_debug(1, "%s\n", err.data);
      rc = false;
    }
  }

  FREE(&err.data);
  return rc;
}

/**
 * cs_inherit_variable - Create in inherited config item
 * @param cs     Config items
 * @param parent HashElem of parent config item
 * @param name   Name of account-specific config item
 * @retval ptr New HashElem representing the inherited config item
 */
struct HashElem *cs_inherit_variable(const struct ConfigSet *cs,
                                     struct HashElem *parent, const char *name)
{
  if (!cs || !parent)
    return NULL; /* LCOV_EXCL_LINE */

  struct Buffer err;
  mutt_buffer_init(&err);
  err.data = calloc(1, STRING);
  err.dsize = STRING;

  struct Inheritance *i = mutt_mem_calloc(1, sizeof(*i));
  i->parent = parent;
  i->name = mutt_str_strdup(name);

  struct HashElem *he = mutt_hash_typed_insert(cs->hash, i->name, DT_INHERITED, i);
  if (!he)
  {
    FREE(&i->name);
    FREE(&i);
  }

  FREE(&err.data);
  return he;
}

/**
 * cs_add_listener - Add a listener (callback function)
 * @param cs Config items
 * @param fn Listener callback function
 */
void cs_add_listener(struct ConfigSet *cs, cs_listener fn)
{
  if (!cs || !fn)
    return; /* LCOV_EXCL_LINE */

  for (size_t i = 0; i < mutt_array_size(cs->listeners); i++)
  {
    if (cs->listeners[i] == fn)
    {
      mutt_debug(1, "Listener was already registered\n");
      return;
    }
  }

  for (size_t i = 0; i < mutt_array_size(cs->listeners); i++)
  {
    if (!cs->listeners[i])
    {
      cs->listeners[i] = fn;
      return;
    }
  }
}

/**
 * cs_remove_listener - Remove a listener (callback function)
 * @param cs Config items
 * @param fn Listener callback function
 */
void cs_remove_listener(struct ConfigSet *cs, cs_listener fn)
{
  if (!cs || !fn)
    return; /* LCOV_EXCL_LINE */

  for (size_t i = 0; i < mutt_array_size(cs->listeners); i++)
  {
    if (cs->listeners[i] == fn)
    {
      cs->listeners[i] = NULL;
      return;
    }
  }
  mutt_debug(1, "Listener wasn't registered\n");
}

/**
 * cs_notify_listeners - Notify all listeners of an event
 * @param cs   Config items
 * @param he   HashElem representing config item
 * @param name Name of config item
 * @param ev   Type of event
 */
void cs_notify_listeners(const struct ConfigSet *cs, struct HashElem *he,
                         const char *name, enum ConfigEvent ev)
{
  if (!cs || !he || !name)
    return; /* LCOV_EXCL_LINE */

  for (size_t i = 0; i < mutt_array_size(cs->listeners); i++)
  {
    if (!cs->listeners[i])
      return;

    cs->listeners[i](cs, he, name, ev);
  }
}

/**
 * cs_he_reset - Reset a config item to its initial value
 * @param cs   Config items
 * @param he   HashElem representing config item
 * @param err  Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
int cs_he_reset(const struct ConfigSet *cs, struct HashElem *he, struct Buffer *err)
{
  if (!cs || !he)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  /* An inherited var that's already pointing to its parent.
   * Return 'success', but don't send a notification. */
  if ((he->type & DT_INHERITED) && (DTYPE(he->type) == 0))
    return CSR_SUCCESS;

  const struct ConfigSetType *cst = NULL;
  const struct ConfigDef *cdef = NULL;

  int rc = CSR_SUCCESS;

  if (he->type & DT_INHERITED)
  {
    struct Inheritance *i = he->data;
    cst = cs_get_type_def(cs, i->parent->type);
    cdef = i->parent->data;

    if (cst && cst->destroy)
      cst->destroy(cs, (void **) &i->var, cdef);

    he->type = DT_INHERITED;
  }
  else
  {
    cst = cs_get_type_def(cs, he->type);
    cdef = he->data;

    if (cst)
      rc = cst->reset(cs, cdef->var, cdef, err);
  }

  if ((CSR_RESULT(rc) == CSR_SUCCESS) && !(rc & CSR_SUC_NO_CHANGE))
    cs_notify_listeners(cs, he, he->key.strkey, CE_RESET);
  return rc;
}

/**
 * cs_str_reset - Reset a config item to its initial value
 * @param cs   Config items
 * @param name Name of config item
 * @param err  Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
int cs_str_reset(const struct ConfigSet *cs, const char *name, struct Buffer *err)
{
  if (!cs || !name)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  struct HashElem *he = cs_get_elem(cs, name);
  if (!he)
  {
    mutt_buffer_printf(err, "Unknown var '%s'", name);
    return CSR_ERR_UNKNOWN;
  }

  return cs_he_reset(cs, he, err);
}

/**
 * cs_he_initial_set - Set the initial value of a config item
 * @param cs    Config items
 * @param he    HashElem representing config item
 * @param value Value to set
 * @param err   Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
int cs_he_initial_set(const struct ConfigSet *cs, struct HashElem *he,
                      const char *value, struct Buffer *err)
{
  if (!cs || !he)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  struct ConfigDef *cdef = NULL;
  const struct ConfigSetType *cst = NULL;

  if (he->type & DT_INHERITED)
  {
    struct Inheritance *i = he->data;
    cdef = i->parent->data;
    mutt_debug(1, "Variable '%s' is inherited type.\n", cdef->name);
    return CSR_ERR_CODE;
  }

  cdef = he->data;
  cst = cs_get_type_def(cs, he->type);
  if (!cst)
  {
    mutt_debug(1, "Variable '%s' has an invalid type %d\n", cdef->name, he->type);
    return CSR_ERR_CODE;
  }

  int rc = cst->string_set(cs, NULL, cdef, value, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
    return rc;

  cs_notify_listeners(cs, he, he->key.strkey, CE_INITIAL_SET);
  return CSR_SUCCESS;
}

/**
 * cs_str_initial_set - Set the initial value of a config item
 * @param cs    Config items
 * @param name  Name of config item
 * @param value Value to set
 * @param err   Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
int cs_str_initial_set(const struct ConfigSet *cs, const char *name,
                       const char *value, struct Buffer *err)
{
  if (!cs || !name)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  struct HashElem *he = cs_get_elem(cs, name);
  if (!he)
  {
    mutt_buffer_printf(err, "Unknown var '%s'", name);
    return CSR_ERR_UNKNOWN;
  }

  return cs_he_initial_set(cs, he, value, err);
}

/**
 * cs_he_initial_get - Get the initial, or parent, value of a config item
 * @param cs     Config items
 * @param he     HashElem representing config item
 * @param result Buffer for results or error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 *
 * If a config item is inherited from another, then this will get the parent's
 * value.  Otherwise, it will get the config item's initial value.
 */
int cs_he_initial_get(const struct ConfigSet *cs, struct HashElem *he, struct Buffer *result)
{
  if (!cs || !he)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  struct Inheritance *i = NULL;
  const struct ConfigDef *cdef = NULL;
  const struct ConfigSetType *cst = NULL;

  if (he->type & DT_INHERITED)
  {
    i = he->data;
    cdef = i->parent->data;
    cst = cs_get_type_def(cs, i->parent->type);
  }
  else
  {
    cdef = he->data;
    cst = cs_get_type_def(cs, he->type);
  }

  if (!cst)
  {
    mutt_debug(1, "Variable '%s' has an invalid type %d\n", cdef->name, DTYPE(he->type));
    return CSR_ERR_CODE;
  }

  return cst->string_get(cs, NULL, cdef, result);
}

/**
 * cs_str_initial_get - Get the initial, or parent, value of a config item
 * @param cs     Config items
 * @param name   Name of config item
 * @param result Buffer for results or error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 *
 * If a config item is inherited from another, then this will get the parent's
 * value.  Otherwise, it will get the config item's initial value.
 */
int cs_str_initial_get(const struct ConfigSet *cs, const char *name, struct Buffer *result)
{
  if (!cs || !name)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  struct HashElem *he = cs_get_elem(cs, name);
  if (!he)
  {
    mutt_buffer_printf(result, "Unknown var '%s'", name);
    return CSR_ERR_UNKNOWN;
  }

  return cs_he_initial_get(cs, he, result);
}

/**
 * cs_he_string_set - Set a config item by string
 * @param cs    Config items
 * @param he    HashElem representing config item
 * @param value Value to set
 * @param err   Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
int cs_he_string_set(const struct ConfigSet *cs, struct HashElem *he,
                     const char *value, struct Buffer *err)
{
  if (!cs || !he)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  struct ConfigDef *cdef = NULL;
  const struct ConfigSetType *cst = NULL;
  void *var = NULL;

  if (he->type & DT_INHERITED)
  {
    struct Inheritance *i = he->data;
    cdef = i->parent->data;
    var = &i->var;
    cst = cs_get_type_def(cs, i->parent->type);
  }
  else
  {
    cdef = he->data;
    var = cdef->var;
    cst = cs_get_type_def(cs, he->type);
  }

  if (!cst)
  {
    mutt_debug(1, "Variable '%s' has an invalid type %d\n", cdef->name, he->type);
    return CSR_ERR_CODE;
  }

  if (!var)
    return CSR_ERR_CODE;

  int rc = cst->string_set(cs, var, cdef, value, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
    return rc;

  if (he->type & DT_INHERITED)
  {
    struct Inheritance *i = he->data;
    he->type = i->parent->type | DT_INHERITED;
  }
  if (!(rc & CSR_SUC_NO_CHANGE))
    cs_notify_listeners(cs, he, he->key.strkey, CE_SET);
  return rc;
}

/**
 * cs_str_string_set - Set a config item by string
 * @param cs    Config items
 * @param name  Name of config item
 * @param value Value to set
 * @param err   Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
int cs_str_string_set(const struct ConfigSet *cs, const char *name,
                      const char *value, struct Buffer *err)
{
  if (!cs || !name)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  struct HashElem *he = cs_get_elem(cs, name);
  if (!he)
  {
    mutt_buffer_printf(err, "Unknown var '%s'", name);
    return CSR_ERR_UNKNOWN;
  }

  return cs_he_string_set(cs, he, value, err);
}

/**
 * cs_he_string_get - Get a config item as a string
 * @param cs     Config items
 * @param he     HashElem representing config item
 * @param result Buffer for results or error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
int cs_he_string_get(const struct ConfigSet *cs, struct HashElem *he, struct Buffer *result)
{
  if (!cs || !he)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  struct Inheritance *i = NULL;
  const struct ConfigDef *cdef = NULL;
  const struct ConfigSetType *cst = NULL;
  void *var = NULL;

  if (he->type & DT_INHERITED)
  {
    i = he->data;
    cdef = i->parent->data;
    cst = cs_get_type_def(cs, i->parent->type);
  }
  else
  {
    cdef = he->data;
    cst = cs_get_type_def(cs, he->type);
  }

  if ((he->type & DT_INHERITED) && (DTYPE(he->type) != 0))
  {
    var = &i->var; /* Local value */
  }
  else
  {
    var = cdef->var; /* Normal var */
  }

  if (!cst)
  {
    mutt_debug(1, "Variable '%s' has an invalid type %d\n", cdef->name, DTYPE(he->type));
    return CSR_ERR_CODE;
  }

  return cst->string_get(cs, var, cdef, result);
}

/**
 * cs_str_string_get - Get a config item as a string
 * @param cs     Config items
 * @param name   Name of config item
 * @param result Buffer for results or error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
int cs_str_string_get(const struct ConfigSet *cs, const char *name, struct Buffer *result)
{
  if (!cs || !name)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  struct HashElem *he = cs_get_elem(cs, name);
  if (!he)
  {
    mutt_buffer_printf(result, "Unknown var '%s'", name);
    return CSR_ERR_UNKNOWN;
  }

  return cs_he_string_get(cs, he, result);
}

/**
 * cs_he_native_set - Natively set the value of a HashElem config item
 * @param cs    Config items
 * @param he    HashElem representing config item
 * @param value Native pointer/value to set
 * @param err   Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
int cs_he_native_set(const struct ConfigSet *cs, struct HashElem *he,
                     intptr_t value, struct Buffer *err)
{
  if (!cs || !he)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  const struct ConfigDef *cdef = NULL;
  const struct ConfigSetType *cst = NULL;
  void *var = NULL;

  if (he->type & DT_INHERITED)
  {
    struct Inheritance *i = he->data;
    cdef = i->parent->data;
    var = &i->var;
    cst = cs_get_type_def(cs, i->parent->type);
  }
  else
  {
    cdef = he->data;
    var = cdef->var;
    cst = cs_get_type_def(cs, he->type);
  }

  if (!cst)
  {
    mutt_debug(1, "Variable '%s' has an invalid type %d\n", cdef->name, he->type);
    return CSR_ERR_CODE;
  }

  int rc = cst->native_set(cs, var, cdef, value, err);
  if (CSR_RESULT(rc) == CSR_SUCCESS)
  {
    if (he->type & DT_INHERITED)
      he->type = cdef->type | DT_INHERITED;
    if (!(rc & CSR_SUC_NO_CHANGE))
      cs_notify_listeners(cs, he, cdef->name, CE_SET);
  }

  return rc;
}

/**
 * cs_str_native_set - Natively set the value of a string config item
 * @param cs    Config items
 * @param name  Name of config item
 * @param value Native pointer/value to set
 * @param err   Buffer for error messages
 * @retval int Result, e.g. #CSR_SUCCESS
 */
int cs_str_native_set(const struct ConfigSet *cs, const char *name,
                      intptr_t value, struct Buffer *err)
{
  if (!cs || !name)
    return CSR_ERR_CODE; /* LCOV_EXCL_LINE */

  struct HashElem *he = cs_get_elem(cs, name);
  if (!he)
  {
    mutt_buffer_printf(err, "Unknown var '%s'", name);
    return CSR_ERR_UNKNOWN;
  }

  const struct ConfigDef *cdef = NULL;
  const struct ConfigSetType *cst = NULL;
  void *var = NULL;

  if (he->type & DT_INHERITED)
  {
    struct Inheritance *i = he->data;
    cdef = i->parent->data;
    var = &i->var;
    cst = cs_get_type_def(cs, i->parent->type);
  }
  else
  {
    cdef = he->data;
    var = cdef->var;
    cst = cs_get_type_def(cs, he->type);
  }

  if (!cst)
  {
    mutt_debug(1, "Variable '%s' has an invalid type %d\n", cdef->name, he->type);
    return CSR_ERR_CODE;
  }

  int rc = cst->native_set(cs, var, cdef, value, err);
  if (CSR_RESULT(rc) == CSR_SUCCESS)
  {
    if (he->type & DT_INHERITED)
      he->type = cdef->type | DT_INHERITED;
    if (!(rc & CSR_SUC_NO_CHANGE))
      cs_notify_listeners(cs, he, cdef->name, CE_SET);
  }

  return rc;
}

/**
 * cs_he_native_get - Natively get the value of a HashElem config item
 * @param cs  Config items
 * @param he  HashElem representing config item
 * @param err Buffer for results or error messages
 * @retval intptr_t Native pointer/value
 */
intptr_t cs_he_native_get(const struct ConfigSet *cs, struct HashElem *he, struct Buffer *err)
{
  if (!cs || !he)
    return INT_MIN; /* LCOV_EXCL_LINE */

  struct Inheritance *i = NULL;
  const struct ConfigDef *cdef = NULL;
  const struct ConfigSetType *cst = NULL;
  void *var = NULL;

  if (he->type & DT_INHERITED)
  {
    i = he->data;
    cdef = i->parent->data;
    cst = cs_get_type_def(cs, i->parent->type);
  }
  else
  {
    cdef = he->data;
    cst = cs_get_type_def(cs, he->type);
  }

  if ((he->type & DT_INHERITED) && (DTYPE(he->type) != 0))
  {
    var = &i->var;
  }
  else
  {
    var = cdef->var;
  }

  if (!cst)
  {
    mutt_buffer_printf(err, "Variable '%s' has an invalid type %d", cdef->name, he->type);
    return INT_MIN;
  }

  return cst->native_get(cs, var, cdef, err);
}

/**
 * cs_str_native_get - Natively get the value of a string config item
 * @param cs   Config items
 * @param name Name of config item
 * @param err  Buffer for error messages
 * @retval intptr_t Native pointer/value
 */
intptr_t cs_str_native_get(const struct ConfigSet *cs, const char *name, struct Buffer *err)
{
  if (!cs || !name)
    return INT_MIN; /* LCOV_EXCL_LINE */

  struct HashElem *he = cs_get_elem(cs, name);
  return cs_he_native_get(cs, he, err);
}
