/**
 * @file
 * Wrapper around crypto functions
 *
 * @authors
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
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
 * @page ncrypt_cryptglue Wrapper around crypto functions
 *
 * This file dispatches the generic crypto functions to the implemented
 * backend or provides dummy stubs.
 *
 * @note Some generic functions are handled in crypt.c
 *
 * @note This file has been changed to make use of the new module system.
 * Consequently there's a 1:1 mapping between the functions contained in this
 * file and the functions implemented by the crypto modules.
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "cryptglue.h"
#include "lib.h"
#include "crypt_mod.h"
#include "module_data.h"
#ifndef CRYPT_BACKEND_GPGME
#include "gui/lib.h"
#endif
#if defined(CRYPT_BACKEND_GPGME) || defined(USE_AUTOCRYPT)
#include "config/lib.h"
#endif
#ifdef CRYPT_BACKEND_GPGME
#include "crypt_gpgme.h"
#endif
#ifdef CRYPT_BACKEND_CLASSIC_PGP
#include "pgpkey.h"
#endif
#ifdef USE_AUTOCRYPT
#include "email/lib.h"
#include "autocrypt/lib.h"
#include "globals.h"
#else
struct Envelope;
#endif

struct Address;
struct AddressList;

#ifdef CRYPT_BACKEND_CLASSIC_PGP
extern const struct CryptModuleSpecs CryptModPgpClassic;
#endif

#ifdef CRYPT_BACKEND_CLASSIC_SMIME
extern const struct CryptModuleSpecs CryptModSmimeClassic;
#endif

#ifdef CRYPT_BACKEND_GPGME
extern const struct CryptModuleSpecs CryptModPgpGpgme;
extern const struct CryptModuleSpecs CryptModSmimeGpgme;
#endif

/* If the crypto module identifier by IDENTIFIER has been registered,
 * call its function FUNC.  Do nothing else.  This may be used as an
 * expression. */
#define CRYPT_MOD_CALL_CHECK(mod_data, identifier, func)                       \
  (crypto_module_lookup(mod_data, APPLICATION_##identifier) &&                 \
   (crypto_module_lookup(mod_data, APPLICATION_##identifier))->func)

/* Call the function FUNC in the crypto module identified by
 * IDENTIFIER. This may be used as an expression. */
#define CRYPT_MOD_CALL(mod_data, identifier, func)                             \
  (*(crypto_module_lookup(mod_data, APPLICATION_##identifier))->func)

/**
 * crypt_init - Initialise the crypto backends
 *
 * This calls CryptModuleSpecs::init()
 */
void crypt_init(void)
{
#ifdef CRYPT_BACKEND_GPGME
  const bool c_crypt_use_gpgme = cs_subset_bool(NeoMutt->sub, "crypt_use_gpgme");
#endif
#ifdef CRYPT_BACKEND_CLASSIC_PGP
  if (
#ifdef CRYPT_BACKEND_GPGME
      (!c_crypt_use_gpgme)
#else
      1
#endif
  )
    crypto_module_register(&CryptModPgpClassic);
#endif

#ifdef CRYPT_BACKEND_CLASSIC_SMIME
  if (
#ifdef CRYPT_BACKEND_GPGME
      (!c_crypt_use_gpgme)
#else
      1
#endif
  )
    crypto_module_register(&CryptModSmimeClassic);
#endif

#ifdef CRYPT_BACKEND_GPGME
  if (c_crypt_use_gpgme)
  {
    crypto_module_register(&CryptModPgpGpgme);
    crypto_module_register(&CryptModSmimeGpgme);
  }
#endif

#if defined(CRYPT_BACKEND_CLASSIC_PGP) ||                                      \
    defined(CRYPT_BACKEND_CLASSIC_SMIME) || defined(CRYPT_BACKEND_GPGME)
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, PGP, init))
    CRYPT_MOD_CALL(mod_data, PGP, init)();

  if (CRYPT_MOD_CALL_CHECK(mod_data, SMIME, init))
    CRYPT_MOD_CALL(mod_data, SMIME, init)();
#endif
}

/**
 * crypt_cleanup - Clean up backend
 * @param mod_data Ncrypt module data
 */
void crypt_cleanup(struct NcryptModuleData *mod_data)
{
  if (CRYPT_MOD_CALL_CHECK(mod_data, PGP, cleanup))
    (CRYPT_MOD_CALL(mod_data, PGP, cleanup))(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, SMIME, cleanup))
    (CRYPT_MOD_CALL(mod_data, SMIME, cleanup))(mod_data);

#ifdef CRYPT_BACKEND_CLASSIC_PGP
  pgp_id_defaults_cleanup(&mod_data->pgp_id_defaults);
#endif

#ifdef CRYPT_BACKEND_GPGME
  gpgme_id_defaults_cleanup(mod_data);
#endif
}

/**
 * crypt_invoke_message - Display an informative message
 * @param type Crypto type, see #SecurityFlags
 *
 * Show a message that a backend will be invoked.
 */
void crypt_invoke_message(SecurityFlags type)
{
  if (((WithCrypto & APPLICATION_PGP) != 0) && (type & APPLICATION_PGP))
    mutt_message(_("Invoking PGP..."));
  else if (((WithCrypto & APPLICATION_SMIME) != 0) && (type & APPLICATION_SMIME))
    mutt_message(_("Invoking S/MIME..."));
}

/**
 * crypt_has_module_backend - Is there a crypto backend for a given type?
 * @param type Crypto type, see #SecurityFlags
 * @retval true  Backend is present
 * @retval false Backend is not present
 */
bool crypt_has_module_backend(SecurityFlags type)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (((WithCrypto & APPLICATION_PGP) != 0) && (type & APPLICATION_PGP) &&
      crypto_module_lookup(mod_data, APPLICATION_PGP))
  {
    return true;
  }

  if (((WithCrypto & APPLICATION_SMIME) != 0) && (type & APPLICATION_SMIME) &&
      crypto_module_lookup(mod_data, APPLICATION_SMIME))
  {
    return true;
  }

  return false;
}

/**
 * crypt_pgp_void_passphrase - Wrapper for CryptModuleSpecs::void_passphrase()
 */
void crypt_pgp_void_passphrase(void)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, PGP, void_passphrase))
    CRYPT_MOD_CALL(mod_data, PGP, void_passphrase)();
}

