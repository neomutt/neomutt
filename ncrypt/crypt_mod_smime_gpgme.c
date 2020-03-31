/**
 * @file
 * Wrappers for calls to GPGME SMIME
 *
 * @authors
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

/**
 * @page crypt_crypt_mod_smime_gpgme Wrappers for calls to GPGME SMIME
 *
 * Wrappers for calls to GPGME SMIME
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "lib.h"
#include "crypt_gpgme.h"
#include "crypt_mod.h"

/**
 * smime_gpgme_void_passphrase - Implements CryptModuleSpecs::void_passphrase()
 *
 * This is handled by gpg-agent.
 */
static void smime_gpgme_void_passphrase(void)
{
}

/**
 * smime_gpgme_valid_passphrase - Implements CryptModuleSpecs::valid_passphrase()
 *
 * This is handled by gpg-agent.
 */
static bool smime_gpgme_valid_passphrase(void)
{
  return true;
}

// clang-format off
/**
 * CryptModSmimeGpgme - GPGME SMIME - Implements ::CryptModuleSpecs
 */
struct CryptModuleSpecs CryptModSmimeGpgme = {
  APPLICATION_SMIME,

  smime_gpgme_init,
  NULL, /* cleanup */
  smime_gpgme_void_passphrase,
  smime_gpgme_valid_passphrase,
  smime_gpgme_decrypt_mime,
  smime_gpgme_application_handler,
  NULL, /* encrypted_handler */
  smime_gpgme_find_keys,
  smime_gpgme_sign_message,
  smime_gpgme_verify_one,
  smime_gpgme_send_menu,
  NULL, /* set_sender */

  NULL, /* pgp_encrypt_message */
  NULL, /* pgp_make_key_attachment */
  NULL, /* pgp_check_traditional */
  NULL, /* pgp_traditional_encryptsign */
  NULL, /* pgp_invoke_getkeys */
  NULL, /* pgp_invoke_import */
  NULL, /* pgp_extract_key_from_attachment */

  NULL, /* smime_getkeys */
  smime_gpgme_verify_sender,
  smime_gpgme_build_smime_entity,
  NULL, /* smime_invoke_import */
};
// clang-format on
