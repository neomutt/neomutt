/**
 * @file
 * Subset of config items
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

/**
 * @page config_subset Subset of config items
 *
 * Subset of config items
 */

#include "config.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "mutt/lib.h"
#include "subset.h"
#include "set.h"

struct Notify;

/**
 * elem_list_sort - Sort two HashElem pointers to config
 * @param a First HashElem
 * @param b Second HashElem
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int elem_list_sort(const void *a, const void *b)
{
  if (!a || !b)
    return 0;

  const struct HashElem *hea = *(struct HashElem const *const *) a;
  const struct HashElem *heb = *(struct HashElem const *const *) b;

  return mutt_istr_cmp(hea->key.strkey, heb->key.strkey);
}

/**
 * get_elem_list - Create a sorted list of all config items
 * @param cs ConfigSet to read
 * @retval ptr Null-terminated array of HashElem
 */
struct HashElem **get_elem_list(struct ConfigSet *cs)
{
  if (!cs)
    return NULL;

  struct HashElem **list = mutt_mem_calloc(1024, sizeof(struct HashElem *));
  size_t index = 0;

  struct HashWalkState walk = { 0 };
  struct HashElem *he = NULL;

  while ((he = mutt_hash_walk(cs->hash, &walk)))
  {
    list[index++] = he;
    if (index == 1022)
      break; /* LCOV_EXCL_LINE */
  }

  qsort(list, index, sizeof(struct HashElem *), elem_list_sort);

  return list;
}

/**
 * cs_subset_free - Free a Config Subset
 * @param ptr Subset to free
 *
 * @note Config items matching this Subset will be freed
 */
void cs_subset_free(struct ConfigSubset **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct ConfigSubset *sub = *ptr;

  if (sub->cs && sub->name)
  {
    char scope[256];
    snprintf(scope, sizeof(scope), "%s:", sub->name);

    // We don't know if any config items have been set,
    // so search for anything with a matching scope.
    struct HashElem **list = get_elem_list(sub->cs);
    for (size_t i = 0; list[i]; i++)
    {
      const char *item = list[i]->key.strkey;
      if (mutt_str_startswith(item, scope) != 0)
      {
        cs_uninherit_variable(sub->cs, item);
      }
    }
    FREE(&list);
  }

  notify_free(&sub->notify);
  FREE(&sub->name);
  FREE(ptr);
}

/**
 * cs_subset_new - Create a new Config Subset
 * @param name   Name for this Subset
 * @param sub_parent Parent Subset
 * @param not_parent Parent Notification
 * @retval ptr New Subset
 *
 * @note The name will be combined with the parents' names
 */
struct ConfigSubset *cs_subset_new(const char *name, struct ConfigSubset *sub_parent,
                                   struct Notify *not_parent)
{
  struct ConfigSubset *sub = mutt_mem_calloc(1, sizeof(*sub));

  if (sub_parent)
  {
    sub->parent = sub_parent;
    sub->cs = sub_parent->cs;
  }

  if (name)
  {
    char scope[256];

    if (sub_parent && sub_parent->name)
      snprintf(scope, sizeof(scope), "%s:%s", sub_parent->name, name);
    else
      mutt_str_copy(scope, name, sizeof(scope));

    sub->name = mutt_str_dup(scope);
  }

  sub->notify = notify_new();
  notify_set_parent(sub->notify, not_parent);

  return sub;
}

/**
 * cs_subset_lookup - Find an inherited config item
 * @param sub  Subset to search
 * @param name Name of Config item to find
 * @retval ptr HashElem of the config item
 */
struct HashElem *cs_subset_lookup(const struct ConfigSubset *sub, const char *name)
{
  if (!sub || !name)
    return NULL;

  char scope[256];
  if (sub->name)
    snprintf(scope, sizeof(scope), "%s:%s", sub->name, name);
  else
    mutt_str_copy(scope, name, sizeof(scope));

  return cs_get_elem(sub->cs, scope);
}

/**
 * cs_subset_create_inheritance - Create a Subset config item (inherited)
 * @param sub  Config Subset
 * @param name Name of config item
 * @retval ptr  HashElem of the config item
 * @retval NULL Error
 */
struct HashElem *cs_subset_create_inheritance(const struct ConfigSubset *sub, const char *name)
{
  if (!sub)
    return NULL;

  struct HashElem *he = cs_subset_lookup(sub, name);
  if (he)
    return he;

  if (sub->parent)
  {
    // Create parent before creating name
    he = cs_subset_create_inheritance(sub->parent, name);
  }

  if (!he)
    return NULL;

