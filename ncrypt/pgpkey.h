/**
 * @file
 * PGP key management routines
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef _NCRYPT_PGPKEY_H
#define _NCRYPT_PGPKEY_H

struct Address;
struct Body;
struct PgpKeyInfo;

/**
 * enum PgpRing - PGP ring type
 */
enum PgpRing
{
  PGP_PUBRING, /**< Public keys */
  PGP_SECRING, /**< Secret keys */
};

struct Body *pgp_make_key_attachment(char *tempf);

struct PgpKeyInfo * pgp_ask_for_key(char *tag, char *whatfor, short abilities, enum PgpRing keyring);
struct PgpKeyInfo * pgp_getkeybyaddr(struct Address *a, short abilities, enum PgpRing keyring, int oppenc_mode);
struct PgpKeyInfo * pgp_getkeybystr(char *p, short abilities, enum PgpRing keyring);

#endif /* _NCRYPT_PGPKEY_H */
