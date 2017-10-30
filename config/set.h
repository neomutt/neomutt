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

#ifndef _CONFIG_SET_H
#define _CONFIG_SET_H

#include <stdbool.h>
#include <stdint.h>

struct Buffer;
struct ConfigSet;
struct HashElem;
struct ConfigDef;

/**
 * enum ConfigEvent - Config notification types
 */
enum ConfigEvent
{
  CE_SET = 1,   /**< Config item has been set */
  CE_RESET, /**< Config item has been reset to initial, or parent, value */
  CE_INITIAL_SET, /**< Config item's initial value has been set */
};

/* Config Set Results */
#define CSR_SUCCESS       0 /**< Action completed successfully */
#define CSR_ERR_CODE      1 /**< Problem with the code */
#define CSR_ERR_UNKNOWN   2 /**< Unrecognised config item */
#define CSR_ERR_INVALID   3 /**< Value hasn't been set */

/* Flags for CSR_SUCCESS */
#define CSR_SUC_INHERITED (1 << 4) /**< Value is inherited */
#define CSR_SUC_EMPTY     (1 << 5) /**< Value is empty/unset */
#define CSR_SUC_WARNING   (1 << 6) /**< Notify the user of a warning */
#define CSR_SUC_NO_CHANGE (1 << 7) /**< The value hasn't changed */

/* Flags for CSR_INVALID */
#define CSR_INV_TYPE      (1 << 4) /**< Value is not valid for the type */
#define CSR_INV_VALIDATOR (1 << 5) /**< Value was rejected by the validator */

#define CSR_RESULT_MASK 0x0F
#define CSR_RESULT(x) ((x) & CSR_RESULT_MASK)

/**
 * enum CsListenerAction - Config Listener responses
 */
enum CsListenerAction
{
  CSLA_CONTINUE = 1, /**< Continue notifying listeners */
  CSLA_STOP,         /**< Stop notifying listeners */
};

typedef bool    (*cs_listener)   (const struct ConfigSet *cs, struct HashElem *he, const char *name, enum ConfigEvent ev);
typedef int     (*cs_validator)  (const struct ConfigSet *cs, const struct ConfigDef *cdef, intptr_t value, struct Buffer *err);

typedef int     (*cst_string_set)(const struct ConfigSet *cs, void *var,       struct ConfigDef *cdef, const char *value, struct Buffer *err);
typedef int     (*cst_string_get)(const struct ConfigSet *cs, void *var, const struct ConfigDef *cdef,                    struct Buffer *result);
typedef int     (*cst_native_set)(const struct ConfigSet *cs, void *var, const struct ConfigDef *cdef, intptr_t value,    struct Buffer *err);
typedef intptr_t(*cst_native_get)(const struct ConfigSet *cs, void *var, const struct ConfigDef *cdef,                    struct Buffer *err);

typedef int     (*cst_reset)     (const struct ConfigSet *cs, void *var, const struct ConfigDef *cdef, struct Buffer *err);
typedef void    (*cst_destroy)   (const struct ConfigSet *cs, void *var, const struct ConfigDef *cdef);

#define IP (intptr_t)

#define CS_REG_DISABLED (1 << 0)

/**
 * struct ConfigDef - Config item definition
 *
 * Every config variable that NeoMutt supports is backed by a ConfigDef.
 */
struct ConfigDef
{
  const char   *name;      /**< User-visible name */
  unsigned int  type;      /**< Variable type, e.g. #DT_STRING */
  intptr_t      flags;     /**< Notification flags, e.g. #R_PAGER */
  void         *var;       /**< Pointer to the global variable */
  intptr_t      initial;   /**< Initial value */
  cs_validator  validator; /**< Validator callback function */
};

/**
 * struct ConfigSetType - Type definition for a config item
 *
 * Each config item has a type which is defined by a set of callback functions.
 */
struct ConfigSetType
{
  const char *name;          /**< Name of the type, e.g. "String" */
  cst_string_set string_set; /**< Convert the variable to a string */
  cst_string_get string_get; /**< Initialise a variable from a string */
  cst_native_set native_set; /**< Set the variable using a C-native type */
  cst_native_get native_get; /**< Get the variable's value as a C-native type */
  cst_reset reset;           /**< Reset the variable to its initial, or parent, value */
  cst_destroy destroy;       /**< Free the resources for a variable */
};

/**
 * struct ConfigSet - Container for lots of config items
 *
 * The config items are stored in a HashTable so that their names can be looked
 * up efficiently.  Each config item is repesented by a HashElem.  Once
 * created, this HashElem is static and may be used for the lifetime of the
 * ConfigSet.
 */
struct ConfigSet
{
  struct Hash *hash;              /**< HashTable storing the config itesm */
  struct ConfigSetType types[18]; /**< All the defined config types */
  cs_listener listeners[8];       /**< Listeners for notifications of changes to config items */
};

struct ConfigSet *cs_create(size_t size);
void              cs_init(struct ConfigSet *cs, size_t size);
void              cs_free(struct ConfigSet **cs);

struct HashElem *           cs_get_elem(const struct ConfigSet *cs, const char *name);
const struct ConfigSetType *cs_get_type_def(const struct ConfigSet *cs, unsigned int type);

bool             cs_register_type(struct ConfigSet *cs, unsigned int type, const struct ConfigSetType *cst);
bool             cs_register_variables(const struct ConfigSet *cs, struct ConfigDef vars[], int flags);
struct HashElem *cs_inherit_variable(const struct ConfigSet *cs, struct HashElem *parent, const char *name);

void cs_add_listener(struct ConfigSet *cs, cs_listener fn);
void cs_remove_listener(struct ConfigSet *cs, cs_listener fn);
void cs_notify_listeners(const struct ConfigSet *cs, struct HashElem *he, const char *name, enum ConfigEvent ev);

int      cs_he_initial_get (const struct ConfigSet *cs, struct HashElem *he,                    struct Buffer *result);
int      cs_he_initial_set (const struct ConfigSet *cs, struct HashElem *he, const char *value, struct Buffer *err);
intptr_t cs_he_native_get  (const struct ConfigSet *cs, struct HashElem *he,                    struct Buffer *err);
int      cs_he_native_set  (const struct ConfigSet *cs, struct HashElem *he, intptr_t value,    struct Buffer *err);
int      cs_he_reset       (const struct ConfigSet *cs, struct HashElem *he,                    struct Buffer *err);
int      cs_he_string_get  (const struct ConfigSet *cs, struct HashElem *he,                    struct Buffer *result);
int      cs_he_string_set  (const struct ConfigSet *cs, struct HashElem *he, const char *value, struct Buffer *err);

int      cs_str_initial_get(const struct ConfigSet *cs, const char *name,                       struct Buffer *result);
int      cs_str_initial_set(const struct ConfigSet *cs, const char *name,    const char *value, struct Buffer *err);
intptr_t cs_str_native_get (const struct ConfigSet *cs, const char *name,                       struct Buffer *err);
int      cs_str_native_set (const struct ConfigSet *cs, const char *name,    intptr_t value,    struct Buffer *err);
int      cs_str_reset      (const struct ConfigSet *cs, const char *name,                       struct Buffer *err);
int      cs_str_string_get (const struct ConfigSet *cs, const char *name,                       struct Buffer *result);
int      cs_str_string_set (const struct ConfigSet *cs, const char *name,    const char *value, struct Buffer *err);

#endif /* _CONFIG_SET_H */
