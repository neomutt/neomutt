/**
 * @file
 * Register crypto modules
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_NCRYPT_CRYPT_MOD_H
#define MUTT_NCRYPT_CRYPT_MOD_H

#include <stdbool.h>
#include <stdio.h>
#include "lib.h"

struct Address;
struct AddressList;
struct Body;
struct Email;
struct Envelope;
struct Message;
struct State;

/**
 * @defgroup crypto_api Crypto API
 *
 * The Crypto API
 *
 * A structure to describe a crypto module.
 */
struct CryptModuleSpecs
{
  int identifier; ///< Identifying bit

  /**
   * @defgroup crypto_init init()
   * @ingroup crypto_api
   *
   * init - Initialise the crypto module
   */
  void (*init)(void);

  /**
   * @defgroup crypto_cleanup cleanup()
   * @ingroup crypto_api
   *
   * cleanup - Clean up the crypt module
   */
  void(*cleanup)(void);

  /**
   * @defgroup crypto_void_passphrase void_passphrase()
   * @ingroup crypto_api
   *
   * void_passphrase - Forget the cached passphrase
   */
  void (*void_passphrase)(void);

  /**
   * @defgroup crypto_valid_passphrase valid_passphrase()
   * @ingroup crypto_api
   *
   * valid_passphrase - Ensure we have a valid passphrase
   * @retval true  Success
   * @retval false Failed
   *
   * If the passphrase is within the expiry time (backend-specific), use it.
   * If not prompt the user again.
   */
  bool (*valid_passphrase)(void);

  /**
   * @defgroup crypto_decrypt_mime decrypt_mime()
   * @ingroup crypto_api
   *
   * decrypt_mime - Decrypt an encrypted MIME part
   * @param[in]  fp_in  File containing the encrypted part
   * @param[out] fp_out File containing the decrypted part
   * @param[in]  b      Body of the email
   * @param[out] cur    Body containing the decrypted part
   * @retval  0 Success
   * @retval -1 Failure
   */
  int (*decrypt_mime)(FILE *fp_in, FILE **fp_out, struct Body *b, struct Body **cur);

  /**
   * @defgroup crypto_application_handler application_handler()
   * @ingroup crypto_api
   *
   * application_handler - Manage the MIME type "application/pgp" or "application/smime"
   * @param m     Body of the email
   * @param state State of text being processed
   * @retval 0 Success
   * @retval -1 Error
   */
  int (*application_handler)(struct Body *m, struct State *state);

  /**
   * @defgroup crypto_encrypted_handler encrypted_handler()
   * @ingroup crypto_api
   *
   * encrypted_handler - Manage a PGP or S/MIME encrypted MIME part
   * @param m     Body of the email
   * @param state State of text being processed
   * @retval 0 Success
   * @retval -1 Error
   */
  int (*encrypted_handler)(struct Body *m, struct State *state);

  /**
   * @defgroup crypto_find_keys find_keys()
   * @ingroup crypto_api
   *
   * find_keys - Find the keyids of the recipients of a message
   * @param addrlist    Address List
   * @param oppenc_mode If true, use opportunistic encryption
   * @retval ptr  Space-separated string of keys
   * @retval NULL At least one of the keys can't be found
   *
   * If oppenc_mode is true, only keys that can be determined without prompting
   * will be used.
   */
  char *(*find_keys)(const struct AddressList *addrlist, bool oppenc_mode);

  /**
   * @defgroup crypto_sign_message sign_message()
   * @ingroup crypto_api
   *
   * sign_message - Cryptographically sign the Body of a message
   * @param a Body of the message
   * @param from From line
   * @retval ptr  New encrypted Body
   * @retval NULL Error
   */
  struct Body *(*sign_message)(struct Body *a, const struct AddressList *from);

  /**
   * @defgroup crypto_verify_one verify_one()
   * @ingroup crypto_api
   *
   * verify_one - Check a signed MIME part against a signature
   * @param sigbdy Body of the signed mail
   * @param state  State of text being processed
   * @param tempf  File containing the key
   * @retval  0 Success
   * @retval -1 Error
   */
  int (*verify_one)(struct Body *sigbdy, struct State *state, const char *tempf);

  /**
   * @defgroup crypto_send_menu send_menu()
   * @ingroup crypto_api
   *
   * send_menu - Ask the user whether to sign and/or encrypt the email
   * @param e Email
   * @retval num Flags, e.g. #APPLICATION_PGP | #SEC_ENCRYPT
   */
  SecurityFlags (*send_menu)(struct Email *e);

  /**
   * @defgroup crypto_set_sender set_sender()
   * @ingroup crypto_api
   *
   * set_sender - Set the sender of the email
   * @param sender Email address
   */
  void (*set_sender)(const char *sender);