/**
 * crypt_pgp_valid_passphrase - Wrapper for CryptModuleSpecs::valid_passphrase()
 */
bool crypt_pgp_valid_passphrase(void)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, PGP, valid_passphrase))
    return CRYPT_MOD_CALL(mod_data, PGP, valid_passphrase)();

  return false;
}

/**
 * crypt_pgp_decrypt_mime - Wrapper for CryptModuleSpecs::decrypt_mime()
 */
int crypt_pgp_decrypt_mime(FILE *fp_in, FILE **fp_out, struct Body *b, struct Body **b_dec)
{
#ifdef USE_AUTOCRYPT
  const bool c_autocrypt = cs_subset_bool(NeoMutt->sub, "autocrypt");
  if (c_autocrypt)
  {
    OptAutocryptGpgme = true;
    int result = pgp_gpgme_decrypt_mime(fp_in, fp_out, b, b_dec);
    OptAutocryptGpgme = false;
    if (result == 0)
    {
      b->is_autocrypt = true;
      return result;
    }
  }
#endif

  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, PGP, decrypt_mime))
    return CRYPT_MOD_CALL(mod_data, PGP, decrypt_mime)(fp_in, fp_out, b, b_dec);

  return -1;
}

/**
 * crypt_pgp_application_handler - Wrapper for CryptModuleSpecs::application_handler() - Implements ::handler_t - @ingroup handler_api
 */
int crypt_pgp_application_handler(struct Body *b_email, struct State *state)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, PGP, application_handler))
    return CRYPT_MOD_CALL(mod_data, PGP, application_handler)(b_email, state);

  return -1;
}

/**
 * crypt_pgp_encrypted_handler - Wrapper for CryptModuleSpecs::encrypted_handler() - Implements ::handler_t - @ingroup handler_api
 */
int crypt_pgp_encrypted_handler(struct Body *b_email, struct State *state)
{
#ifdef USE_AUTOCRYPT
  const bool c_autocrypt = cs_subset_bool(NeoMutt->sub, "autocrypt");
  if (c_autocrypt)
  {
    OptAutocryptGpgme = true;
    int result = pgp_gpgme_encrypted_handler(b_email, state);
    OptAutocryptGpgme = false;
    if (result == 0)
    {
      b_email->is_autocrypt = true;
      return result;
    }
  }
#endif

  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, PGP, encrypted_handler))
    return CRYPT_MOD_CALL(mod_data, PGP, encrypted_handler)(b_email, state);

  return -1;
}

/**
 * crypt_pgp_invoke_getkeys - Wrapper for CryptModuleSpecs::pgp_invoke_getkeys()
 */
void crypt_pgp_invoke_getkeys(struct Address *addr)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, PGP, pgp_invoke_getkeys))
    CRYPT_MOD_CALL(mod_data, PGP, pgp_invoke_getkeys)(addr);
}

