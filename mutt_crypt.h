/**
 * Copyright (C) 2003 Werner Koch <wk@gnupg.org>
 * Copyright (C) 2004 g10code GmbH
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

/*
   Common definitions and prototypes for the crypt functions. They are
   all defined in crypt.c and cryptglue.c
*/

#ifndef _MUTT_CRYPT_H
#define _MUTT_CRYPT_H 1

#include "mutt.h"
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
#define OPPENCRYPT (1 << 8) /* Opportunistic encrypt mode */

#define APPLICATION_PGP   (1 << 9)
#define APPLICATION_SMIME (1 << 10)

#define PGP_TRADITIONAL_CHECKED (1 << 11)

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
#if (defined(CRYPT_BACKEND_CLASSIC_PGP) && defined(CRYPT_BACKEND_CLASSIC_SMIME)) || \
    defined(CRYPT_BACKEND_GPGME)
#define WithCrypto (APPLICATION_PGP | APPLICATION_SMIME)
#elif defined(CRYPT_BACKEND_CLASSIC_PGP)
#define WithCrypto APPLICATION_PGP
#elif defined(CRYPT_BACKEND_CLASSIC_SMIME)
#define WithCrypto APPLICATION_SMIME
#else
#define WithCrypto 0
#endif


#define KEYFLAG_CANSIGN           (1 << 0)
#define KEYFLAG_CANENCRYPT        (1 << 1)
#define KEYFLAG_ISX509            (1 << 2)
#define KEYFLAG_SECRET            (1 << 7)
#define KEYFLAG_EXPIRED           (1 << 8)
#define KEYFLAG_REVOKED           (1 << 9)
#define KEYFLAG_DISABLED          (1 << 10)
#define KEYFLAG_SUBKEY            (1 << 11)
#define KEYFLAG_CRITICAL          (1 << 12)
#define KEYFLAG_PREFER_ENCRYPTION (1 << 13)
#define KEYFLAG_PREFER_SIGNING    (1 << 14)

#define KEYFLAG_CANTUSE (KEYFLAG_DISABLED | KEYFLAG_REVOKED | KEYFLAG_EXPIRED)
#define KEYFLAG_RESTRICTIONS (KEYFLAG_CANTUSE | KEYFLAG_CRITICAL)

#define KEYFLAG_ABILITIES (KEYFLAG_CANSIGN | KEYFLAG_CANENCRYPT | KEYFLAG_PREFER_ENCRYPTION | KEYFLAG_PREFER_SIGNING)

enum pgp_ring
{
  PGP_PUBRING,
  PGP_SECRING
};
typedef enum pgp_ring pgp_ring_t;

/* Some prototypes -- old crypt.h. */

int mutt_protect(struct Header *msg, char *keylist);

int mutt_is_multipart_encrypted(struct Body *b);

int mutt_is_valid_multipart_pgp_encrypted(struct Body *b);

int mutt_is_malformed_multipart_pgp_encrypted(struct Body *b);

int mutt_is_multipart_signed(struct Body *b);

int mutt_is_application_pgp(struct Body *m);

int mutt_is_application_smime(struct Body *m);

int mutt_signed_handler(struct Body *a, struct State *s);

int mutt_parse_crypt_hdr(const char *p, int set_empty_signas, int crypt_app);

void convert_to_7bit(struct Body *a);

/* -- crypt.c -- */

/* Print the current time. */
void crypt_current_time(struct State *s, char *app_name);

/* Check out the type of encryption used and set the cached status
   values if there are any. */
int crypt_query(struct Body *m);

/* Fixme: To be documented. */
void crypt_extract_keys_from_messages(struct Header *h);

/* Do a quick check to make sure that we can find all of the
   encryption keys if the user has requested this service.
   Return the list of keys in KEYLIST.
   If oppenc_mode is true, only keys that can be determined without
   prompting will be used.  */
int crypt_get_keys(struct Header *msg, char **keylist, int oppenc_mode);

/* Check if all recipients keys can be automatically determined.
 * Enable encryption if they can, otherwise disable encryption.  */
void crypt_opportunistic_encrypt(struct Header *msg);

/* Forget a passphrase and display a message. */
void crypt_forget_passphrase(void);

/* Check that we have a usable passphrase, ask if not. */
int crypt_valid_passphrase(int flags);

/* Write the message body/part A described by state S to the given
   TEMPFILE.  */
int crypt_write_signed(struct Body *a, struct State *s, const char *tempfile);

