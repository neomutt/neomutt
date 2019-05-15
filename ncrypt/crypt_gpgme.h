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

#ifndef MUTT_NCRYPT_CRYPT_GPGME_H
#define MUTT_NCRYPT_CRYPT_GPGME_H

#include <stdbool.h>
#include <stdio.h>

struct AddressList;
struct Body;
struct Email;
struct State;

void         pgp_gpgme_set_sender(const char *sender);

int          pgp_gpgme_application_handler(struct Body *m, struct State *s);
int          pgp_gpgme_check_traditional(FILE *fp, struct Body *b, bool just_one);
int          pgp_gpgme_decrypt_mime(FILE *fp_in, FILE **fp_out, struct Body *b, struct Body **cur);
int          pgp_gpgme_encrypted_handler(struct Body *a, struct State *s);
struct Body *pgp_gpgme_encrypt_message(struct Body *a, char *keylist, bool sign);
char *       pgp_gpgme_find_keys(struct AddressList *addrlist, bool oppenc_mode);
void         pgp_gpgme_init(void);
void         pgp_gpgme_invoke_import(const char *fname);
struct Body *pgp_gpgme_make_key_attachment(void);
int          pgp_gpgme_send_menu(struct Email *msg);
struct Body *pgp_gpgme_sign_message(struct Body *a);
int          pgp_gpgme_verify_one(struct Body *sigbdy, struct State *s, const char *tempfile);

int          smime_gpgme_application_handler(struct Body *a, struct State *s);
struct Body *smime_gpgme_build_smime_entity(struct Body *a, char *keylist);
int          smime_gpgme_decrypt_mime(FILE *fp_in, FILE **fp_out, struct Body *b, struct Body **cur);
char *       smime_gpgme_find_keys(struct AddressList *addrlist, bool oppenc_mode);
void         smime_gpgme_init(void);
int          smime_gpgme_send_menu(struct Email *msg);
struct Body *smime_gpgme_sign_message(struct Body *a);
int          smime_gpgme_verify_one(struct Body *sigbdy, struct State *s, const char *tempfile);
int          smime_gpgme_verify_sender(struct Email *e);

const char  *mutt_gpgme_print_version(void);

#endif /* MUTT_NCRYPT_CRYPT_GPGME_H */