/**
 * crypt_pgp_check_traditional - Wrapper for CryptModuleSpecs::pgp_check_traditional()
 */
bool crypt_pgp_check_traditional(FILE *fp, struct Body *b, bool just_one)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, PGP, pgp_check_traditional))
    return CRYPT_MOD_CALL(mod_data, PGP, pgp_check_traditional)(fp, b, just_one);

  return false;
}

/**
 * crypt_pgp_traditional_encryptsign - Wrapper for CryptModuleSpecs::pgp_traditional_encryptsign()
 */
struct Body *crypt_pgp_traditional_encryptsign(struct Body *b, SecurityFlags flags, char *keylist)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, PGP, pgp_traditional_encryptsign))
    return CRYPT_MOD_CALL(mod_data, PGP, pgp_traditional_encryptsign)(b, flags, keylist);

  return NULL;
}

/**
 * crypt_pgp_make_key_attachment - Wrapper for CryptModuleSpecs::pgp_make_key_attachment()
 */
struct Body *crypt_pgp_make_key_attachment(void)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, PGP, pgp_make_key_attachment))
    return CRYPT_MOD_CALL(mod_data, PGP, pgp_make_key_attachment)();

  return NULL;
}

/**
 * crypt_pgp_find_keys - Wrapper for CryptModuleSpecs::find_keys()
 */
char *crypt_pgp_find_keys(struct AddressList *addrlist, bool oppenc_mode)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, PGP, find_keys))
    return CRYPT_MOD_CALL(mod_data, PGP, find_keys)(addrlist, oppenc_mode);

  return NULL;
}

/**
 * crypt_pgp_sign_message - Wrapper for CryptModuleSpecs::sign_message()
 */
struct Body *crypt_pgp_sign_message(struct Body *b, const struct AddressList *from)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, PGP, sign_message))
    return CRYPT_MOD_CALL(mod_data, PGP, sign_message)(b, from);

  return NULL;
}

/**
 * crypt_pgp_encrypt_message - Wrapper for CryptModuleSpecs::pgp_encrypt_message()
 */
struct Body *crypt_pgp_encrypt_message(struct Email *e, struct Body *b, char *keylist,
                                       int sign, const struct AddressList *from)
{
#ifdef USE_AUTOCRYPT
  if (e->security & SEC_AUTOCRYPT)
  {
    if (mutt_autocrypt_set_sign_as_default_key(e))
      return NULL;

    OptAutocryptGpgme = true;
    struct Body *result = pgp_gpgme_encrypt_message(b, keylist, sign, from);
    OptAutocryptGpgme = false;

    return result;
  }
#endif

  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, PGP, pgp_encrypt_message))
    return CRYPT_MOD_CALL(mod_data, PGP, pgp_encrypt_message)(b, keylist, sign, from);

  return NULL;
}

/**
 * crypt_pgp_invoke_import - Wrapper for CryptModuleSpecs::pgp_invoke_import()
 */
void crypt_pgp_invoke_import(const char *fname)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, PGP, pgp_invoke_import))
    CRYPT_MOD_CALL(mod_data, PGP, pgp_invoke_import)(fname);
}

/**
 * crypt_pgp_verify_one - Wrapper for CryptModuleSpecs::verify_one()
 */
int crypt_pgp_verify_one(struct Body *b, struct State *state, const char *tempf)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, PGP, verify_one))
    return CRYPT_MOD_CALL(mod_data, PGP, verify_one)(b, state, tempf);

  return -1;
}

/**
 * crypt_pgp_send_menu - Wrapper for CryptModuleSpecs::send_menu()
 */
SecurityFlags crypt_pgp_send_menu(struct Email *e)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, PGP, send_menu))
    return CRYPT_MOD_CALL(mod_data, PGP, send_menu)(e);

  return 0;
}

/**
 * crypt_pgp_extract_key_from_attachment - Wrapper for CryptModuleSpecs::pgp_extract_key_from_attachment()
 */
void crypt_pgp_extract_key_from_attachment(FILE *fp, struct Body *b)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, PGP, pgp_extract_key_from_attachment))
    CRYPT_MOD_CALL(mod_data, PGP, pgp_extract_key_from_attachment)(fp, b);
}

/**
 * crypt_pgp_set_sender - Wrapper for CryptModuleSpecs::set_sender()
 */
void crypt_pgp_set_sender(const char *sender)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, PGP, set_sender))
    CRYPT_MOD_CALL(mod_data, PGP, set_sender)(sender);
}

