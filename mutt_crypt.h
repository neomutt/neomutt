/*
 * Copyright (C) 2003 Werner Koch <wk@gnupg.org>
 * Copyright (C) 2004 g10code GmbH
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
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

/*
   Common definitions and prototypes for the crypt functions. They are
   all defined in crypt.c and cryptglue.c
*/

#ifndef MUTT_CRYPT_H
#define MUTT_CRYPT_H

#include "mutt.h"        /* Need this to declare BODY, ADDRESS. STATE etc. */
/* FIXME: They should be pointer to anonymous structures for better
   information hiding. */



#define ENCRYPT    (1 << 0)
#define SIGN       (1 << 1)
#define GOODSIGN   (1 << 2)
#define BADSIGN    (1 << 3)
#define PARTSIGN   (1 << 4)
#define SIGNOPAQUE (1 << 5)
#define KEYBLOCK   (1 << 6) /* KEY too generic? */
#define INLINE     (1 << 7)

#define APPLICATION_PGP    (1 << 8) 
#define APPLICATION_SMIME  (1 << 9)

#define PGP_TRADITIONAL_CHECKED (1 << 10)

#define PGPENCRYPT  (APPLICATION_PGP | ENCRYPT)
#define PGPSIGN     (APPLICATION_PGP | SIGN)
#define PGPGOODSIGN (APPLICATION_PGP | GOODSIGN)
#define PGPKEY      (APPLICATION_PGP | KEYBLOCK) 
#define PGPINLINE   (APPLICATION_PGP | INLINE)

#define SMIMEENCRYPT  (APPLICATION_SMIME | ENCRYPT)
#define SMIMESIGN     (APPLICATION_SMIME | SIGN)
#define SMIMEGOODSIGN (APPLICATION_SMIME | GOODSIGN)
#define SMIMEBADSIGN  (APPLICATION_SMIME | BADSIGN)
#define SMIMEOPAQUE   (APPLICATION_SMIME | SIGNOPAQUE)


/* WITHCRYPTO actually replaces ifdefs so make the code more readable.
   Because it is defined as a constant and known at compile time, the
   compiler can do dead code elimination and thus it behaves
   effectively as a conditional compile directive. It is set to false
   if no crypto backend is configures or to a bit vector denoting the
   configured backends. */
#if (defined(CRYPT_BACKEND_CLASSIC_PGP) && defined(CRYPT_BACKEND_CLASSIC_SMIME)) || defined (CRYPT_BACKEND_GPGME)
# define WithCrypto (APPLICATION_PGP | APPLICATION_SMIME)
#elif defined(CRYPT_BACKEND_CLASSIC_PGP)
# define WithCrypto  APPLICATION_PGP
#elif defined(CRYPT_BACKEND_CLASSIC_SMIME)
# define WithCrypto  APPLICATION_SMIME
#else
# define WithCrypto 0
#endif


#define KEYFLAG_CANSIGN 		(1 <<  0)
#define KEYFLAG_CANENCRYPT 		(1 <<  1)
#define KEYFLAG_ISX509                  (1 <<  2)
#define KEYFLAG_SECRET			(1 <<  7)
#define KEYFLAG_EXPIRED 		(1 <<  8)
#define KEYFLAG_REVOKED 		(1 <<  9)
#define KEYFLAG_DISABLED 		(1 << 10)
#define KEYFLAG_SUBKEY 			(1 << 11)
#define KEYFLAG_CRITICAL 		(1 << 12)
#define KEYFLAG_PREFER_ENCRYPTION 	(1 << 13)
#define KEYFLAG_PREFER_SIGNING 		(1 << 14)

#define KEYFLAG_CANTUSE (KEYFLAG_DISABLED|KEYFLAG_REVOKED|KEYFLAG_EXPIRED)
#define KEYFLAG_RESTRICTIONS (KEYFLAG_CANTUSE|KEYFLAG_CRITICAL)

#define KEYFLAG_ABILITIES (KEYFLAG_CANSIGN|KEYFLAG_CANENCRYPT|KEYFLAG_PREFER_ENCRYPTION|KEYFLAG_PREFER_SIGNING)

enum pgp_ring
{
  PGP_PUBRING,
  PGP_SECRING
};
typedef enum pgp_ring pgp_ring_t;


struct pgp_keyinfo;
typedef struct pgp_keyinfo *pgp_key_t;



/* Some prototypes -- old crypt.h. */

int mutt_protect (HEADER *, char *);

int mutt_is_multipart_encrypted (BODY *);

int mutt_is_multipart_signed (BODY *);

int mutt_is_application_pgp (BODY *);

int mutt_is_application_smime (BODY *);

int mutt_signed_handler (BODY *, STATE *);

int mutt_parse_crypt_hdr (char *, int, int);


void convert_to_7bit (BODY *);



/*-- crypt.c --*/ 

/* Print the current time. */ 
void crypt_current_time(STATE *s, char *app_name);

