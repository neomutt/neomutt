/**
 * @file
 * API for encryption/signing of emails
 *
 * @authors
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2021 Pietro Cerutti <gahr@gahr.ch>
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
 * @page lib_ncrypt Ncrypt
 *
 * Encrypt/decrypt/sign/verify emails
 *
 * | File                             | Description                          |
 * | :------------------------------- | :----------------------------------- |
 * | ncrypt/config.c                  | @subpage crypt_config                |
 * | ncrypt/crypt.c                   | @subpage crypt_crypt                 |
 * | ncrypt/crypt_gpgme.c             | @subpage crypt_crypt_gpgme           |
 * | ncrypt/crypt_mod.c               | @subpage crypt_crypt_mod             |
 * | ncrypt/crypt_mod_pgp_classic.c   | @subpage crypt_crypt_mod_pgp         |
 * | ncrypt/crypt_mod_pgp_gpgme.c     | @subpage crypt_crypt_mod_pgp_gpgme   |
 * | ncrypt/crypt_mod_smime_classic.c | @subpage crypt_crypt_mod_smime       |
 * | ncrypt/crypt_mod_smime_gpgme.c   | @subpage crypt_crypt_mod_smime_gpgme |
 * | ncrypt/cryptglue.c               | @subpage crypt_cryptglue             |
 * | ncrypt/dlg_gpgme.c               | @subpage crypt_dlg_gpgme             |
 * | ncrypt/dlg_pgp.c                 | @subpage crypt_dlg_pgp               |
 * | ncrypt/dlg_smime.c               | @subpage crypt_dlg_smime             |
 * | ncrypt/expando_command.c         | @subpage ncrypt_expando_command      |
 * | ncrypt/expando_gpgme.c           | @subpage ncrypt_expando_gpgme        |
 * | ncrypt/expando_pgp.c             | @subpage ncrypt_expando_pgp          |
 * | ncrypt/expando_smime.c           | @subpage ncrypt_expando_smime        |
 * | ncrypt/functions.c               | @subpage crypt_functions             |
 * | ncrypt/gnupgparse.c              | @subpage crypt_gnupg                 |
 * | ncrypt/gpgme_functions.c         | @subpage crypt_gpgme_functions       |
 * | ncrypt/module.c                  | @subpage ncrypt_module               |
 * | ncrypt/pgp.c                     | @subpage crypt_pgp                   |
 * | ncrypt/pgp_functions.c           | @subpage pgp_functions               |
 * | ncrypt/pgpinvoke.c               | @subpage crypt_pgpinvoke             |
 * | ncrypt/pgpkey.c                  | @subpage crypt_pgpkey                |
 * | ncrypt/pgplib.c                  | @subpage crypt_pgplib                |
 * | ncrypt/pgpmicalg.c               | @subpage crypt_pgpmicalg             |
 * | ncrypt/pgppacket.c               | @subpage crypt_pgppacket             |
 * | ncrypt/smime.c                   | @subpage crypt_smime                 |
 * | ncrypt/smime_functions.c         | @subpage smime_functions             |
 * | ncrypt/sort_gpgme.c              | @subpage crypt_sort_gpgme            |
 * | ncrypt/sort_pgp.c                | @subpage crypt_sort_pgp              |
 */

#ifndef MUTT_NCRYPT_LIB_H
#define MUTT_NCRYPT_LIB_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

struct Address;
struct Body;
#ifdef USE_AUTOCRYPT
struct Buffer;
#endif
struct Email;
struct EmailArray;
struct Envelope;
struct Mailbox;
struct Message;
struct State;

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
#define SEC_AUTOCRYPT           (1 << 9)  ///< (Autocrypt) Message will be, or was Autocrypt encrypt+signed
#define SEC_AUTOCRYPT_OVERRIDE  (1 << 10) ///< (Autocrypt) Indicates manual set/unset of encryption

#define APPLICATION_PGP         (1 << 11) ///< Use PGP to encrypt/sign
#define APPLICATION_SMIME       (1 << 12) ///< Use SMIME to encrypt/sign
#define PGP_TRADITIONAL_CHECKED (1 << 13) ///< Email has a traditional (inline) signature

#define SEC_ALL_FLAGS          ((1 << 14) - 1)

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
void          crypt_extract_keys_from_messages         (struct Mailbox *m, struct EmailArray *ea);
void          crypt_forget_passphrase                  (void);
int           crypt_get_keys                           (struct Email *e, char **keylist, bool oppenc_mode);
void          crypt_opportunistic_encrypt              (struct Email *e);
SecurityFlags crypt_query                              (struct Body *b);
bool          crypt_valid_passphrase                   (SecurityFlags flags);
SecurityFlags mutt_is_application_pgp                  (const struct Body *b);
SecurityFlags mutt_is_application_smime                (struct Body *b);
SecurityFlags mutt_is_malformed_multipart_pgp_encrypted(struct Body *b);
SecurityFlags mutt_is_multipart_encrypted              (struct Body *b);
SecurityFlags mutt_is_multipart_signed                 (struct Body *b);
int           mutt_is_valid_multipart_pgp_encrypted    (struct Body *b);
int           mutt_protected_headers_handler           (struct Body *b, struct State *state);
int           mutt_protect                             (struct Email *e, char *keylist, bool postpone);
bool          mutt_should_hide_protected_subject       (struct Email *e);
int           mutt_signed_handler                      (struct Body *b, struct State *state);

/* cryptglue.c */
void          crypt_cleanup                            (void);
bool          crypt_has_module_backend                 (SecurityFlags type);
void          crypt_init                               (void);
void          crypt_invoke_message                     (SecurityFlags type);
int           crypt_pgp_application_handler            (struct Body *b_email, struct State *state);
bool          crypt_pgp_check_traditional              (FILE *fp, struct Body *b, bool just_one);
int           crypt_pgp_decrypt_mime                   (FILE *fp_in, FILE **fp_out, struct Body *b, struct Body **b_dec);
int           crypt_pgp_encrypted_handler              (struct Body *b_email, struct State *state);
void          crypt_pgp_extract_key_from_attachment    (FILE *fp, struct Body *b);
void          crypt_pgp_invoke_getkeys                 (struct Address *addr);
struct Body * crypt_pgp_make_key_attachment            (void);
SecurityFlags crypt_pgp_send_menu                      (struct Email *e);
int           crypt_smime_application_handler          (struct Body *b_email, struct State *state);
int           crypt_smime_decrypt_mime                 (FILE *fp_in, FILE **fp_out, struct Body *b, struct Body **b_dec);
void          crypt_smime_getkeys                      (struct Envelope *env);
SecurityFlags crypt_smime_send_menu                    (struct Email *e);
int           crypt_smime_verify_sender                (struct Email *e, struct Message *msg);

/* crypt_mod.c */
void          crypto_module_cleanup                    (void);

#ifdef CRYPT_BACKEND_GPGME
/* crypt_gpgme.c */
void          pgp_gpgme_init                           (void);
#ifdef USE_AUTOCRYPT
int          mutt_gpgme_select_secret_key              (struct Buffer *keyid);
#endif
const char  * mutt_gpgme_print_version                 (void);
#endif

#endif /* MUTT_NCRYPT_LIB_H */
