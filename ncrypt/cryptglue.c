/**
 * @file
 * Wrapper around crypto functions
 *
 * @authors
 * Copyright (C) 2003 Werner Koch <wk@gnupg.org>
 * Copyright (C) 2004 g10 Code GmbH
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

/* This file dispatches the generic crypto functions to the implemented backend
 * or provides dummy stubs.  Note, that some generic functions are handled in
 * crypt.c.
 */

/* Note: This file has been changed to make use of the new module system.
 * Consequently there's a 1:1 mapping between the functions contained in this
 * file and the functions implemented by the crypto modules.
 */

#include "config.h"
#include <stdio.h>
#include "mutt/mutt.h"
#include "crypt_mod.h"
#include "globals.h"
#include "ncrypt.h"
#include "options.h"
#include "protos.h"

struct Address;
struct Body;
struct Envelope;
struct Header;
struct State;

/*

    Generic

*/

#ifdef CRYPT_BACKEND_CLASSIC_PGP
extern struct CryptModuleSpecs crypt_mod_pgp_classic;
#endif

#ifdef CRYPT_BACKEND_CLASSIC_SMIME
extern struct CryptModuleSpecs crypt_mod_smime_classic;
#endif

#ifdef CRYPT_BACKEND_GPGME
extern struct CryptModuleSpecs crypt_mod_pgp_gpgme;
extern struct CryptModuleSpecs crypt_mod_smime_gpgme;
#endif

void crypt_init(void)
{
#ifdef CRYPT_BACKEND_CLASSIC_PGP
  if (
#ifdef CRYPT_BACKEND_GPGME
      (!CryptUseGpgme)
#else
      1
#endif
  )
    crypto_module_register(&crypt_mod_pgp_classic);
#endif

#ifdef CRYPT_BACKEND_CLASSIC_SMIME
  if (
#ifdef CRYPT_BACKEND_GPGME
      (!CryptUseGpgme)
#else
      1
#endif
  )
    crypto_module_register(&crypt_mod_smime_classic);
#endif

  if (CryptUseGpgme)
  {
#ifdef CRYPT_BACKEND_GPGME
    crypto_module_register(&crypt_mod_pgp_gpgme);
    crypto_module_register(&crypt_mod_smime_gpgme);
#else
    mutt_message(_("\"crypt_use_gpgme\" set"
                   " but not built with GPGME support."));
    if (mutt_any_key_to_continue(NULL) == -1)
      mutt_exit(1);
#endif
  }

#if defined(CRYPT_BACKEND_CLASSIC_PGP) ||                                      \
    defined(CRYPT_BACKEND_CLASSIC_SMIME) || defined(CRYPT_BACKEND_GPGME)
  if (CRYPT_MOD_CALL_CHECK(PGP, init))
    (CRYPT_MOD_CALL(PGP, init))();

  if (CRYPT_MOD_CALL_CHECK(SMIME, init))
    (CRYPT_MOD_CALL(SMIME, init))();
#endif
}

/**
 * crypt_invoke_message - Display an informative message
 *
 * Show a message that a backend will be invoked.
 */
void crypt_invoke_message(int type)
{
  if (((WithCrypto & APPLICATION_PGP) != 0) && (type & APPLICATION_PGP))
    mutt_message(_("Invoking PGP..."));
  else if (((WithCrypto & APPLICATION_SMIME) != 0) && (type & APPLICATION_SMIME))
    mutt_message(_("Invoking S/MIME..."));
}

/* Returns 1 if a module backend is registered for the type */
int crypt_has_module_backend(int type)
{
  if (((WithCrypto & APPLICATION_PGP) != 0) && (type & APPLICATION_PGP) &&
      crypto_module_lookup(APPLICATION_PGP))
  {
    return 1;
  }

  if (((WithCrypto & APPLICATION_SMIME) != 0) && (type & APPLICATION_SMIME) &&
      crypto_module_lookup(APPLICATION_SMIME))
  {
    return 1;
  }

  return 0;
}

/*
 * PGP
 */

/**
 * crypt_pgp_void_passphrase - Silently, reset a PGP passphrase
 */
void crypt_pgp_void_passphrase(void)
{
  if (CRYPT_MOD_CALL_CHECK(PGP, void_passphrase))
    (CRYPT_MOD_CALL(PGP, void_passphrase))();
}

int crypt_pgp_valid_passphrase(void)
{
  if (CRYPT_MOD_CALL_CHECK(PGP, valid_passphrase))
    return (CRYPT_MOD_CALL(PGP, valid_passphrase))();

  return 0;
}

/**
 * crypt_pgp_decrypt_mime - Decrypt a PGP/MIME message
 */
int crypt_pgp_decrypt_mime(FILE *a, FILE **b, struct Body *c, struct Body **d)
{
  if (CRYPT_MOD_CALL_CHECK(PGP, decrypt_mime))
    return (CRYPT_MOD_CALL(PGP, decrypt_mime))(a, b, c, d);

  return -1;
}

