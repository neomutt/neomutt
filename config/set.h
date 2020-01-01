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

#ifndef MUTT_CONFIG_SET_H
#define MUTT_CONFIG_SET_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "subset.h"

struct Buffer;
struct ConfigSet;
struct HashElem;
struct ConfigDef;

/* Config Set Results */
#define CSR_SUCCESS       0 ///< Action completed successfully
#define CSR_ERR_CODE      1 ///< Problem with the code
#define CSR_ERR_UNKNOWN   2 ///< Unrecognised config item
#define CSR_ERR_INVALID   3 ///< Value hasn't been set

/* Flags for CSR_SUCCESS */
#define CSR_SUC_INHERITED (1 << 4) ///< Value is inherited
#define CSR_SUC_EMPTY     (1 << 5) ///< Value is empty/unset
#define CSR_SUC_WARNING   (1 << 6) ///< Notify the user of a warning
#define CSR_SUC_NO_CHANGE (1 << 7) ///< The value hasn't changed

/* Flags for CSR_INVALID */
#define CSR_INV_TYPE      (1 << 4) ///< Value is not valid for the type
#define CSR_INV_VALIDATOR (1 << 5) ///< Value was rejected by the validator

#define CSR_RESULT_MASK 0x0F
#define CSR_RESULT(x) ((x) & CSR_RESULT_MASK)

/**
 * typedef cs_validator - Validate a config variable
 * @param cs    Config items
 * @param cdef  Config definition
 * @param value Native value
 * @param err   Message for the user
 * @retval #CSR_SUCCESS     Success
 * @retval #CSR_ERR_INVALID Failure
 */
typedef int (*cs_validator)(const struct ConfigSet *cs, const struct ConfigDef *cdef, intptr_t value, struct Buffer *err);
/**
 * typedef cst_string_set - Set a config item by string
 * @param cs    Config items
 * @param var   Variable to set
 * @param cdef  Variable definition
 * @param value Value to set
 * @param err   Buffer for error messages
 * @retval num Result, e.g. #CSR_SUCCESS
 *
 * If var is NULL, then the config item's initial value will be set.
 */
typedef int (*cst_string_set)(const struct ConfigSet *cs, void *var, struct ConfigDef *cdef, const char *value, struct Buffer *err);
/**
 * typedef cst_string_get - Get a config item as a string
 * @param cs     Config items
 * @param var    Variable to get
 * @param cdef   Variable definition
 * @param result Buffer for results or error messages
 * @retval num Result, e.g. #CSR_SUCCESS
 *
 * If var is NULL, then the config item's initial value will be returned.
 */
typedef int (*cst_string_get)(const struct ConfigSet *cs, void *var, const struct ConfigDef *cdef, struct Buffer *result);
/**
 * typedef cst_native_set - Set a config item by string
 * @param cs    Config items
 * @param var   Variable to set
 * @param cdef  Variable definition
 * @param value Native pointer/value to set
 * @param err   Buffer for error messages
 * @retval num Result, e.g. #CSR_SUCCESS
 */
typedef int (*cst_native_set)(const struct ConfigSet *cs, void *var, const struct ConfigDef *cdef, intptr_t value, struct Buffer *err);
/**
 * typedef cst_native_get - Get a string from a config item
 * @param cs   Config items
 * @param var  Variable to get
 * @param cdef Variable definition
 * @param err  Buffer for error messages
 * @retval intptr_t Config item string
 * @retval INT_MIN  Error
 */
typedef intptr_t (*cst_native_get)(const struct ConfigSet *cs, void *var, const struct ConfigDef *cdef, struct Buffer *err);
/**
 * typedef cst_reset - Reset a config item to its initial value
 * @param cs   Config items
 * @param var  Variable to reset
 * @param cdef Variable definition
 * @param err  Buffer for error messages
 * @retval num Result, e.g. #CSR_SUCCESS
 */
typedef int (*cst_reset)(const struct ConfigSet *cs, void *var, const struct ConfigDef *cdef, struct Buffer *err);
/**
 * typedef cst_destroy - Destroy a config item
 * @param cs   Config items
 * @param var  Variable to destroy
 * @param cdef Variable definition
 */
typedef void (*cst_destroy)(const struct ConfigSet *cs, void *var, const struct ConfigDef *cdef);

#define IP (intptr_t)

#define CS_REG_DISABLED (1 << 0)

/**
 * struct ConfigDef - Config item definition
 *
 * Every config variable that NeoMutt supports is backed by a ConfigDef.
 */
struct ConfigDef
{
  const char   *name;      ///< User-visible name
  unsigned int  type;      ///< Variable type, e.g. #DT_STRING
  void         *var;       ///< Pointer to the global variable
  intptr_t      initial;   ///< Initial value
  intptr_t      data;      ///< Extra variable data
  cs_validator  validator; ///< Validator callback function
};

/**
 * struct ConfigSetType - Type definition for a config item
 *
 * Each config item has a type which is defined by a set of callback functions.
 */
struct ConfigSetType
{
  const char *name;          ///< Name of the type, e.g. "String"
  cst_string_set string_set; ///< Convert the variable to a string
  cst_string_get string_get; ///< Initialise a variable from a string
  cst_native_set native_set; ///< Set the variable using a C-native type
  cst_native_get native_get; ///< Get the variable's value as a C-native type
  cst_reset reset;           ///< Reset the variable to its initial, or parent, value
  cst_destroy destroy;       ///< Free the resources for a variable
};

/**
 * struct ConfigSet - Container for lots of config items
 *
 * The config items are stored in a HashTable so that their names can be looked
 * up efficiently.  Each config item is represented by a HashElem.  Once
 * created, this HashElem is static and may be used for the lifetime of the
 * ConfigSet.
 */
struct ConfigSet
{
  struct Hash *hash;              ///< HashTable storing the config items
  struct ConfigSetType types[18]; ///< All the defined config types
};

struct ConfigSet *cs_new(size_t size);
void              cs_free(struct ConfigSet **ptr);

struct HashElem *           cs_get_base    (struct HashElem *he);
struct HashElem *           cs_get_elem    (const struct ConfigSet *cs, const char *name);
const struct ConfigSetType *cs_get_type_def(const struct ConfigSet *cs, unsigned int type);

bool             cs_register_type     (struct ConfigSet *cs, unsigned int type, const struct ConfigSetType *cst);
bool             cs_register_variables(const struct ConfigSet *cs, struct ConfigDef vars[], int flags);
struct HashElem *cs_inherit_variable  (const struct ConfigSet *cs, struct HashElem *parent, const char *name);
void             cs_uninherit_variable(const struct ConfigSet *cs, const char *name);

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

#endif /* MUTT_CONFIG_SET_H */
