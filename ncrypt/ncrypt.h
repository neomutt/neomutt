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

/**
 * @page ncrypt NCRYPT: Encrypt/decrypt/sign/verify emails
 *
 * Encrypt/decrypt/sign/verify emails
 *
 * | File                             | Description                          |
 * | :------------------------------- | :----------------------------------- |
 * | ncrypt/crypt.c                   | @subpage crypt_crypt                 |
 * | ncrypt/cryptglue.c               | @subpage crypt_cryptglue             |
 * | ncrypt/crypt_gpgme.c             | @subpage crypt_crypt_gpgme           |
 * | ncrypt/crypt_mod.c               | @subpage crypt_crypt_mod             |
 * | ncrypt/crypt_mod_pgp_classic.c   | @subpage crypt_crypt_mod_pgp         |
 * | ncrypt/crypt_mod_pgp_gpgme.c     | @subpage crypt_crypt_mod_pgp_gpgme   |
 * | ncrypt/crypt_mod_smime_classic.c | @subpage crypt_crypt_mod_smime       |
 * | ncrypt/crypt_mod_smime_gpgme.c   | @subpage crypt_crypt_mod_smime_gpgme |
 * | ncrypt/gnupgparse.c              | @subpage crypt_gnupg                 |
 * | ncrypt/pgp.c                     | @subpage crypt_pgp                   |
 * | ncrypt/pgpinvoke.c               | @subpage crypt_pgpinvoke             |
 * | ncrypt/pgpkey.c                  | @subpage crypt_pgpkey                |
 * | ncrypt/pgplib.c                  | @subpage crypt_pgplib                |
 * | ncrypt/pgpmicalg.c               | @subpage crypt_pgpmicalg             |
 * | ncrypt/pgppacket.c               | @subpage crypt_pgppacket             |
 * | ncrypt/smime.c                   | @subpage crypt_smime                 |
 */

#ifndef MUTT_NCRYPT_NCRYPT_H
#define MUTT_NCRYPT_NCRYPT_H

#include <stdbool.h>
#include <stdio.h>

struct Address;
struct Body;
struct Envelope;
struct Email;
struct EmailList;
struct State;

/* These Config Variables are only used in ncrypt/crypt.c */
extern bool          CryptTimestamp;
extern unsigned char PgpEncryptSelf; ///< Deprecated, see PgpSelfEncrypt
extern unsigned char PgpMimeAuto;
extern bool          PgpRetainableSigs;
extern bool          PgpSelfEncrypt;
extern bool          PgpStrictEnc;
extern unsigned char SmimeEncryptSelf; ///< Deprecated, see SmimeSelfEncrypt
extern bool          SmimeSelfEncrypt;

/* These Config Variables are only used in ncrypt/cryptglue.c */
extern bool CryptUseGpgme;

/* These Config Variables are only used in ncrypt/pgp.c */
extern bool          PgpCheckExit;
extern bool          PgpCheckGpgDecryptStatusFd;
extern struct Regex *PgpDecryptionOkay;
extern struct Regex *PgpGoodSign;
extern long          PgpTimeout;
extern bool          PgpUseGpgAgent;

/* These Config Variables are only used in ncrypt/pgpinvoke.c */
extern char *PgpClearsignCommand;
extern char *PgpDecodeCommand;
extern char *PgpDecryptCommand;
extern char *PgpEncryptOnlyCommand;
extern char *PgpEncryptSignCommand;
extern char *PgpExportCommand;
extern char *PgpGetkeysCommand;
extern char *PgpImportCommand;
extern char *PgpListPubringCommand;
extern char *PgpListSecringCommand;
extern char *PgpSignCommand;
extern char *PgpVerifyCommand;
extern char *PgpVerifyKeyCommand;

