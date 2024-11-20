/**
 * @file
 * PGP Key Sorting
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
 * @page crypt_sort_pgp PGP Key Sorting
 *
 * PGP Key Sorting
 */

#include "config.h"
#include <locale.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "lib.h"
#include "pgp.h"
#include "pgplib.h"
#include "sort.h"

/**
 * pgp_sort_address - Compare two keys by their addresses - Implements ::sort_t - @ingroup sort_api
 */
static int pgp_sort_address(const void *a, const void *b, void *sdata)
{
  struct PgpUid const *s = *(struct PgpUid const *const *) a;
  struct PgpUid const *t = *(struct PgpUid const *const *) b;
  const bool sort_reverse = *(bool *) sdata;

  int rc = mutt_istr_cmp(s->addr, t->addr);
  if (rc != 0)
    goto done;

  rc = mutt_istr_cmp(pgp_fpr_or_lkeyid(s->parent), pgp_fpr_or_lkeyid(t->parent));

done:
  return sort_reverse ? -rc : rc;
}

/**
 * pgp_sort_date - Compare two keys by their dates - Implements ::sort_t - @ingroup sort_api
 */
static int pgp_sort_date(const void *a, const void *b, void *sdata)
{
  struct PgpUid const *s = *(struct PgpUid const *const *) a;
  struct PgpUid const *t = *(struct PgpUid const *const *) b;
  const bool sort_reverse = *(bool *) sdata;

  int rc = mutt_numeric_cmp(s->parent->gen_time, t->parent->gen_time);
  if (rc != 0)
    goto done;

  rc = mutt_istr_cmp(s->addr, t->addr);

done:
  return sort_reverse ? -rc : rc;
}

/**
 * pgp_sort_keyid - Compare two keys by their IDs - Implements ::sort_t - @ingroup sort_api
 */
static int pgp_sort_keyid(const void *a, const void *b, void *sdata)
{
  struct PgpUid const *s = *(struct PgpUid const *const *) a;
  struct PgpUid const *t = *(struct PgpUid const *const *) b;
  const bool sort_reverse = *(bool *) sdata;

  int rc = mutt_istr_cmp(pgp_fpr_or_lkeyid(s->parent), pgp_fpr_or_lkeyid(t->parent));
  if (rc != 0)
    goto done;

  rc = mutt_istr_cmp(s->addr, t->addr);

done:
  return sort_reverse ? -rc : rc;
}

/**
 * pgp_sort_trust - Compare two keys by their trust levels - Implements ::sort_t - @ingroup sort_api
 */
static int pgp_sort_trust(const void *a, const void *b, void *sdata)
{
  struct PgpUid const *s = *(struct PgpUid const *const *) a;
  struct PgpUid const *t = *(struct PgpUid const *const *) b;
  const bool sort_reverse = *(bool *) sdata;

  int rc = mutt_numeric_cmp(s->parent->flags & KEYFLAG_RESTRICTIONS,
                            t->parent->flags & KEYFLAG_RESTRICTIONS);
  if (rc != 0)
    goto done;

  // Note: reversed
  rc = mutt_numeric_cmp(t->trust, s->trust);
  if (rc != 0)
    goto done;

  // Note: reversed
  rc = mutt_numeric_cmp(t->parent->keylen, s->parent->keylen);
  if (rc != 0)
    goto done;

  // Note: reversed
  rc = mutt_numeric_cmp(t->parent->gen_time, s->parent->gen_time);
  if (rc != 0)
    goto done;

  rc = mutt_istr_cmp(s->addr, t->addr);
  if (rc != 0)
    goto done;

  rc = mutt_istr_cmp(pgp_fpr_or_lkeyid(s->parent), pgp_fpr_or_lkeyid(t->parent));

done:
  return sort_reverse ? -rc : rc;
}

/**
 * pgp_sort_keys - Sort an array of PGP keys
 * @param pua Array to sort
 *
 * Sort PGP keys according to `$pgp_sort_keys`
 */
void pgp_sort_keys(struct PgpUidArray *pua)
{
  if (!pua)
    return;

  sort_t fn = NULL;
  short c_pgp_sort_keys = cs_subset_sort(NeoMutt->sub, "pgp_key_sort");
  switch (c_pgp_sort_keys & SORT_MASK)
  {
    case KEY_SORT_ADDRESS:
      fn = pgp_sort_address;
      break;
    case KEY_SORT_DATE:
      fn = pgp_sort_date;
      break;
    case KEY_SORT_KEYID:
      fn = pgp_sort_keyid;
      break;
    case KEY_SORT_TRUST:
    default:
      fn = pgp_sort_trust;
      break;
  }

  if (ARRAY_SIZE(pua) > 1)
  {
    bool sort_reverse = c_pgp_sort_keys & SORT_REVERSE;
    ARRAY_SORT(pua, fn, &sort_reverse);
  }
}
