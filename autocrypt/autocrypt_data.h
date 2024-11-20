/**
 * @file
 * Private Autocrypt Data
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_AUTOCRYPT_AUTOCRYPT_DATA_H
#define MUTT_AUTOCRYPT_AUTOCRYPT_DATA_H

#include <stdbool.h>
#include "private.h"

struct Menu;

/**
 * struct AutocryptData - Data to pass to the Autocrypt Functions
 */
struct AutocryptData
{
  bool done;                         ///< Should we close the Dialog?
  struct Menu *menu;                 ///< Autocrypt Menu
  struct AccountEntryArray entries;  ///< Account Entries
};

struct AutocryptData *autocrypt_data_new(void);
void                  autocrypt_data_free(struct Menu *menu, void **ptr);

void account_entry_array_clear(struct AccountEntryArray *entries);

#endif /* MUTT_AUTOCRYPT_AUTOCRYPT_DATA_H */
