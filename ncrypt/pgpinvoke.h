/**
 * @file
 * Wrapper around calls to external PGP program
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

#ifndef _NCRYPT_PGPINVOKE_H
#define _NCRYPT_PGPINVOKE_H

#include <stdio.h>
#include <unistd.h>
#include "pgpkey.h"

struct Address;
struct List;

/* The PGP invocation interface */

void pgp_invoke_import(const char *fname);
void pgp_invoke_getkeys(struct Address *addr);

pid_t pgp_invoke_decode(FILE **pgpin, FILE **pgpout, FILE **pgperr, int pgpinfd, int pgpoutfd,
                        int pgperrfd, const char *fname, short need_passphrase);
pid_t pgp_invoke_verify(FILE **pgpin, FILE **pgpout, FILE **pgperr, int pgpinfd, int pgpoutfd,
                        int pgperrfd, const char *fname, const char *sig_fname);
pid_t pgp_invoke_decrypt(FILE **pgpin, FILE **pgpout, FILE **pgperr, int pgpinfd,
                         int pgpoutfd, int pgperrfd, const char *fname);
pid_t pgp_invoke_sign(FILE **pgpin, FILE **pgpout, FILE **pgperr, int pgpinfd,
                      int pgpoutfd, int pgperrfd, const char *fname);
pid_t pgp_invoke_encrypt(FILE **pgpin, FILE **pgpout, FILE **pgperr,
                         int pgpinfd, int pgpoutfd, int pgperrfd,
                         const char *fname, const char *uids, int sign);
pid_t pgp_invoke_export(FILE **pgpin, FILE **pgpout, FILE **pgperr, int pgpinfd,
                        int pgpoutfd, int pgperrfd, const char *uids);
pid_t pgp_invoke_verify_key(FILE **pgpin, FILE **pgpout, FILE **pgperr, int pgpinfd,
                            int pgpoutfd, int pgperrfd, const char *uids);
pid_t pgp_invoke_list_keys(FILE **pgpin, FILE **pgpout, FILE **pgperr, int pgpinfd,
                           int pgpoutfd, int pgperrfd, enum PgpRing keyring, struct List *hints);
pid_t pgp_invoke_traditional(FILE **pgpin, FILE **pgpout, FILE **pgperr,
                             int pgpinfd, int pgpoutfd, int pgperrfd,
                             const char *fname, const char *uids, int flags);

#endif /* _NCRYPT_PGPINVOKE_H */
