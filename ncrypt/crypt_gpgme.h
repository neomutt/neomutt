/**
 * @file
 * Wrapper for PGP/SMIME calls to GPGME
 *
 * @authors
 * Copyright (C) 2004 g10 Code GmbH
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

#ifndef MUTT_NCRYPT_CRYPT_GPGME_H
#define MUTT_NCRYPT_CRYPT_GPGME_H

#include <gpgme.h>
#include <stdbool.h>
#include <stdio.h>
#include "lib.h"

struct AddressList;
struct Body;
struct Email;
struct Mailbox;
struct State;

/* We work based on user IDs, getting from a user ID to the key is
 * check and does not need any memory (GPGME uses reference counting). */
/**
 * struct CryptKeyInfo - A stored PGP key
 */
struct CryptKeyInfo
{
  struct CryptKeyInfo *next;
  gpgme_key_t kobj;
  int idx;                   ///< and the user ID at this index
  const char *uid;           ///< and for convenience point to this user ID
  KeyFlags flags;            ///< global and per uid flags (for convenience)
  gpgme_validity_t validity; ///< uid validity (cached for convenience)
};

/**
 * enum KeyInfo - PGP Key info
 */
enum KeyInfo
{
  KIP_NAME = 0,    ///< PGP Key field: Name
  KIP_AKA,         ///< PGP Key field: aka (Also Known As)
  KIP_VALID_FROM,  ///< PGP Key field: Valid From date
  KIP_VALID_TO,    ///< PGP Key field: Valid To date
  KIP_KEY_TYPE,    ///< PGP Key field: Key Type
  KIP_KEY_USAGE,   ///< PGP Key field: Key Usage
  KIP_FINGERPRINT, ///< PGP Key field: Fingerprint
  KIP_SERIAL_NO,   ///< PGP Key field: Serial number
  KIP_ISSUED_BY,   ///< PGP Key field: Issued By
  KIP_SUBKEY,      ///< PGP Key field: Subkey
  KIP_MAX,
};

/**
 * enum KeyCap - PGP/SMIME Key Capabilities
 */
enum KeyCap
{
  KEY_CAP_CAN_ENCRYPT, ///< Key can be used for encryption
  KEY_CAP_CAN_SIGN,    ///< Key can be used for signing
  KEY_CAP_CAN_CERTIFY, ///< Key can be used to certify
};

void         pgp_gpgme_set_sender(const char *sender);

int          pgp_gpgme_application_handler(struct Body *m, struct State *s);
int          pgp_gpgme_check_traditional(FILE *fp, struct Body *b, bool just_one);
int          pgp_gpgme_decrypt_mime(FILE *fp_in, FILE **fp_out, struct Body *b, struct Body **cur);
int          pgp_gpgme_encrypted_handler(struct Body *a, struct State *s);
struct Body *pgp_gpgme_encrypt_message(struct Body *a, char *keylist, bool sign, const struct AddressList *from);
char *       pgp_gpgme_find_keys(struct AddressList *addrlist, bool oppenc_mode);
void         pgp_gpgme_invoke_import(const char *fname);
struct Body *pgp_gpgme_make_key_attachment(void);
int          pgp_gpgme_send_menu(struct Email *e);
struct Body *pgp_gpgme_sign_message(struct Body *a, const struct AddressList *from);
int          pgp_gpgme_verify_one(struct Body *sigbdy, struct State *s, const char *tempfile);

int          smime_gpgme_application_handler(struct Body *a, struct State *s);
struct Body *smime_gpgme_build_smime_entity(struct Body *a, char *keylist);
int          smime_gpgme_decrypt_mime(FILE *fp_in, FILE **fp_out, struct Body *b, struct Body **cur);
char *       smime_gpgme_find_keys(struct AddressList *addrlist, bool oppenc_mode);
void         smime_gpgme_init(void);
int          smime_gpgme_send_menu(struct Email *e);
struct Body *smime_gpgme_sign_message(struct Body *a, const struct AddressList *from);
int          smime_gpgme_verify_one(struct Body *sigbdy, struct State *s, const char *tempfile);
int          smime_gpgme_verify_sender(struct Mailbox *m, struct Email *e);

bool crypt_id_is_strong(struct CryptKeyInfo *key);
int digit(const char *s);
const char *crypt_fpr_or_lkeyid(struct CryptKeyInfo *k);
unsigned int key_check_cap(gpgme_key_t key, enum KeyCap cap);
gpgme_ctx_t create_gpgme_context(bool for_smime);
const char *crypt_keyid(struct CryptKeyInfo *k);
int crypt_id_is_valid(struct CryptKeyInfo *key);
struct CryptKeyInfo *crypt_copy_key(struct CryptKeyInfo *key);

#endif /* MUTT_NCRYPT_CRYPT_GPGME_H */
