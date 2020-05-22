/**
 * @file
 * Subset of Config Items
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_CONFIG_SUBSET_H
#define MUTT_CONFIG_SUBSET_H

#include <stdint.h>

struct Buffer;
struct ConfigSet;
struct HashElem;
struct Notify;

/**
 * enum ConfigScope - Who does this Config belong to?
 */
enum ConfigScope
{
  SET_SCOPE_NEOMUTT, ///< This Config is NeoMutt-specific (global)
  SET_SCOPE_ACCOUNT, ///< This Config is Account-specific
  SET_SCOPE_MAILBOX, ///< This Config is Mailbox-specific
};

/**
 * struct ConfigSubset - A set of inherited config items
 */
struct ConfigSubset
{
  const char *name;            ///< Scope name of Subset
  enum ConfigScope scope;      ///< Scope of Subset, e.g. #SET_SCOPE_ACCOUNT
  struct ConfigSubset *parent; ///< Parent Subset
  struct ConfigSet *cs;        ///< Parent ConfigSet
  struct Notify *notify;       ///< Notifications system
};

/**
 * enum NotifyConfig - Config notification types
 *
 * Observers of #NT_CONFIG will be passed an #EventConfig.
 */
enum NotifyConfig
{
  NT_CONFIG_SET = 1,     ///< Config item has been set
  NT_CONFIG_RESET,       ///< Config item has been reset to initial, or parent, value
  NT_CONFIG_INITIAL_SET, ///< Config item's initial value has been set
};

/**
 * struct EventConfig - A config-change event
 */
struct EventConfig
{
  const struct ConfigSubset *sub; ///< Config Subset
  const char *name;               ///< Name of config item that changed
  struct HashElem *he;            ///< Config item that changed
};

struct ConfigSubset *cs_subset_new (const char *name, struct ConfigSubset *sub_parent, struct Notify *not_parent);
void                 cs_subset_free(struct ConfigSubset **ptr);

struct HashElem *cs_subset_create_inheritance(const struct ConfigSubset *sub, const char *name);
struct HashElem *cs_subset_lookup            (const struct ConfigSubset *sub, const char *name);
void             cs_subset_notify_observers  (const struct ConfigSubset *sub, struct HashElem *he, enum NotifyConfig ev);

intptr_t cs_subset_he_native_get          (const struct ConfigSubset *sub, struct HashElem *he,                    struct Buffer *err);
int      cs_subset_he_native_set          (const struct ConfigSubset *sub, struct HashElem *he, intptr_t value,    struct Buffer *err);
int      cs_subset_he_reset               (const struct ConfigSubset *sub, struct HashElem *he,                    struct Buffer *err);
int      cs_subset_he_string_get          (const struct ConfigSubset *sub, struct HashElem *he,                    struct Buffer *result);
int      cs_subset_he_string_minus_equals (const struct ConfigSubset *sub, struct HashElem *he, const char *value, struct Buffer *err);
int      cs_subset_he_string_plus_equals  (const struct ConfigSubset *sub, struct HashElem *he, const char *value, struct Buffer *err);
int      cs_subset_he_string_set          (const struct ConfigSubset *sub, struct HashElem *he, const char *value, struct Buffer *err);

intptr_t cs_subset_str_native_get         (const struct ConfigSubset *sub, const char *name,                       struct Buffer *err);
int      cs_subset_str_native_set         (const struct ConfigSubset *sub, const char *name,    intptr_t value,    struct Buffer *err);
int      cs_subset_str_reset              (const struct ConfigSubset *sub, const char *name,                       struct Buffer *err);
int      cs_subset_str_string_get         (const struct ConfigSubset *sub, const char *name,                       struct Buffer *result);
int      cs_subset_str_string_minus_equals(const struct ConfigSubset *sub, const char *name,    const char *value, struct Buffer *err);
int      cs_subset_str_string_plus_equals (const struct ConfigSubset *sub, const char *name,    const char *value, struct Buffer *err);
int      cs_subset_str_string_set         (const struct ConfigSubset *sub, const char *name,    const char *value, struct Buffer *err);

int               elem_list_sort(const void *a, const void *b);
struct HashElem **get_elem_list(struct ConfigSet *cs);

#endif /* MUTT_CONFIG_SUBSET_H */
