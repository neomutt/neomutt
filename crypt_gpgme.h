/**
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

#ifndef _MUTT_CRYPT_GPGME_H
#define _MUTT_CRYPT_GPGME_H 1

#include "mutt_crypt.h"

void pgp_gpgme_init (void);
void smime_gpgme_init (void);

char *pgp_gpgme_findkeys (ADDRESS *adrlist, int oppenc_mode);
char *smime_gpgme_findkeys (ADDRESS *adrlist, int oppenc_mode);

BODY *pgp_gpgme_encrypt_message (BODY *a, char *keylist, int sign);
BODY *smime_gpgme_build_smime_entity (BODY *a, char *keylist);

int pgp_gpgme_decrypt_mime (FILE *fpin, FILE **fpout, BODY *b, BODY **cur);
int smime_gpgme_decrypt_mime (FILE *fpin, FILE **fpout, BODY *b, BODY **cur);

int pgp_gpgme_check_traditional (FILE *fp, BODY *b, int tagged_only);
void pgp_gpgme_invoke_import (const char* fname);

int pgp_gpgme_application_handler (BODY *m, STATE *s);
int smime_gpgme_application_handler (BODY *a, STATE *s);
int pgp_gpgme_encrypted_handler (BODY *a, STATE *s);

BODY *pgp_gpgme_make_key_attachment (char *tempf);

BODY *pgp_gpgme_sign_message (BODY *a);
BODY *smime_gpgme_sign_message (BODY *a);

int pgp_gpgme_verify_one (BODY *sigbdy, STATE *s, const char *tempfile);
int smime_gpgme_verify_one (BODY *sigbdy, STATE *s, const char *tempfile);

int pgp_gpgme_send_menu (HEADER *msg);
int smime_gpgme_send_menu (HEADER *msg);

int smime_gpgme_verify_sender (HEADER *h);

void mutt_gpgme_set_sender (const char *sender);

#endif /* _MUTT_CRYPT_GPGME_H */