  char scope[256];
  snprintf(scope, sizeof(scope), "%s:%s", sub->name, name);
  return cs_inherit_variable(sub->cs, he, scope);
}

/**
 * cs_subset_notify_observers - Notify all observers of an event
 * @param sub  Config Subset
 * @param he   HashElem representing config item
 * @param ev   Type of event
 */
void cs_subset_notify_observers(const struct ConfigSubset *sub,
                                struct HashElem *he, enum NotifyConfig ev)
{
  if (!sub || !he)
    return;

  struct HashElem *he_base = cs_get_base(he);
  struct EventConfig ec = { sub, he_base->key.strkey, he };
  notify_send(sub->notify, NT_CONFIG, ev, &ec);
}

/**
 * cs_subset_he_native_get - Natively get the value of a HashElem config item
 * @param sub Config Subset
 * @param he  HashElem representing config item
 * @param err Buffer for error messages
 * @retval intptr_t Native pointer/value
 * @retval INT_MIN  Error
 */
intptr_t cs_subset_he_native_get(const struct ConfigSubset *sub,
                                 struct HashElem *he, struct Buffer *err)
{
  if (!sub)
    return INT_MIN;

  return cs_he_native_get(sub->cs, he, err);
}

/**
 * cs_subset_str_native_get - Natively get the value of a string config item
 * @param sub  Config Subset
 * @param name Name of config item
 * @param err Buffer for error messages
 * @retval intptr_t Native pointer/value
 * @retval INT_MIN  Error
 */
intptr_t cs_subset_str_native_get(const struct ConfigSubset *sub,
                                  const char *name, struct Buffer *err)
{
  struct HashElem *he = cs_subset_create_inheritance(sub, name);

  return cs_subset_he_native_get(sub, he, err);
}

/**
 * cs_subset_he_native_set - Natively set the value of a HashElem config item
 * @param sub   Config Subset
 * @param he    HashElem representing config item
 * @param value Native pointer/value to set
 * @param err   Buffer for error messages
 * @retval num Result, e.g. #CSR_SUCCESS
 */
int cs_subset_he_native_set(const struct ConfigSubset *sub, struct HashElem *he,
                            intptr_t value, struct Buffer *err)
{
  if (!sub)
    return CSR_ERR_CODE;

  int rc = cs_he_native_set(sub->cs, he, value, err);

  if ((CSR_RESULT(rc) == CSR_SUCCESS) && !(rc & CSR_SUC_NO_CHANGE))
    cs_subset_notify_observers(sub, he, NT_CONFIG_SET);

  return rc;
}

/**
 * cs_subset_str_native_set - Natively set the value of a string config item
 * @param sub   Config Subset
 * @param name  Name of config item
 * @param value Native pointer/value to set
 * @param err   Buffer for error messages
 * @retval num Result, e.g. #CSR_SUCCESS
 */
int cs_subset_str_native_set(const struct ConfigSubset *sub, const char *name,
                             intptr_t value, struct Buffer *err)
{
  struct HashElem *he = cs_subset_create_inheritance(sub, name);

  return cs_subset_he_native_set(sub, he, value, err);
}

/**
 * cs_subset_he_reset - Reset a config item to its initial value
 * @param sub  Config Subset
 * @param he   HashElem representing config item
 * @param err  Buffer for error messages
 * @retval num Result, e.g. #CSR_SUCCESS
 */
int cs_subset_he_reset(const struct ConfigSubset *sub, struct HashElem *he, struct Buffer *err)
{
  if (!sub)
    return CSR_ERR_CODE;

  int rc = cs_he_reset(sub->cs, he, err);

  if ((CSR_RESULT(rc) == CSR_SUCCESS) && !(rc & CSR_SUC_NO_CHANGE))
    cs_subset_notify_observers(sub, he, NT_CONFIG_RESET);

  return rc;
}

/**
 * cs_subset_str_reset - Reset a config item to its initial value
 * @param sub  Config Subset
 * @param name Name of config item
 * @param err  Buffer for error messages
 * @retval num Result, e.g. #CSR_SUCCESS
 */
int cs_subset_str_reset(const struct ConfigSubset *sub, const char *name, struct Buffer *err)
{
  struct HashElem *he = cs_subset_create_inheritance(sub, name);

  return cs_subset_he_reset(sub, he, err);
}

/**
 * cs_subset_he_string_get - Get a config item as a string
 * @param sub    Config Subset
 * @param he     HashElem representing config item
 * @param result Buffer for results or error messages
 * @retval num Result, e.g. #CSR_SUCCESS
 */
int cs_subset_he_string_get(const struct ConfigSubset *sub, struct HashElem *he,
                            struct Buffer *result)
{
  if (!sub)
    return CSR_ERR_CODE;