/**
 * crypt_pgp_application_pgp_handler - MIME handler for the pgp content-type
 */
int crypt_pgp_application_pgp_handler(struct Body *m, struct State *s)
{
  if (CRYPT_MOD_CALL_CHECK(PGP, application_handler))
    return (CRYPT_MOD_CALL(PGP, application_handler))(m, s);

  return -1;
}

/**
 * crypt_pgp_encrypted_handler - MIME handler for an PGP/MIME encrypted message
 */
int crypt_pgp_encrypted_handler(struct Body *a, struct State *s)
{
  if (CRYPT_MOD_CALL_CHECK(PGP, encrypted_handler))
    return (CRYPT_MOD_CALL(PGP, encrypted_handler))(a, s);

  return -1;
}

void crypt_pgp_invoke_getkeys(struct Address *addr)
{
  if (CRYPT_MOD_CALL_CHECK(PGP, pgp_invoke_getkeys))
    (CRYPT_MOD_CALL(PGP, pgp_invoke_getkeys))(addr);
}

/**
 * crypt_pgp_check_traditional - Check for a traditional PGP message in body B
 */
int crypt_pgp_check_traditional(FILE *fp, struct Body *b, int just_one)
{
  if (CRYPT_MOD_CALL_CHECK(PGP, pgp_check_traditional))
    return (CRYPT_MOD_CALL(PGP, pgp_check_traditional))(fp, b, just_one);

  return 0;
}

struct Body *crypt_pgp_traditional_encryptsign(struct Body *a, int flags, char *keylist)
{
  if (CRYPT_MOD_CALL_CHECK(PGP, pgp_traditional_encryptsign))
    return (CRYPT_MOD_CALL(PGP, pgp_traditional_encryptsign))(a, flags, keylist);

  return NULL;
}

/**
 * crypt_pgp_make_key_attachment - Generate a PGP public key attachment
 */
struct Body *crypt_pgp_make_key_attachment(char *tempf)
{
  if (CRYPT_MOD_CALL_CHECK(PGP, pgp_make_key_attachment))
    return (CRYPT_MOD_CALL(PGP, pgp_make_key_attachment))(tempf);

  return NULL;
}

/**
 * crypt_pgp_findkeys - Find the keyids of the recipients of the message
 *
 * It returns NULL if any of the keys can not be found.  If oppenc_mode is
 * true, only keys that can be determined without prompting will be used.
 */
char *crypt_pgp_findkeys(struct Address *addrlist, int oppenc_mode)
{
  if (CRYPT_MOD_CALL_CHECK(PGP, findkeys))
    return (CRYPT_MOD_CALL(PGP, findkeys))(addrlist, oppenc_mode);

  return NULL;
}

/**
 * crypt_pgp_sign_message - Create a new body with a PGP signed message from A
 */
struct Body *crypt_pgp_sign_message(struct Body *a)
{
  if (CRYPT_MOD_CALL_CHECK(PGP, sign_message))
    return (CRYPT_MOD_CALL(PGP, sign_message))(a);

  return NULL;
}

/**
 * crypt_pgp_encrypt_message - Encrypt a message
 *
 * Warning: A is no longer freed in this routine, you need to free it later.
 * This is necessary for $fcc_attach.
 */
struct Body *crypt_pgp_encrypt_message(struct Body *a, char *keylist, int sign)
{
  if (CRYPT_MOD_CALL_CHECK(PGP, pgp_encrypt_message))
    return (CRYPT_MOD_CALL(PGP, pgp_encrypt_message))(a, keylist, sign);

  return NULL;
}

/**
 * crypt_pgp_invoke_import - Invoke the PGP command to import a key
 */
void crypt_pgp_invoke_import(const char *fname)
{
  if (CRYPT_MOD_CALL_CHECK(PGP, pgp_invoke_import))
    (CRYPT_MOD_CALL(PGP, pgp_invoke_import))(fname);
}

int crypt_pgp_verify_one(struct Body *sigbdy, struct State *s, const char *tempf)
{
  if (CRYPT_MOD_CALL_CHECK(PGP, verify_one))
    return (CRYPT_MOD_CALL(PGP, verify_one))(sigbdy, s, tempf);

  return -1;
}

int crypt_pgp_send_menu(struct Header *msg)
{
  if (CRYPT_MOD_CALL_CHECK(PGP, send_menu))
    return (CRYPT_MOD_CALL(PGP, send_menu))(msg);

  return 0;
}

void crypt_pgp_extract_keys_from_attachment_list(FILE *fp, int tag, struct Body *top)
{
  if (CRYPT_MOD_CALL_CHECK(PGP, pgp_extract_keys_from_attachment_list))
    (CRYPT_MOD_CALL(PGP, pgp_extract_keys_from_attachment_list))(fp, tag, top);
}

