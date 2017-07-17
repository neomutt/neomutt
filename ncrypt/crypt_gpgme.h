/**
 * @file
 * Wrapper for PGP/SMIME calls to GPGME
 *
 * @authors
 * Copyright (C) 2004 g10 Code GmbH
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

#ifndef _NCRYPT_CRYPT_GPGME_H
#define _NCRYPT_CRYPT_GPGME_H

#include <stdio.h>

struct Address;
struct Body;
struct Header;
struct State;

void pgp_gpgme_init(void);
void smime_gpgme_init(void);

char *pgp_gpgme_findkeys(struct Address *adrlist, int oppenc_mode);
char *smime_gpgme_findkeys(struct Address *adrlist, int oppenc_mode);

struct Body *pgp_gpgme_encrypt_message(struct Body *a, char *keylist, int sign);
struct Body *smime_gpgme_build_smime_entity(struct Body *a, char *keylist);

int pgp_gpgme_decrypt_mime(FILE *fpin, FILE **fpout, struct Body *b, struct Body **cur);
int smime_gpgme_decrypt_mime(FILE *fpin, FILE **fpout, struct Body *b, struct Body **cur);

int pgp_gpgme_check_traditional(FILE *fp, struct Body *b, int tagged_only);
void pgp_gpgme_invoke_import(const char *fname);

int pgp_gpgme_application_handler(struct Body *m, struct State *s);
int smime_gpgme_application_handler(struct Body *a, struct State *s);
int pgp_gpgme_encrypted_handler(struct Body *a, struct State *s);

struct Body *pgp_gpgme_make_key_attachment(char *tempf);

struct Body *pgp_gpgme_sign_message(struct Body *a);
struct Body *smime_gpgme_sign_message(struct Body *a);

int pgp_gpgme_verify_one(struct Body *sigbdy, struct State *s, const char *tempfile);
int smime_gpgme_verify_one(struct Body *sigbdy, struct State *s, const char *tempfile);

int pgp_gpgme_send_menu(struct Header *msg);
int smime_gpgme_send_menu(struct Header *msg);

int smime_gpgme_verify_sender(struct Header *h);

void mutt_gpgme_set_sender(const char *sender);

#endif /* _NCRYPT_CRYPT_GPGME_H */
