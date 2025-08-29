/**
 * @file
 *
 * GPGME Key Sorting
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
 * @page crypt_sort_gpgme GPGME Key Sorting
 *
 * GPGME Key Sorting
 */

#include "config.h"
#include <locale.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "lib.h"
#include "crypt_gpgme.h"
#include "sort.h"

/**
 * crypt_sort_address - Compare two keys by their addresses - Implements ::sort_t - @ingroup sort_api
 */
static int crypt_sort_address(const void *a, const void *b, void *sdata)
{
  struct CryptKeyInfo *s = *(struct CryptKeyInfo **) a;
  struct CryptKeyInfo *t = *(struct CryptKeyInfo **) b;
  const bool sort_reverse = *(bool *) sdata;

  int rc = mutt_istr_cmp(s->uid, t->uid);
  if (rc != 0)
    goto done;

  rc = mutt_istr_cmp(crypt_fpr_or_lkeyid(s), crypt_fpr_or_lkeyid(t));

done:
  return sort_reverse ? -rc : rc;
}

/**
 * crypt_sort_keyid - Compare two keys by their IDs - Implements ::sort_t - @ingroup sort_api
 */
static int crypt_sort_keyid(const void *a, const void *b, void *sdata)
{
  struct CryptKeyInfo *s = *(struct CryptKeyInfo **) a;
  struct CryptKeyInfo *t = *(struct CryptKeyInfo **) b;
  const bool sort_reverse = *(bool *) sdata;

  int rc = mutt_istr_cmp(crypt_fpr_or_lkeyid(s), crypt_fpr_or_lkeyid(t));
  if (rc != 0)
    goto done;

  rc = mutt_istr_cmp(s->uid, t->uid);

done:
  return sort_reverse ? -rc : rc;
}

/**
 * crypt_sort_date - Compare two keys by their dates - Implements ::sort_t - @ingroup sort_api
 */
static int crypt_sort_date(const void *a, const void *b, void *sdata)
{
  struct CryptKeyInfo *s = *(struct CryptKeyInfo **) a;
  struct CryptKeyInfo *t = *(struct CryptKeyInfo **) b;
  const bool sort_reverse = *(bool *) sdata;

  unsigned long ts = 0;
  unsigned long tt = 0;
  int rc = 0;

  if (s->kobj->subkeys && (s->kobj->subkeys->timestamp > 0))
    ts = s->kobj->subkeys->timestamp;
  if (t->kobj->subkeys && (t->kobj->subkeys->timestamp > 0))
    tt = t->kobj->subkeys->timestamp;

  if (ts > tt)
  {
    rc = 1;
    goto done;
  }

  if (ts < tt)
  {
    rc = -1;
    goto done;
  }

  rc = mutt_istr_cmp(s->uid, t->uid);

done:
  return sort_reverse ? -rc : rc;
}

/**
 * crypt_sort_trust - Compare two keys by their trust levels - Implements ::sort_t - @ingroup sort_api
 */
static int crypt_sort_trust(const void *a, const void *b, void *sdata)
{
  struct CryptKeyInfo *s = *(struct CryptKeyInfo **) a;
  struct CryptKeyInfo *t = *(struct CryptKeyInfo **) b;
  const bool sort_reverse = *(bool *) sdata;

  unsigned long ts = 0;
  unsigned long tt = 0;

  int rc = mutt_numeric_cmp(s->flags & KEYFLAG_RESTRICTIONS, t->flags & KEYFLAG_RESTRICTIONS);
  if (rc != 0)
    goto done;

  // Note: reversed
  rc = mutt_numeric_cmp(t->validity, s->validity);
  if (rc != 0)
    return rc;

  ts = 0;
  tt = 0;
  if (s->kobj->subkeys)
    ts = s->kobj->subkeys->length;
  if (t->kobj->subkeys)
    tt = t->kobj->subkeys->length;

  // Note: reversed
  rc = mutt_numeric_cmp(tt, ts);
  if (rc != 0)
    goto done;

  ts = 0;
  tt = 0;
  if (s->kobj->subkeys && (s->kobj->subkeys->timestamp > 0))
    ts = s->kobj->subkeys->timestamp;
  if (t->kobj->subkeys && (t->kobj->subkeys->timestamp > 0))
    tt = t->kobj->subkeys->timestamp;

  // Note: reversed
  rc = mutt_numeric_cmp(tt, ts);
  if (rc != 0)
    goto done;

  rc = mutt_istr_cmp(s->uid, t->uid);
  if (rc != 0)
    goto done;

  rc = mutt_istr_cmp(crypt_fpr_or_lkeyid(s), crypt_fpr_or_lkeyid(t));

done:
  return sort_reverse ? -rc : rc;
}

/**
 * gpgme_sort_keys - Sort an array of GPGME keys
 * @param ckia Array to sort
 *
 * Sort GPGME keys according to `$pgp_sort_keys`
 */
void gpgme_sort_keys(struct CryptKeyInfoArray *ckia)
{
  const short c_pgp_sort_keys = cs_subset_sort(NeoMutt->sub, "pgp_key_sort");
  sort_t fn = NULL;
  switch (c_pgp_sort_keys & SORT_MASK)
  {
    case KEY_SORT_ADDRESS:
      fn = crypt_sort_address;
      break;
    case KEY_SORT_DATE:
      fn = crypt_sort_date;
      break;
    case KEY_SORT_KEYID:
      fn = crypt_sort_keyid;
      break;
    case KEY_SORT_TRUST:
    default:
      fn = crypt_sort_trust;
      break;
  }

  if (ARRAY_SIZE(ckia) > 1)
  {
    bool sort_reverse = c_pgp_sort_keys & SORT_REVERSE;
    ARRAY_SORT(ckia, fn, &sort_reverse);
  }
}