/* These Config Variables are only used in ncrypt/smime.c */
extern bool  SmimeAskCertLabel;
extern char *SmimeCaLocation;
extern char *SmimeCertificates;
extern char *SmimeDecryptCommand;
extern bool  SmimeDecryptUseDefaultKey;
extern char *SmimeEncryptCommand;
extern char *SmimeGetCertCommand;
extern char *SmimeGetCertEmailCommand;
extern char *SmimeGetSignerCertCommand;
extern char *SmimeImportCertCommand;
extern char *SmimeKeys;
extern char *SmimePk7outCommand;
extern char *SmimeSignCommand;
extern char *SmimeSignDigestAlg;
extern long  SmimeTimeout;
extern char *SmimeVerifyCommand;
extern char *SmimeVerifyOpaqueCommand;

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

#define PGP_ENCRYPT  (APPLICATION_PGP | ENCRYPT)
#define PGP_SIGN     (APPLICATION_PGP | SIGN)
#define PGP_GOODSIGN (APPLICATION_PGP | GOODSIGN)
#define PGP_KEY      (APPLICATION_PGP | KEYBLOCK)
#define PGP_INLINE   (APPLICATION_PGP | INLINE)

#define SMIME_ENCRYPT  (APPLICATION_SMIME | ENCRYPT)
#define SMIME_SIGN     (APPLICATION_SMIME | SIGN)
#define SMIME_GOODSIGN (APPLICATION_SMIME | GOODSIGN)
#define SMIME_BADSIGN  (APPLICATION_SMIME | BADSIGN)
#define SMIME_OPAQUE   (APPLICATION_SMIME | SIGNOPAQUE)

/* WITHCRYPTO actually replaces ifdefs so make the code more readable.
 * Because it is defined as a constant and known at compile time, the
 * compiler can do dead code elimination and thus it behaves
 * effectively as a conditional compile directive. It is set to false
 * if no crypto backend is configured or to a bit vector denoting the
 * configured backends. */
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

/* crypt.c */
void         crypt_extract_keys_from_messages(struct EmailList *el);
void         crypt_forget_passphrase(void);
int          crypt_get_keys(struct Email *msg, char **keylist, bool oppenc_mode);
void         crypt_opportunistic_encrypt(struct Email *msg);
int          crypt_query(struct Body *m);
int          crypt_valid_passphrase(int flags);
int          mutt_is_application_pgp(struct Body *m);
int          mutt_is_application_smime(struct Body *m);
int          mutt_is_malformed_multipart_pgp_encrypted(struct Body *b);
int          mutt_is_multipart_encrypted(struct Body *b);
int          mutt_is_multipart_signed(struct Body *b);
int          mutt_is_valid_multipart_pgp_encrypted(struct Body *b);
int          mutt_protect(struct Email *msg, char *keylist);
int          mutt_protected_headers_handler(struct Body *m, struct State *s);
bool         mutt_should_hide_protected_subject(struct Email *e);
int          mutt_signed_handler(struct Body *a, struct State *s);

/* cryptglue.c */
bool         crypt_has_module_backend(int type);
void         crypt_init(void);
void         crypt_invoke_message(int type);
int          crypt_pgp_application_handler(struct Body *m, struct State *s);
int          crypt_pgp_check_traditional(FILE *fp, struct Body *b, bool just_one);
int          crypt_pgp_decrypt_mime(FILE *a, FILE **b, struct Body *c, struct Body **d);
int          crypt_pgp_encrypted_handler(struct Body *a, struct State *s);
void         crypt_pgp_extract_key_from_attachment(FILE *fp, struct Body *top);
void         crypt_pgp_invoke_getkeys(struct Address *addr);
struct Body *crypt_pgp_make_key_attachment(void);
int          crypt_pgp_send_menu(struct Email *msg);
int          crypt_smime_application_handler(struct Body *m, struct State *s);
int          crypt_smime_decrypt_mime(FILE *a, FILE **b, struct Body *c, struct Body **d);
void         crypt_smime_getkeys(struct Envelope *env);
int          crypt_smime_send_menu(struct Email *msg);
int          crypt_smime_verify_sender(struct Email *e);

/* crypt_mod.c */
void crypto_module_free(void);

#endif /* MUTT_NCRYPT_NCRYPT_H */
