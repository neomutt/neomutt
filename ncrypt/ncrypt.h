/**
 * @file
 * API for encryption/signing of emails
 *
 * @authors
 * Copyright (C) 2003 Werner Koch <wk@gnupg.org>
 * Copyright (C) 2004 g10code GmbH
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

#ifndef _NCRYPT_NCRYPT_H
#define _NCRYPT_NCRYPT_H

#include <stdio.h>

struct Address;
struct Body;
struct Envelope;
struct Header;
struct State;

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
   if no crypto backend is configured or to a bit vector denoting the
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

/* -- crypt.c -- */
int crypt_query(struct Body *m);
void crypt_extract_keys_from_messages(struct Header *h);
int crypt_get_keys(struct Header *msg, char **keylist, int oppenc_mode);
void crypt_opportunistic_encrypt(struct Header *msg);
void crypt_forget_passphrase(void);
int crypt_valid_passphrase(int flags);

/* -- cryptglue.c -- */
void crypt_invoke_message(int type);
int crypt_pgp_decrypt_mime(FILE *a, FILE **b, struct Body *c, struct Body **d);
int crypt_pgp_application_pgp_handler(struct Body *m, struct State *s);
int crypt_pgp_encrypted_handler(struct Body *a, struct State *s);
void crypt_pgp_invoke_getkeys(struct Address *addr);
int crypt_pgp_check_traditional (FILE *fp, struct Body *b, int just_one);
struct Body *crypt_pgp_make_key_attachment(char *tempf);
int crypt_pgp_send_menu(struct Header *msg);
void crypt_pgp_extract_keys_from_attachment_list(FILE *fp, int tag, struct Body *top);
int crypt_smime_decrypt_mime(FILE *a, FILE **b, struct Body *c, struct Body **d);
int crypt_smime_application_smime_handler(struct Body *m, struct State *s);
void crypt_smime_getkeys(struct Envelope *env);
int crypt_smime_verify_sender(struct Header *h);
int crypt_smime_send_menu(struct Header *msg);
void crypt_init(void);

/* Returns 1 if a module backend is registered for the type */
int crypt_has_module_backend(int type);

#endif /* _NCRYPT_NCRYPT_H */