/**
 * crypt_smime_void_passphrase - Wrapper for CryptModuleSpecs::void_passphrase()
 */
void crypt_smime_void_passphrase(void)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, SMIME, void_passphrase))
    CRYPT_MOD_CALL(mod_data, SMIME, void_passphrase)();
}

/**
 * crypt_smime_valid_passphrase - Wrapper for CryptModuleSpecs::valid_passphrase()
 */
bool crypt_smime_valid_passphrase(void)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, SMIME, valid_passphrase))
    return CRYPT_MOD_CALL(mod_data, SMIME, valid_passphrase)();

  return false;
}

/**
 * crypt_smime_decrypt_mime - Wrapper for CryptModuleSpecs::decrypt_mime()
 */
int crypt_smime_decrypt_mime(FILE *fp_in, FILE **fp_out, struct Body *b, struct Body **b_dec)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, SMIME, decrypt_mime))
    return CRYPT_MOD_CALL(mod_data, SMIME, decrypt_mime)(fp_in, fp_out, b, b_dec);

  return -1;
}

/**
 * crypt_smime_application_handler - Wrapper for CryptModuleSpecs::application_handler() - Implements ::handler_t - @ingroup handler_api
 */
int crypt_smime_application_handler(struct Body *b_email, struct State *state)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, SMIME, application_handler))
    return CRYPT_MOD_CALL(mod_data, SMIME, application_handler)(b_email, state);

  return -1;
}

/**
 * crypt_smime_getkeys - Wrapper for CryptModuleSpecs::smime_getkeys()
 */
void crypt_smime_getkeys(struct Envelope *env)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, SMIME, smime_getkeys))
    CRYPT_MOD_CALL(mod_data, SMIME, smime_getkeys)(env);
}

/**
 * crypt_smime_verify_sender - Wrapper for CryptModuleSpecs::smime_verify_sender()
 */
int crypt_smime_verify_sender(struct Email *e, struct Message *msg)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, SMIME, smime_verify_sender))
    return CRYPT_MOD_CALL(mod_data, SMIME, smime_verify_sender)(e, msg);

  return 1;
}

/**
 * crypt_smime_find_keys - Wrapper for CryptModuleSpecs::find_keys()
 */
char *crypt_smime_find_keys(struct AddressList *addrlist, bool oppenc_mode)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, SMIME, find_keys))
    return CRYPT_MOD_CALL(mod_data, SMIME, find_keys)(addrlist, oppenc_mode);

  return NULL;
}

/**
 * crypt_smime_sign_message - Wrapper for CryptModuleSpecs::sign_message()
 */
struct Body *crypt_smime_sign_message(struct Body *b, const struct AddressList *from)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, SMIME, sign_message))
    return CRYPT_MOD_CALL(mod_data, SMIME, sign_message)(b, from);

  return NULL;
}

/**
 * crypt_smime_build_smime_entity - Wrapper for CryptModuleSpecs::smime_build_smime_entity()
 */
struct Body *crypt_smime_build_smime_entity(struct Body *b, char *certlist)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, SMIME, smime_build_smime_entity))
    return CRYPT_MOD_CALL(mod_data, SMIME, smime_build_smime_entity)(b, certlist);

  return NULL;
}

/**
 * crypt_smime_invoke_import - Wrapper for CryptModuleSpecs::smime_invoke_import()
 */
void crypt_smime_invoke_import(const char *infile, const char *mailbox)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, SMIME, smime_invoke_import))
    CRYPT_MOD_CALL(mod_data, SMIME, smime_invoke_import)(infile, mailbox);
}

/**
 * crypt_smime_verify_one - Wrapper for CryptModuleSpecs::verify_one()
 */
int crypt_smime_verify_one(struct Body *b, struct State *state, const char *tempf)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, SMIME, verify_one))
    return CRYPT_MOD_CALL(mod_data, SMIME, verify_one)(b, state, tempf);

  return -1;
}

/**
 * crypt_smime_send_menu - Wrapper for CryptModuleSpecs::send_menu()
 */
SecurityFlags crypt_smime_send_menu(struct Email *e)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, SMIME, send_menu))
    return CRYPT_MOD_CALL(mod_data, SMIME, send_menu)(e);

  return 0;
}

/**
 * crypt_smime_set_sender - Wrapper for CryptModuleSpecs::set_sender()
 */
void crypt_smime_set_sender(const char *sender)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  ASSERT(mod_data);

  if (CRYPT_MOD_CALL_CHECK(mod_data, SMIME, set_sender))
    CRYPT_MOD_CALL(mod_data, SMIME, set_sender)(sender);
}
