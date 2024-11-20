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

/**
 * @page autocrypt_autocrypt_data Private Autocrypt Data
 *
 * Private Autocrypt Data
 */

#include "config.h"
#include <stddef.h>
#include "private.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "autocrypt_data.h"

/**
 * autocrypt_data_new - Create new Autocrypt Data
 * @retval ptr New Autocrypt Data
 */
struct AutocryptData *autocrypt_data_new(void)
{
  struct AutocryptData *ad = MUTT_MEM_CALLOC(1, struct AutocryptData);

  ARRAY_INIT(&ad->entries);

  return ad;
}

/**
 * account_entry_array_clear - Clear an AccountEntry array
 * @param entries Array to clear
 *
 * @note The array itself will not be freed
 */
void account_entry_array_clear(struct AccountEntryArray *entries)
{
  struct AccountEntry **pe = NULL;
  ARRAY_FOREACH(pe, entries)
  {
    struct AccountEntry *e = *pe;
    mutt_autocrypt_db_account_free(&e->account);
    mutt_addr_free(&e->addr);
    FREE(pe);
  }

  ARRAY_FREE(entries);
}

/**
 * autocrypt_data_free - Free Autocrypt Data - Implements Menu::mdata_free() - @ingroup menu_mdata_free
 */
void autocrypt_data_free(struct Menu *menu, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct AutocryptData *ad = *ptr;
  account_entry_array_clear(&ad->entries);

  FREE(ptr);
}
