/**
 * @file
 * PGP key management routines
 *
 * @authors
 * Copyright (C) 2017-2020 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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

#ifndef MUTT_NCRYPT_PGPKEY_H
#define MUTT_NCRYPT_PGPKEY_H

#include <stdbool.h>
#include "lib.h"

struct Address;
struct PgpKeyInfo;
struct PgpUid;

/**
 * enum PgpRing - PGP ring type
 */
enum PgpRing
{
  PGP_PUBRING, ///< Public keys
  PGP_SECRING, ///< Secret keys
};

struct Body *pgp_class_make_key_attachment(void);

struct PgpKeyInfo *pgp_ask_for_key   (char *tag, const char *whatfor, KeyFlags abilities, enum PgpRing keyring);
struct PgpKeyInfo *pgp_getkeybyaddr  (struct Address *a, KeyFlags abilities, enum PgpRing keyring, bool oppenc_mode);
struct PgpKeyInfo *pgp_getkeybystr   (const char *p, KeyFlags abilities, enum PgpRing keyring);
bool               pgp_id_is_strong  (struct PgpUid *uid);
bool               pgp_id_is_valid   (struct PgpUid *uid);
bool               pgp_keys_are_valid(struct PgpKeyInfo *keys);
bool               pgp_key_is_valid  (struct PgpKeyInfo *k);
struct PgpKeyInfo *pgp_principal_key (struct PgpKeyInfo *key);

#endif /* MUTT_NCRYPT_PGPKEY_H */
