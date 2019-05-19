/**
 * @file
 * API for encryption/signing of emails
 *
 * @authors
 * Copyright (C) 2003 Werner Koch <wk@gnupg.org>
 * Copyright (C) 2004 g10code GmbH
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
#include <stdint.h>
#include <stdio.h>

struct Address;
struct AddressList;
struct Body;
struct Envelope;
struct Email;
struct EmailList;
struct State;

/* These Config Variables are only used in ncrypt/crypt.c */
extern bool          C_CryptTimestamp;
extern unsigned char C_PgpEncryptSelf; ///< Deprecated, see #C_PgpSelfEncrypt
extern unsigned char C_PgpMimeAuto;
extern bool          C_PgpRetainableSigs;
extern bool          C_PgpSelfEncrypt;
extern bool          C_PgpStrictEnc;
extern unsigned char C_SmimeEncryptSelf; ///< Deprecated, see #C_SmimeSelfEncrypt
extern bool          C_SmimeSelfEncrypt;

/* These Config Variables are only used in ncrypt/cryptglue.c */
extern bool C_CryptUseGpgme;

/* These Config Variables are only used in ncrypt/pgp.c */
extern bool          C_PgpCheckExit;
extern bool          C_PgpCheckGpgDecryptStatusFd;
extern struct Regex *C_PgpDecryptionOkay;
extern struct Regex *C_PgpGoodSign;
extern long          C_PgpTimeout;
extern bool          C_PgpUseGpgAgent;

/* These Config Variables are only used in ncrypt/pgpinvoke.c */
extern char *C_PgpClearsignCommand;
extern char *C_PgpDecodeCommand;
extern char *C_PgpDecryptCommand;
extern char *C_PgpEncryptOnlyCommand;
extern char *C_PgpEncryptSignCommand;
extern char *C_PgpExportCommand;
extern char *C_PgpGetkeysCommand;
extern char *C_PgpImportCommand;
extern char *C_PgpListPubringCommand;
extern char *C_PgpListSecringCommand;
extern char *C_PgpSignCommand;
extern char *C_PgpVerifyCommand;
extern char *C_PgpVerifyKeyCommand;

/* These Config Variables are only used in ncrypt/smime.c */
extern bool  C_SmimeAskCertLabel;
extern char *C_SmimeCaLocation;
extern char *C_SmimeCertificates;
extern char *C_SmimeDecryptCommand;
extern bool  C_SmimeDecryptUseDefaultKey;
extern char *C_SmimeEncryptCommand;
extern char *C_SmimeGetCertCommand;
extern char *C_SmimeGetCertEmailCommand;
extern char *C_SmimeGetSignerCertCommand;
extern char *C_SmimeImportCertCommand;
extern char *C_SmimeKeys;
extern char *C_SmimePk7outCommand;
extern char *C_SmimeSignCommand;
extern char *C_SmimeSignDigestAlg;
extern long  C_SmimeTimeout;
extern char *C_SmimeVerifyCommand;
extern char *C_SmimeVerifyOpaqueCommand;

typedef uint16_t SecurityFlags;           ///< Flags, e.g. #SEC_ENCRYPT
#define SEC_NO_FLAGS                  0   ///< No flags are set
#define SEC_ENCRYPT             (1 << 0)  ///< Email is encrypted
#define SEC_SIGN                (1 << 1)  ///< Email is signed
#define SEC_GOODSIGN            (1 << 2)  ///< Email has a valid signature
#define SEC_BADSIGN             (1 << 3)  ///< Email has a bad signature
#define SEC_PARTSIGN            (1 << 4)  ///< Not all parts of the email is signed
#define SEC_SIGNOPAQUE          (1 << 5)  ///< Email has an opaque signature (encrypted)
#define SEC_KEYBLOCK            (1 << 6)  ///< Email has a key attached
#define SEC_INLINE              (1 << 7)  ///< Email has an inline signature
#define SEC_OPPENCRYPT          (1 << 8)  ///< Opportunistic encrypt mode
#define APPLICATION_PGP         (1 << 9)  ///< Use PGP to encrypt/sign
#define APPLICATION_SMIME       (1 << 10) ///< Use SMIME to encrypt/sign
#define PGP_TRADITIONAL_CHECKED (1 << 11) ///< Email has a traditional (inline) signature

#define SEC_ALL_FLAGS          ((1 << 12) - 1)

#define PGP_ENCRYPT  (APPLICATION_PGP | SEC_ENCRYPT)
#define PGP_SIGN     (APPLICATION_PGP | SEC_SIGN)
#define PGP_GOODSIGN (APPLICATION_PGP | SEC_GOODSIGN)
#define PGP_KEY      (APPLICATION_PGP | SEC_KEYBLOCK)
#define PGP_INLINE   (APPLICATION_PGP | SEC_INLINE)

