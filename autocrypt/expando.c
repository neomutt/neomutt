/**
 * @file
 * Autocrypt Expando definitions
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
 * @page autocrypt_expando Autocrypt Expando definitions
 *
 * Autocrypt Expando definitions
 */

#include <stdbool.h>
#include <stdio.h>
#include "private.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "expando.h"
#include "lib.h"
#include "expando/lib.h"

/**
 * autocrypt_address - Autocrypt: Address - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void autocrypt_address(const struct ExpandoNode *node, void *data,
                              MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AccountEntry *entry = data;

  buf_copy(buf, entry->addr->mailbox);
}

/**
 * autocrypt_enabled - Autocrypt: Status flag - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void autocrypt_enabled(const struct ExpandoNode *node, void *data,
                              MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AccountEntry *entry = data;

  if (entry->account->enabled)
  {
    /* L10N: Autocrypt Account menu.
           flag that an account is enabled/active */
    buf_addstr(buf, _("active"));
  }
  else
  {
    /* L10N: Autocrypt Account menu.
           flag that an account is disabled/inactive */
    buf_addstr(buf, _("inactive"));
  }
}

/**
 * autocrypt_keyid - Autocrypt: GPG Key - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void autocrypt_keyid(const struct ExpandoNode *node, void *data,
                            MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AccountEntry *entry = data;

  const char *s = entry->account->keyid;
  buf_strcpy(buf, s);
}

/**
 * autocrypt_number_num - Autocrypt: Index number - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long autocrypt_number_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct AccountEntry *entry = data;

  return entry->num;
}

/**
 * autocrypt_prefer_encrypt - Autocrypt: Prefer-encrypt flag - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void autocrypt_prefer_encrypt(const struct ExpandoNode *node, void *data,
                                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AccountEntry *entry = data;

  if (entry->account->prefer_encrypt)
  {
    /* L10N: Autocrypt Account menu.
           flag that an account has prefer-encrypt set */
    buf_addstr(buf, _("prefer encrypt"));
  }
  else
  {
    /* L10N: Autocrypt Account menu.
           flag that an account has prefer-encrypt unset;
           thus encryption will need to be manually enabled.  */
    buf_addstr(buf, _("manual encrypt"));
  }
}

/**
 * AutocryptRenderCallbacks - Callbacks for Autocrypt Expandos
 *
 * @sa AutocryptFormatDef, ExpandoDataAutocrypt, ExpandoDataGlobal
 */
const struct ExpandoRenderCallback AutocryptRenderCallbacks[] = {
  // clang-format off
  { ED_AUTOCRYPT, ED_AUT_ADDRESS,        autocrypt_address,        NULL },
  { ED_AUTOCRYPT, ED_AUT_ENABLED,        autocrypt_enabled,        NULL },
  { ED_AUTOCRYPT, ED_AUT_KEYID,          autocrypt_keyid,          NULL },
  { ED_AUTOCRYPT, ED_AUT_NUMBER,         NULL,                     autocrypt_number_num },
  { ED_AUTOCRYPT, ED_AUT_PREFER_ENCRYPT, autocrypt_prefer_encrypt, NULL },
  { -1, -1, NULL, NULL },
  // clang-format on
};
