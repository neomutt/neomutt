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

#ifndef MUTT_NCRYPT_PGPINVOKE_H
#define MUTT_NCRYPT_PGPINVOKE_H

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include "ncrypt/lib.h"
#include "pgpkey.h"

struct Address;
struct ListHead;

/* The PGP invocation interface */

void pgp_class_invoke_import(const char *fname);
void pgp_class_invoke_getkeys(struct Address *addr);

pid_t pgp_invoke_decode     (FILE **fp_pgp_in, FILE **fp_pgp_out, FILE **fp_pgp_err, int fd_pgp_in, int fd_pgp_out, int fd_pgp_err, const char *fname, bool need_passphrase);
pid_t pgp_invoke_decrypt    (FILE **fp_pgp_in, FILE **fp_pgp_out, FILE **fp_pgp_err, int fd_pgp_in, int fd_pgp_out, int fd_pgp_err, const char *fname);
pid_t pgp_invoke_encrypt    (FILE **fp_pgp_in, FILE **fp_pgp_out, FILE **fp_pgp_err, int fd_pgp_in, int fd_pgp_out, int fd_pgp_err, const char *fname, const char *uids, bool sign);
pid_t pgp_invoke_export     (FILE **fp_pgp_in, FILE **fp_pgp_out, FILE **fp_pgp_err, int fd_pgp_in, int fd_pgp_out, int fd_pgp_err, const char *uids);
pid_t pgp_invoke_list_keys  (FILE **fp_pgp_in, FILE **fp_pgp_out, FILE **fp_pgp_err, int fd_pgp_in, int fd_pgp_out, int fd_pgp_err, enum PgpRing keyring, struct ListHead *hints);
pid_t pgp_invoke_sign       (FILE **fp_pgp_in, FILE **fp_pgp_out, FILE **fp_pgp_err, int fd_pgp_in, int fd_pgp_out, int fd_pgp_err, const char *fname);
pid_t pgp_invoke_traditional(FILE **fp_pgp_in, FILE **fp_pgp_out, FILE **fp_pgp_err, int fd_pgp_in, int fd_pgp_out, int fd_pgp_err, const char *fname, const char *uids, SecurityFlags flags);
pid_t pgp_invoke_verify     (FILE **fp_pgp_in, FILE **fp_pgp_out, FILE **fp_pgp_err, int fd_pgp_in, int fd_pgp_out, int fd_pgp_err, const char *fname, const char *sig_fname);
pid_t pgp_invoke_verify_key (FILE **fp_pgp_in, FILE **fp_pgp_out, FILE **fp_pgp_err, int fd_pgp_in, int fd_pgp_out, int fd_pgp_err, const char *uids);

#endif /* MUTT_NCRYPT_PGPINVOKE_H */
