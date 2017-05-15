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

#ifndef _NCRYPT_CRYPTGLUE_H
#define _NCRYPT_CRYPTGLUE_H

struct Address;
struct Body;
struct State;

/* Silently forget about a passphrase. */
void crypt_pgp_void_passphrase(void);

int crypt_pgp_valid_passphrase(void);

/* fixme: needs documentation. */
struct Body *crypt_pgp_traditional_encryptsign(struct Body *a, int flags, char *keylist);

/* This routine attempts to find the keyids of the recipients of a message.  It
 * returns NULL if any of the keys can not be found.  If oppenc_mode is true,
 * only keys that can be determined without prompting will be used.  */
char *crypt_pgp_findkeys(struct Address *adrlist, int oppenc_mode);

/* Create a new body with a PGP signed message from A. */
struct Body *crypt_pgp_sign_message(struct Body *a);

/* Warning: A is no longer freed in this routine, you need to free it later.
 * This is necessary for $fcc_attach. */
struct Body *crypt_pgp_encrypt_message(struct Body *a, char *keylist, int sign);

/* Invoke the PGP command to import a key. */
void crypt_pgp_invoke_import(const char *fname);

/* fixme: needs documentation */
int crypt_pgp_verify_one(struct Body *sigbdy, struct State *s, const char *tempf);

void crypt_pgp_set_sender(const char *sender);

/* Silently forget about a passphrase. */
void crypt_smime_void_passphrase(void);

int crypt_smime_valid_passphrase(void);

/* Ask for an SMIME key. */

/* This routine attempts to find the keyids of the recipients of a message.  It
 * returns NULL if any of the keys can not be found.  If oppenc_mode is true,
 * only keys that can be determined without prompting will be used.  */
char *crypt_smime_findkeys(struct Address *adrlist, int oppenc_mode);

/* fixme: Needs documentation. */
struct Body *crypt_smime_sign_message(struct Body *a);

/* fixme: needs documentation. */
struct Body *crypt_smime_build_smime_entity(struct Body *a, char *certlist);

/* Add a certificate and update index file (externally). */
void crypt_smime_invoke_import(char *infile, char *mailbox);

void crypt_smime_set_sender(const char *sender);

/* fixme: needs documentation */
int crypt_smime_verify_one(struct Body *sigbdy, struct State *s, const char *tempf);

#endif /* _NCRYPT_CRYPTGLUE_H */
