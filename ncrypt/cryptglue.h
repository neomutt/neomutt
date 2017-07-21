/**
 * @file
 * Wrapper around crypto functions
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

#ifndef _NCRYPT_CRYPTGLUE_H
#define _NCRYPT_CRYPTGLUE_H

struct Address;
struct Body;
struct State;

void crypt_pgp_void_passphrase(void);
int crypt_pgp_valid_passphrase(void);
struct Body *crypt_pgp_traditional_encryptsign(struct Body *a, int flags, char *keylist);
char *crypt_pgp_findkeys(struct Address *adrlist, int oppenc_mode);
struct Body *crypt_pgp_sign_message(struct Body *a);
struct Body *crypt_pgp_encrypt_message(struct Body *a, char *keylist, int sign);
void crypt_pgp_invoke_import(const char *fname);
int crypt_pgp_verify_one(struct Body *sigbdy, struct State *s, const char *tempf);
void crypt_pgp_set_sender(const char *sender);

void crypt_smime_void_passphrase(void);
int crypt_smime_valid_passphrase(void);
char *crypt_smime_findkeys(struct Address *adrlist, int oppenc_mode);
struct Body *crypt_smime_sign_message(struct Body *a);
struct Body *crypt_smime_build_smime_entity(struct Body *a, char *certlist);
void crypt_smime_invoke_import(char *infile, char *mailbox);
void crypt_smime_set_sender(const char *sender);
int crypt_smime_verify_one(struct Body *sigbdy, struct State *s, const char *tempf);

#endif /* _NCRYPT_CRYPTGLUE_H */