void crypt_pgp_set_sender(const char *sender)
{
  if (CRYPT_MOD_CALL_CHECK(PGP, set_sender))
    (CRYPT_MOD_CALL(PGP, set_sender))(sender);
}

/*
 * S/MIME
 */

/**
 * crypt_smime_void_passphrase - Silently, reset an SMIME passphrase
 */
void crypt_smime_void_passphrase(void)
{
  if (CRYPT_MOD_CALL_CHECK(SMIME, void_passphrase))
    (CRYPT_MOD_CALL(SMIME, void_passphrase))();
}

int crypt_smime_valid_passphrase(void)
{
  if (CRYPT_MOD_CALL_CHECK(SMIME, valid_passphrase))
    return (CRYPT_MOD_CALL(SMIME, valid_passphrase))();

  return 0;
}

/**
 * crypt_smime_decrypt_mime - Decrypt am S/MIME message
 */
int crypt_smime_decrypt_mime(FILE *a, FILE **b, struct Body *c, struct Body **d)
{
  if (CRYPT_MOD_CALL_CHECK(SMIME, decrypt_mime))
    return (CRYPT_MOD_CALL(SMIME, decrypt_mime))(a, b, c, d);

  return -1;
}

/**
 * crypt_smime_application_smime_handler - Handler for application/smime
 */
int crypt_smime_application_smime_handler(struct Body *m, struct State *s)
{
  if (CRYPT_MOD_CALL_CHECK(SMIME, application_handler))
    return (CRYPT_MOD_CALL(SMIME, application_handler))(m, s);

  return -1;
}

/**
 * crypt_smime_encrypted_handler - Handler for an PGP/MIME encrypted message
 */
void crypt_smime_encrypted_handler(struct Body *a, struct State *s)
{
  if (CRYPT_MOD_CALL_CHECK(SMIME, encrypted_handler))
    (CRYPT_MOD_CALL(SMIME, encrypted_handler))(a, s);
}

void crypt_smime_getkeys(struct Envelope *env)
{
  if (CRYPT_MOD_CALL_CHECK(SMIME, smime_getkeys))
    (CRYPT_MOD_CALL(SMIME, smime_getkeys))(env);
}

/**
 * crypt_smime_verify_sender - Check that the sender matches
 */
int crypt_smime_verify_sender(struct Header *h)
{
  if (CRYPT_MOD_CALL_CHECK(SMIME, smime_verify_sender))
    return (CRYPT_MOD_CALL(SMIME, smime_verify_sender))(h);

  return 1;
}

/**
 * crypt_smime_findkeys - Find the keyids of the recipients of the message
 *
 * It returns NULL if any of the keys can not be found.  If oppenc_mode is
 * true, only keys that can be determined without prompting will be used.
 */
char *crypt_smime_findkeys(struct Address *addrlist, int oppenc_mode)
{
  if (CRYPT_MOD_CALL_CHECK(SMIME, findkeys))
    return (CRYPT_MOD_CALL(SMIME, findkeys))(addrlist, oppenc_mode);

  return NULL;
}

struct Body *crypt_smime_sign_message(struct Body *a)
{
  if (CRYPT_MOD_CALL_CHECK(SMIME, sign_message))
    return (CRYPT_MOD_CALL(SMIME, sign_message))(a);

  return NULL;
}

struct Body *crypt_smime_build_smime_entity(struct Body *a, char *certlist)
{
  if (CRYPT_MOD_CALL_CHECK(SMIME, smime_build_smime_entity))
    return (CRYPT_MOD_CALL(SMIME, smime_build_smime_entity))(a, certlist);

  return NULL;
}

/**
 * crypt_smime_invoke_import - Add a certificate and update index file
 *
 * This is done externally.
 */
void crypt_smime_invoke_import(char *infile, char *mailbox)
{
  if (CRYPT_MOD_CALL_CHECK(SMIME, smime_invoke_import))
    (CRYPT_MOD_CALL(SMIME, smime_invoke_import))(infile, mailbox);
}

int crypt_smime_verify_one(struct Body *sigbdy, struct State *s, const char *tempf)
{
  if (CRYPT_MOD_CALL_CHECK(SMIME, verify_one))
    return (CRYPT_MOD_CALL(SMIME, verify_one))(sigbdy, s, tempf);

  return -1;
}

int crypt_smime_send_menu(struct Header *msg)
{
  if (CRYPT_MOD_CALL_CHECK(SMIME, send_menu))
    return (CRYPT_MOD_CALL(SMIME, send_menu))(msg);

  return 0;
}

void crypt_smime_set_sender(const char *sender)
{
  if (CRYPT_MOD_CALL_CHECK(SMIME, set_sender))
    (CRYPT_MOD_CALL(SMIME, set_sender))(sender);
}
