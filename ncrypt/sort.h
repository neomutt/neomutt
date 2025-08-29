/**
 * @file
 * Crypto Key sorting functions
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

#ifndef MUTT_NCRYPT_SORT_H
#define MUTT_NCRYPT_SORT_H

struct CryptKeyInfoArray;
struct PgpUidArray;

/**
 * enum KeySortType - Methods for sorting Crypto Keys
 */
enum KeySortType
{
  KEY_SORT_ADDRESS,    ///< Sort by address
  KEY_SORT_DATE,       ///< Sort by date
  KEY_SORT_KEYID,      ///< Sort by key id
  KEY_SORT_TRUST,      ///< Sort by trust level
};

void gpgme_sort_keys(struct CryptKeyInfoArray *ckia);
void pgp_sort_keys  (struct PgpUidArray *pua);

#endif /* MUTT_NCRYPT_SORT_H */
