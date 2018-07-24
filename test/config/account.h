/**
 * @file
 * A collection of account-specific config items
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

#ifndef _CONFIG_ACCOUNT_H
#define _CONFIG_ACCOUNT_H

#include <stdint.h>

struct Buffer;
struct ConfigSet;

/**
 * struct Account - Account-local config items
 */
struct Account
{
  char *name;                 /**< Name of Account */
  const struct ConfigSet *cs; /**< Parent ConfigSet */
  const char **var_names;     /**< Array of the names of local config items */
  int num_vars;               /**< Number of local config items */
  struct HashElem **vars;     /**< Array of the HashElems of local config items */
};

struct Account *ac_create(const struct ConfigSet *cs, const char *name, const char *var_names[]);
void ac_free(const struct ConfigSet *cs, struct Account **ac);

int ac_set_value(const struct Account *ac, unsigned int vid, intptr_t value, struct Buffer *err);
int ac_get_value(const struct Account *ac, unsigned int vid, struct Buffer *err);

#endif /* _CONFIG_ACCOUNT_H */
