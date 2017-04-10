/**
 * Copyright (C) 1996-1997 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2003 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2004 g10 Code GmbH
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

#ifndef _MUTT_PGP_H
#define _MUTT_PGP_H 1

#ifdef CRYPT_BACKEND_CLASSIC_PGP

#include "mutt_crypt.h"
#include "pgplib.h"


/* prototypes */

bool pgp_use_gpg_agent(void);

int pgp_check_traditional(FILE *fp, BODY *b, int tagged_only);
BODY *pgp_make_key_attachment(char *tempf);
const char *pgp_micalg(const char *fname);

char *_pgp_keyid (pgp_key_t);
char *pgp_keyid (pgp_key_t);
char *pgp_short_keyid(pgp_key_t k);
char *pgp_long_keyid(pgp_key_t k);
char *pgp_fpr_or_lkeyid(pgp_key_t k);

int pgp_decrypt_mime(FILE *fpin, FILE **fpout, BODY *b, BODY **cur);

pgp_key_t pgp_ask_for_key(char *tag, char *whatfor, short abilities, pgp_ring_t keyring);
pgp_key_t pgp_get_candidates(pgp_ring_t keyring, LIST *hints);
pgp_key_t pgp_getkeybyaddr(ADDRESS *a, short abilities, pgp_ring_t keyring, int oppenc_mode);
pgp_key_t pgp_getkeybystr(char *p, short abilities, pgp_ring_t keyring);

char *pgp_find_keys(ADDRESS *adrlist, int oppenc_mode);

int pgp_application_pgp_handler(BODY *m, STATE *s);
int pgp_encrypted_handler(BODY *a, STATE *s);
void pgp_extract_keys_from_attachment_list(FILE *fp, int tag, BODY *top);
void pgp_void_passphrase(void);
int pgp_valid_passphrase(void);


/* The PGP invocation interface - not really beautiful. */

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
                           int pgpoutfd, int pgperrfd, pgp_ring_t keyring, LIST *hints);
pid_t pgp_invoke_traditional(FILE **pgpin, FILE **pgpout, FILE **pgperr,
                             int pgpinfd, int pgpoutfd, int pgperrfd,
                             const char *fname, const char *uids, int flags);


void pgp_invoke_import(const char *fname);
void pgp_invoke_getkeys(ADDRESS *addr);


/* private ? */
int pgp_verify_one(BODY *sigbdy, STATE *s, const char *tempfile);
BODY *pgp_traditional_encryptsign(BODY *a, int flags, char *keylist);
BODY *pgp_encrypt_message(BODY *a, char *keylist, int sign);
BODY *pgp_sign_message(BODY *a);

int pgp_send_menu(HEADER *msg);

#endif /* CRYPT_BACKEND_CLASSIC_PGP */

#endif /* _MUTT_PGP_H */
