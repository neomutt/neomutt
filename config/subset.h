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
struct HashElem;

/**
 * struct ConfigSubset - A set of inherited config items
 */
struct ConfigSubset
{
  char *name;                  ///< Scope name of Subset
  struct ConfigSubset *parent; ///< Parent Subset
  struct ConfigSet *cs;        ///< Parent ConfigSet
};

struct ConfigSubset *cs_subset_new       (const char *name, struct ConfigSubset *parent);
void                 cs_subset_free      (struct ConfigSubset **ptr);
struct HashElem *    cs_subset_lookup    (const struct ConfigSubset *sub, const char *name);
struct HashElem *    cs_subset_create_var(const struct ConfigSubset *sub, const char *name, struct Buffer *err);

intptr_t             cs_subset_native_get(const struct ConfigSubset *sub, struct HashElem *he,                    struct Buffer *err);
int                  cs_subset_native_set(const struct ConfigSubset *sub, struct HashElem *he, intptr_t value,    struct Buffer *err);
int                  cs_subset_reset     (const struct ConfigSubset *sub, struct HashElem *he,                    struct Buffer *err);
int                  cs_subset_string_get(const struct ConfigSubset *sub, struct HashElem *he,                    struct Buffer *result);
int                  cs_subset_string_set(const struct ConfigSubset *sub, struct HashElem *he, const char *value, struct Buffer *err);

#endif /* MUTT_CONFIG_SUBSET_H */
