/**
 * Copyright (C) 2001-2002 Oliver Ehli <elmy@acm.org>
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

#ifndef _MUTT_SMIME_H
#define _MUTT_SMIME_H 1

#ifdef CRYPT_BACKEND_CLASSIC_SMIME

#include "mutt_crypt.h"

typedef struct smime_key
{
  char *email;
  char *hash;
  char *label;
  char *issuer;
  char trust; /* i=Invalid r=revoked e=expired u=unverified v=verified t=trusted */
  int flags;
  struct smime_key *next;
} smime_key_t;


void smime_void_passphrase(void);
int smime_valid_passphrase(void);

int smime_decrypt_mime(FILE *fpin, FILE **fpout, BODY *b, BODY **cur);

int smime_application_smime_handler(BODY *m, STATE *s);


BODY *smime_sign_message(BODY *a);

BODY *smime_build_smime_entity(BODY *a, char *certlist);

int smime_verify_one(BODY *sigbdy, STATE *s, const char *tempfile);


int smime_verify_sender(HEADER *h);

void smime_getkeys(ENVELOPE *env);

char *smime_find_keys(ADDRESS *adrlist, int oppenc_mode);

void smime_invoke_import(char *infile, char *mailbox);

int smime_send_menu(HEADER *msg);

#endif

#endif /* _MUTT_SMIME_H */