/* Check out the type of encryption used and set the cached status
   values if there are any. */
int crypt_query (BODY *m);

/* Fixme: To be documented. */
void crypt_extract_keys_from_messages (HEADER *h);

/* Do a quick check to make sure that we can find all of the
   encryption keys if the user has requested this service. 
   Return the list of keys in KEYLIST. */
int crypt_get_keys (HEADER *msg, char **keylist);

/* Forget a passphrase and display a message. */
void crypt_forget_passphrase (void);

/* Check that we have a usable passphrase, ask if not. */
int crypt_valid_passphrase (int);

/* Write the message body/part A described by state S to a the given
   TEMPFILE.  */
int crypt_write_signed(BODY *a, STATE *s, const char *tempf);



/*-- cryptglue.c --*/

/* Show a message that a backend will be invoked. */
void crypt_invoke_message (int type);


/* Silently forget about a passphrase. */
void crypt_pgp_void_passphrase (void);

int crypt_pgp_valid_passphrase (void);


/* Decrypt a PGP/MIME message. */
int crypt_pgp_decrypt_mime (FILE *a, FILE **b, BODY *c, BODY **d);

/* MIME handler for the application/pgp content-type. */
int crypt_pgp_application_pgp_handler (BODY *m, STATE *s);

/* MIME handler for an PGP/MIME encrypted message. */
int crypt_pgp_encrypted_handler (BODY *a, STATE *s);

/* fixme: needs documentation. */
void crypt_pgp_invoke_getkeys (ADDRESS *addr);

/* Ask for a PGP key. */
pgp_key_t crypt_pgp_ask_for_key (char *tag, char *whatfor,
                                 short abilities, pgp_ring_t keyring);

/* Check for a traditional PGP message in body B. */
int crypt_pgp_check_traditional (FILE *fp, BODY *b, int tagged_only);

/* fixme: needs documentation. */
BODY *crypt_pgp_traditional_encryptsign (BODY *a, int flags, char *keylist);

/* Release the PGP key KPP (note, that we pass a pointer to it). */
void crypt_pgp_free_key (pgp_key_t *kpp);

/* Generate a PGP public key attachment. */
BODY *crypt_pgp_make_key_attachment (char *tempf);

/* This routine attempts to find the keyids of the recipients of a
   message.  It returns NULL if any of the keys can not be found.  */
char *crypt_pgp_findkeys (ADDRESS *to, ADDRESS *cc, ADDRESS *bcc);

/* Create a new body with a PGP signed message from A. */
BODY *crypt_pgp_sign_message (BODY *a);

/* Warning: A is no longer freed in this routine, you need to free it
   later.  This is necessary for $fcc_attach. */
BODY *crypt_pgp_encrypt_message (BODY *a, char *keylist, int sign);

/* Invoke the PGP command to import a key. */
void crypt_pgp_invoke_import (const char *fname);

int crypt_pgp_send_menu (HEADER *msg, int *redraw);

/* fixme: needs documentation */
int crypt_pgp_verify_one (BODY *sigbdy, STATE *s, const char *tempf);

/* Access the keyID in K. */
char *crypt_pgp_keyid (pgp_key_t k);

/* fixme: needs documentation */
void crypt_pgp_extract_keys_from_attachment_list (FILE *fp, int tag,BODY *top);

void crypt_pgp_set_sender (const char *sender);



/* Silently forget about a passphrase. */
void crypt_smime_void_passphrase (void);

int crypt_smime_valid_passphrase (void);

/* Decrypt an S/MIME message. */
int crypt_smime_decrypt_mime (FILE *a, FILE **b, BODY *c, BODY **d);

/* MIME handler for the application/smime content-type. */
int crypt_smime_application_smime_handler (BODY *m, STATE *s);

/* fixme: Needs documentation. */
void crypt_smime_getkeys (ENVELOPE *env);

/* Check that the sender matches. */
int crypt_smime_verify_sender(HEADER *h);

/* Ask for an SMIME key. */
char *crypt_smime_ask_for_key (char *prompt, char *mailbox, short public);

/* This routine attempts to find the keyids of the recipients of a
   message.  It returns NULL if any of the keys can not be found.  */
char *crypt_smime_findkeys (ADDRESS *to, ADDRESS *cc, ADDRESS *bcc);

/* fixme: Needs documentation. */
BODY *crypt_smime_sign_message (BODY *a);

/* fixme: needs documentation. */
BODY *crypt_smime_build_smime_entity (BODY *a, char *certlist);

/* Add a certificate and update index file (externally). */
void crypt_smime_invoke_import (char *infile, char *mailbox);

int crypt_smime_send_menu (HEADER *msg, int *redraw);

void crypt_smime_set_sender (const char *sender);

/* fixme: needs documentation */
int crypt_smime_verify_one (BODY *sigbdy, STATE *s, const char *tempf);

void crypt_init (void);

#endif /*MUTT_CRYPT_H*/
