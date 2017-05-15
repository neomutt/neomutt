/**
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
 *
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

typedef enum pgp_ring {
  PGP_PUBRING,
  PGP_SECRING,
} pgp_ring_t;

struct Body *pgp_make_key_attachment(char *tempf);

struct PgpKeyInfo * pgp_ask_for_key(char *tag, char *whatfor, short abilities, pgp_ring_t keyring);
struct PgpKeyInfo * pgp_getkeybyaddr(struct Address *a, short abilities, pgp_ring_t keyring, int oppenc_mode);
struct PgpKeyInfo * pgp_getkeybystr(char *p, short abilities, pgp_ring_t keyring);

#endif /* _NCRYPT_PGPKEY_H */