/* Obtain pointers to fingerprint or short or long key ID, if any.

   Upon return, at most one of return, *ppl and *pps pointers is non-NULL,
   indicating the longest fingerprint or ID found, if any.

   Return:  Copy of fingerprint, if any, stripped of all spaces, else NULL.
            Must be FREE'd by caller.
   *pphint  Start of string to be passed to pgp_add_string_to_hints() or
            crypt_add_string_to_hints().
   *ppl     Start of long key ID if detected, else NULL.
   *pps     Start of short key ID if detected, else NULL. */
const char *crypt_get_fingerprint_or_id(char *p, const char **pphint,
                                        const char **ppl, const char **pps);

/* Check if a string contains a numerical key */
bool crypt_is_numerical_keyid(const char *s);

/* -- cryptglue.c -- */

/* Show a message that a backend will be invoked. */
void crypt_invoke_message(int type);


/* Silently forget about a passphrase. */
void crypt_pgp_void_passphrase(void);

int crypt_pgp_valid_passphrase(void);


/* Decrypt a PGP/MIME message. */
int crypt_pgp_decrypt_mime(FILE *a, FILE **b, struct Body *c, struct Body **d);

/* MIME handler for the application/pgp content-type. */
int crypt_pgp_application_pgp_handler(struct Body *m, struct State *s);

/* MIME handler for an PGP/MIME encrypted message. */
int crypt_pgp_encrypted_handler(struct Body *a, struct State *s);

/* fixme: needs documentation. */
void crypt_pgp_invoke_getkeys(struct Address *addr);

/* Check for a traditional PGP message in body B. */
int crypt_pgp_check_traditional(FILE *fp, struct Body *b, int tagged_only);

/* fixme: needs documentation. */
struct Body *crypt_pgp_traditional_encryptsign(struct Body *a, int flags, char *keylist);

/* Generate a PGP public key attachment. */
struct Body *crypt_pgp_make_key_attachment(char *tempf);

/* This routine attempts to find the keyids of the recipients of a
   message.  It returns NULL if any of the keys can not be found.
   If oppenc_mode is true, only keys that can be determined without
   prompting will be used.  */
char *crypt_pgp_findkeys(struct Address *adrlist, int oppenc_mode);

/* Create a new body with a PGP signed message from A. */
struct Body *crypt_pgp_sign_message(struct Body *a);

/* Warning: A is no longer freed in this routine, you need to free it
   later.  This is necessary for $fcc_attach. */
struct Body *crypt_pgp_encrypt_message(struct Body *a, char *keylist, int sign);

/* Invoke the PGP command to import a key. */
void crypt_pgp_invoke_import(const char *fname);

int crypt_pgp_send_menu(struct Header *msg);

/* fixme: needs documentation */
int crypt_pgp_verify_one(struct Body *sigbdy, struct State *s, const char *tempf);

/* fixme: needs documentation */
void crypt_pgp_extract_keys_from_attachment_list(FILE *fp, int tag, struct Body *top);

void crypt_pgp_set_sender(const char *sender);

/* Silently forget about a passphrase. */
void crypt_smime_void_passphrase(void);

int crypt_smime_valid_passphrase(void);

/* Decrypt an S/MIME message. */
int crypt_smime_decrypt_mime(FILE *a, FILE **b, struct Body *c, struct Body **d);

/* MIME handler for the application/smime content-type. */
int crypt_smime_application_smime_handler(struct Body *m, struct State *s);

/* fixme: Needs documentation. */
void crypt_smime_getkeys(struct Envelope *env);

/* Check that the sender matches. */
int crypt_smime_verify_sender(struct Header *h);

/* Ask for an SMIME key. */

/* This routine attempts to find the keyids of the recipients of a
   message.  It returns NULL if any of the keys can not be found.
   If oppenc_mode is true, only keys that can be determined without
   prompting will be used.  */
char *crypt_smime_findkeys(struct Address *adrlist, int oppenc_mode);

/* fixme: Needs documentation. */
struct Body *crypt_smime_sign_message(struct Body *a);

/* fixme: needs documentation. */
struct Body *crypt_smime_build_smime_entity(struct Body *a, char *certlist);

/* Add a certificate and update index file (externally). */
void crypt_smime_invoke_import(char *infile, char *mailbox);

int crypt_smime_send_menu(struct Header *msg);

void crypt_smime_set_sender(const char *sender);

/* fixme: needs documentation */
int crypt_smime_verify_one(struct Body *sigbdy, struct State *s, const char *tempf);

void crypt_init(void);

#endif /* _MUTT_CRYPT_H */
