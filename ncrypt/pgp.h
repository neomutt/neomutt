/**
 * @file
 * PGP sign, encrypt, check routines
 *
 * @authors
 * Copyright (C) 1996-1997 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2003 Thomas Roessler <roessler@does-not-exist.org>
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

#ifndef MUTT_NCRYPT_PGP_H
#define MUTT_NCRYPT_PGP_H

#include <stdbool.h>
#include <stdio.h>
#include "ncrypt.h"

struct AddressList;
struct Body;
struct Email;
struct PgpKeyInfo;
struct State;

bool pgp_use_gpg_agent(void);

int pgp_class_check_traditional(FILE *fp, struct Body *b, bool just_one);

char *pgp_this_keyid(struct PgpKeyInfo *k);
char *pgp_keyid(struct PgpKeyInfo *k);
char *pgp_short_keyid(struct PgpKeyInfo * k);
char *pgp_long_keyid(struct PgpKeyInfo * k);
char *pgp_fpr_or_lkeyid(struct PgpKeyInfo * k);

int pgp_class_decrypt_mime(FILE *fp_in, FILE **fp_out, struct Body *b, struct Body **cur);

char *pgp_class_find_keys(struct AddressList *addrlist, bool oppenc_mode);

int pgp_class_application_handler(struct Body *m, struct State *s);
int pgp_class_encrypted_handler(struct Body *a, struct State *s);
void pgp_class_extract_key_from_attachment(FILE *fp, struct Body *top);
void pgp_class_void_passphrase(void);
bool pgp_class_valid_passphrase(void);

int pgp_class_verify_one(struct Body *sigbdy, struct State *s, const char *tempfile);
struct Body *pgp_class_traditional_encryptsign(struct Body *a, SecurityFlags flags, char *keylist);
struct Body *pgp_class_encrypt_message(struct Body *a, char *keylist, bool sign);
struct Body *pgp_class_sign_message(struct Body *a);

int pgp_class_send_menu(struct Email *msg);

#endif /* MUTT_NCRYPT_PGP_H */
