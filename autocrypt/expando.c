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
 * autocrypt_a - Autocrypt: Address - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void autocrypt_a(const struct ExpandoNode *node, void *data,
                        MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AccountEntry *entry = data;

  buf_copy(buf, entry->addr->mailbox);
}

/**
 * autocrypt_k - Autocrypt: GPG Key - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void autocrypt_k(const struct ExpandoNode *node, void *data,
                        MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AccountEntry *entry = data;

  const char *s = entry->account->keyid;
  buf_strcpy(buf, s);
}

/**
 * autocrypt_n_num - Autocrypt: Index number - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long autocrypt_n_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct AccountEntry *entry = data;

  return entry->num;
}

/**
 * autocrypt_p - Autocrypt: Prefer-encrypt flag - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void autocrypt_p(const struct ExpandoNode *node, void *data,
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
 * autocrypt_s - Autocrypt: Status flag - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void autocrypt_s(const struct ExpandoNode *node, void *data,
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
 * AutocryptRenderData - Callbacks for Autocrypt Expandos
 *
 * @sa AutocryptFormatDef, ExpandoDataAutocrypt, ExpandoDataGlobal
 */
const struct ExpandoRenderData AutocryptRenderData[] = {
  // clang-format off
  { ED_AUTOCRYPT, ED_AUT_ADDRESS,        autocrypt_a,     NULL },
  { ED_AUTOCRYPT, ED_AUT_KEYID,          autocrypt_k,     NULL },
  { ED_AUTOCRYPT, ED_AUT_NUMBER,         NULL,            autocrypt_n_num },
  { ED_AUTOCRYPT, ED_AUT_PREFER_ENCRYPT, autocrypt_p,     NULL },
  { ED_AUTOCRYPT, ED_AUT_ENABLED,        autocrypt_s,     NULL },
  { -1, -1, NULL, NULL },
  // clang-format on
};