  return cs_he_string_get(sub->cs, he, result);
}

/**
 * cs_subset_str_string_get - Get a config item as a string
 * @param sub    Config Subset
 * @param name   Name of config item
 * @param result Buffer for results or error messages
 * @retval num Result, e.g. #CSR_SUCCESS
 */
int cs_subset_str_string_get(const struct ConfigSubset *sub, const char *name,
                             struct Buffer *result)
{
  struct HashElem *he = cs_subset_create_inheritance(sub, name);

  return cs_subset_he_string_get(sub, he, result);
}

/**
 * cs_subset_he_string_set - Set a config item by string
 * @param sub   Config Subset
 * @param he    HashElem representing config item
 * @param value Value to set
 * @param err   Buffer for error messages
 * @retval num Result, e.g. #CSR_SUCCESS
 */
int cs_subset_he_string_set(const struct ConfigSubset *sub, struct HashElem *he,
                            const char *value, struct Buffer *err)
{
  if (!sub)
    return CSR_ERR_CODE;

  int rc = cs_he_string_set(sub->cs, he, value, err);

  if ((CSR_RESULT(rc) == CSR_SUCCESS) && !(rc & CSR_SUC_NO_CHANGE))
    cs_subset_notify_observers(sub, he, NT_CONFIG_SET);

  return rc;
}

/**
 * cs_subset_str_string_set - Set a config item by string
 * @param sub   Config Subset
 * @param name  Name of config item
 * @param value Value to set
 * @param err   Buffer for error messages
 * @retval num Result, e.g. #CSR_SUCCESS
 */
int cs_subset_str_string_set(const struct ConfigSubset *sub, const char *name,
                             const char *value, struct Buffer *err)
{
  struct HashElem *he = cs_subset_create_inheritance(sub, name);

  return cs_subset_he_string_set(sub, he, value, err);
}

/**
 * cs_subset_he_string_plus_equals - Add to a config item by string
 * @param sub   Config Subset
 * @param he    HashElem representing config item
 * @param value Value to set
 * @param err   Buffer for error messages
 * @retval num Result, e.g. #CSR_SUCCESS
 */
int cs_subset_he_string_plus_equals(const struct ConfigSubset *sub, struct HashElem *he,
                                    const char *value, struct Buffer *err)
{
  if (!sub)
    return CSR_ERR_CODE;

  int rc = cs_he_string_plus_equals(sub->cs, he, value, err);

  if ((CSR_RESULT(rc) == CSR_SUCCESS) && !(rc & CSR_SUC_NO_CHANGE))
    cs_subset_notify_observers(sub, he, NT_CONFIG_SET);

  return rc;
}

/**
 * cs_subset_str_string_plus_equals - Add to a config item by string
 * @param sub   Config Subset
 * @param name  Name of config item
 * @param value Value to set
 * @param err   Buffer for error messages
 * @retval num Result, e.g. #CSR_SUCCESS
 */
int cs_subset_str_string_plus_equals(const struct ConfigSubset *sub, const char *name,
                                     const char *value, struct Buffer *err)
{
  struct HashElem *he = cs_subset_create_inheritance(sub, name);

  return cs_subset_he_string_plus_equals(sub, he, value, err);
}

/**
 * cs_subset_he_string_minus_equals - Remove from a config item by string
 * @param sub   Config Subset
 * @param he    HashElem representing config item
 * @param value Value to set
 * @param err   Buffer for error messages
 * @retval num Result, e.g. #CSR_SUCCESS
 */
int cs_subset_he_string_minus_equals(const struct ConfigSubset *sub, struct HashElem *he,
                                     const char *value, struct Buffer *err)
{
  if (!sub)
    return CSR_ERR_CODE;

  int rc = cs_he_string_minus_equals(sub->cs, he, value, err);

  if ((CSR_RESULT(rc) == CSR_SUCCESS) && !(rc & CSR_SUC_NO_CHANGE))
    cs_subset_notify_observers(sub, he, NT_CONFIG_SET);

  return rc;
}

/**
 * cs_subset_str_string_minus_equals - Remove from a config item by string
 * @param sub   Config Subset
 * @param name  Name of config item
 * @param value Value to set
 * @param err   Buffer for error messages
 * @retval num Result, e.g. #CSR_SUCCESS
 */
int cs_subset_str_string_minus_equals(const struct ConfigSubset *sub, const char *name,
                                      const char *value, struct Buffer *err)
{
  struct HashElem *he = cs_subset_create_inheritance(sub, name);

  return cs_subset_he_string_minus_equals(sub, he, value, err);
}