  /**
   * @defgroup crypto_pgp_encrypt_message pgp_encrypt_message()
   * @ingroup crypto_api
   *
   * pgp_encrypt_message - PGP encrypt an email
   * @param a       Body of email to encrypt
   * @param keylist List of keys, or fingerprints (space separated)
   * @param sign    If true, sign the message too
   * @param from    From line, to choose the key to sign
   * @retval ptr  Encrypted Body
   * @retval NULL Error
   *
   * Encrypt the mail body to all the given keys.
   */
  struct Body *(*pgp_encrypt_message)(struct Body *a, char *keylist, bool sign, const struct AddressList *from);

  /**
   * @defgroup crypto_pgp_make_key_attachment pgp_make_key_attachment()
   * @ingroup crypto_api
   *
   * pgp_make_key_attachment - Generate a public key attachment
   * @retval ptr  New Body containing the attachment
   * @retval NULL Error
   */
  struct Body *(*pgp_make_key_attachment)(void);

  /**
   * @defgroup crypto_pgp_check_traditional pgp_check_traditional()
   * @ingroup crypto_api
   *
   * pgp_check_traditional - Look for inline (non-MIME) PGP content
   * @param fp       File pointer to the current attachment
   * @param b        Body of email to check
   * @param just_one If true, just check one email part
   * @retval true  It's an inline PGP email
   * @retval false It's not inline, or an error
   */
  bool (*pgp_check_traditional)(FILE *fp, struct Body *b, bool just_one);

  /**
   * @defgroup crypto_pgp_traditional_encryptsign pgp_traditional_encryptsign()
   * @ingroup crypto_api
   *
   * pgp_traditional_encryptsign - Create an inline PGP encrypted, signed email
   * @param a       Body of the email
   * @param flags   Flags, see #SecurityFlags
   * @param keylist List of keys to encrypt to (space-separated)
   * @retval ptr  New encrypted/siged Body
   * @retval NULL Error
   */
  struct Body *(*pgp_traditional_encryptsign)(struct Body *a, SecurityFlags flags, char *keylist);

  /**
   * @defgroup crypto_pgp_invoke_getkeys pgp_invoke_getkeys()
   * @ingroup crypto_api
   *
   * pgp_invoke_getkeys - Run a command to download a PGP key
   * @param addr Address to search for
   */
  void (*pgp_invoke_getkeys)(struct Address *addr);

  /**
   * @defgroup crypto_pgp_invoke_import pgp_invoke_import()
   * @ingroup crypto_api
   *
   * pgp_invoke_import - Import a key from a message into the user's public key ring
   * @param fname File containing the message
   */
  void (*pgp_invoke_import)(const char *fname);

  /**
   * @defgroup crypto_pgp_extract_key_from_attachment pgp_extract_key_from_attachment()
   * @ingroup crypto_api
   *
   * pgp_extract_key_from_attachment - Extract PGP key from an attachment
   * @param fp  File containing email
   * @param top Body of the email
   */
  void (*pgp_extract_key_from_attachment)(FILE *fp, struct Body *top);

  /**
   * @defgroup crypto_smime_getkeys smime_getkeys()
   * @ingroup crypto_api
   *
   * smime_getkeys - Get the S/MIME keys required to encrypt this email
   * @param env Envelope of the email
   */
  void (*smime_getkeys)(struct Envelope *env);

  /**
   * @defgroup crypto_smime_verify_sender smime_verify_sender()
   * @ingroup crypto_api
   *
   * smime_verify_sender - Does the sender match the certificate?
   * @param e Email
   * @param msg Message
   * @retval 0 Success
   * @retval 1 Failure
   */
  int (*smime_verify_sender)(struct Email *e, struct Message *msg);

  /**
   * @defgroup crypto_smime_build_smime_entity smime_build_smime_entity()
   * @ingroup crypto_api
   *
   * smime_build_smime_entity - Encrypt the email body to all recipients
   * @param a        Body of email
   * @param certlist List of key fingerprints (space separated)
   * @retval ptr  New S/MIME encrypted Body
   * @retval NULL Error
   */
  struct Body *(*smime_build_smime_entity)(struct Body *a, char *certlist);

  /**
   * @defgroup crypto_smime_invoke_import smime_invoke_import()
   * @ingroup crypto_api
   *
   * smime_invoke_import - Add a certificate and update index file (externally)
   * @param infile  File containing certificate
   * @param mailbox Mailbox
   */
  void (*smime_invoke_import)(const char *infile, const char *mailbox);
};

/* High Level crypto module interface */
void crypto_module_register(const struct CryptModuleSpecs *specs);
const struct CryptModuleSpecs *crypto_module_lookup(int identifier);

#endif /* MUTT_NCRYPT_CRYPT_MOD_H */
