/*
 * Copyright (C) 2001,2002 Oliver Ehli <elmy@acm.org>
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */


#ifdef CRYPT_BACKEND_CLASSIC_SMIME

#include "mutt_crypt.h"





#define smime_valid_passphrase() crypt_valid_passphrase(APPLICATION_SMIME)

void smime_void_passphrase (void);

int   smime_decrypt_mime (FILE *, FILE **, BODY *, BODY **);

void  smime_application_smime_handler (BODY *, STATE *);


BODY* smime_sign_message (BODY *);

BODY* smime_build_smime_entity (BODY *, char *);

int   smime_verify_one(BODY *, STATE *, const char *);


int   smime_verify_sender(HEADER *);


char* smime_get_field_from_db (char *, char *, short, short);

void  smime_getkeys (ENVELOPE *);

char* smime_ask_for_key (char *, char *, short);

char *smime_findKeys (ADDRESS *to, ADDRESS *cc, ADDRESS *bcc);

void  smime_invoke_import (char *, char *);

#endif


