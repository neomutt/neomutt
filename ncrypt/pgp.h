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

#ifndef _NCRYPT_PGP_H
#define _NCRYPT_PGP_H

#ifdef CRYPT_BACKEND_CLASSIC_PGP

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include "ncrypt.h"

struct Address;
struct Body;
struct Header;
struct List;
struct PgpKeyInfo;
struct State;

bool pgp_use_gpg_agent(void);

int pgp_check_traditional(FILE *fp, struct Body *b, int tagged_only);

char *_pgp_keyid(struct PgpKeyInfo *);
char *pgp_keyid(struct PgpKeyInfo *);
char *pgp_short_keyid(struct PgpKeyInfo * k);
char *pgp_long_keyid(struct PgpKeyInfo * k);
char *pgp_fpr_or_lkeyid(struct PgpKeyInfo * k);

int pgp_decrypt_mime(FILE *fpin, FILE **fpout, struct Body *b, struct Body **cur);

char *pgp_find_keys(struct Address *adrlist, int oppenc_mode);

int pgp_application_pgp_handler(struct Body *m, struct State *s);
int pgp_encrypted_handler(struct Body *a, struct State *s);
void pgp_extract_keys_from_attachment_list(FILE *fp, int tag, struct Body *top);
void pgp_void_passphrase(void);
int pgp_valid_passphrase(void);

int pgp_verify_one(struct Body *sigbdy, struct State *s, const char *tempfile);
struct Body *pgp_traditional_encryptsign(struct Body *a, int flags, char *keylist);
struct Body *pgp_encrypt_message(struct Body *a, char *keylist, int sign);
struct Body *pgp_sign_message(struct Body *a);

int pgp_send_menu(struct Header *msg);

#endif /* CRYPT_BACKEND_CLASSIC_PGP */

#endif /* _NCRYPT_PGP_H */