#define SMIME_ENCRYPT  (APPLICATION_SMIME | SEC_ENCRYPT)
#define SMIME_SIGN     (APPLICATION_SMIME | SEC_SIGN)
#define SMIME_GOODSIGN (APPLICATION_SMIME | SEC_GOODSIGN)
#define SMIME_BADSIGN  (APPLICATION_SMIME | SEC_BADSIGN)
#define SMIME_OPAQUE   (APPLICATION_SMIME | SEC_SIGNOPAQUE)

/* WITHCRYPTO actually replaces ifdefs to make the code more readable.
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

typedef uint16_t KeyFlags;                  ///< Flags describing PGP/SMIME keys, e.g. #KEYFLAG_CANSIGN
#define KEYFLAG_NO_FLAGS                0   ///< No flags are set
#define KEYFLAG_CANSIGN           (1 << 0)  ///< Key is suitable for signing
#define KEYFLAG_CANENCRYPT        (1 << 1)  ///< Key is suitable for encryption
#define KEYFLAG_ISX509            (1 << 2)  ///< Key is an X.509 key
#define KEYFLAG_SECRET            (1 << 7)  ///< Key is a secret key
#define KEYFLAG_EXPIRED           (1 << 8)  ///< Key is expired
#define KEYFLAG_REVOKED           (1 << 9)  ///< Key is revoked
#define KEYFLAG_DISABLED          (1 << 10) ///< Key is marked disabled
#define KEYFLAG_SUBKEY            (1 << 11) ///< Key is a subkey
#define KEYFLAG_CRITICAL          (1 << 12) ///< Key is marked critical
#define KEYFLAG_PREFER_ENCRYPTION (1 << 13) ///< Key's owner prefers encryption
#define KEYFLAG_PREFER_SIGNING    (1 << 14) ///< Key's owner prefers signing

#define KEYFLAG_CANTUSE (KEYFLAG_DISABLED | KEYFLAG_REVOKED | KEYFLAG_EXPIRED)
#define KEYFLAG_RESTRICTIONS (KEYFLAG_CANTUSE | KEYFLAG_CRITICAL)

#define KEYFLAG_ABILITIES (KEYFLAG_CANSIGN | KEYFLAG_CANENCRYPT | KEYFLAG_PREFER_ENCRYPTION | KEYFLAG_PREFER_SIGNING)

/* crypt.c */
void         crypt_extract_keys_from_messages(struct EmailList *el);
void         crypt_forget_passphrase(void);
int          crypt_get_keys(struct Email *msg, char **keylist, bool oppenc_mode);
void         crypt_opportunistic_encrypt(struct Email *msg);
SecurityFlags crypt_query(struct Body *m);
bool         crypt_valid_passphrase(SecurityFlags flags);
SecurityFlags mutt_is_application_pgp(struct Body *m);
SecurityFlags mutt_is_application_smime(struct Body *m);
SecurityFlags mutt_is_malformed_multipart_pgp_encrypted(struct Body *b);
SecurityFlags mutt_is_multipart_encrypted(struct Body *b);
SecurityFlags mutt_is_multipart_signed(struct Body *b);
int          mutt_is_valid_multipart_pgp_encrypted(struct Body *b);
int          mutt_protect(struct Email *msg, char *keylist);
int          mutt_protected_headers_handler(struct Body *m, struct State *s);
bool         mutt_should_hide_protected_subject(struct Email *e);
int          mutt_signed_handler(struct Body *a, struct State *s);

/* cryptglue.c */
bool         crypt_has_module_backend(SecurityFlags type);
void         crypt_init(void);
void         crypt_invoke_message(SecurityFlags type);
int          crypt_pgp_application_handler(struct Body *m, struct State *s);
int          crypt_pgp_check_traditional(FILE *fp, struct Body *b, bool just_one);
int          crypt_pgp_decrypt_mime(FILE *fp_in, FILE **fp_out, struct Body *b, struct Body **cur);
int          crypt_pgp_encrypted_handler(struct Body *a, struct State *s);
void         crypt_pgp_extract_key_from_attachment(FILE *fp, struct Body *top);
void         crypt_pgp_invoke_getkeys(struct Address *addr);
struct Body *crypt_pgp_make_key_attachment(void);
int          crypt_pgp_send_menu(struct Email *msg);
int          crypt_smime_application_handler(struct Body *m, struct State *s);
int          crypt_smime_decrypt_mime(FILE *fp_in, FILE **fp_out, struct Body *b, struct Body **cur);
void         crypt_smime_getkeys(struct Envelope *env);
int          crypt_smime_send_menu(struct Email *msg);
int          crypt_smime_verify_sender(struct Email *e);

/* crypt_mod.c */
void crypto_module_free(void);

#endif /* MUTT_NCRYPT_NCRYPT_H */
