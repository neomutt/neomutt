/**
 * @file
 * Wrappers for calls to GPGME PGP
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
 * @page crypt_crypt_mod_pgp_gpgme Wrappers for calls to GPGME PGP
 *
 * Wrappers for calls to GPGME PGP
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "lib.h"
#include "crypt_gpgme.h"
#include "crypt_mod.h"

/**
 * pgp_gpgme_void_passphrase - Implements CryptModuleSpecs::void_passphrase()
 *
 * This is handled by gpg-agent.
 */
static void pgp_gpgme_void_passphrase(void)
{
}

/**
 * pgp_gpgme_valid_passphrase - Implements CryptModuleSpecs::valid_passphrase()
 *
 * This is handled by gpg-agent.
 */
static bool pgp_gpgme_valid_passphrase(void)
{
  return true;
}

// clang-format off
/**
 * CryptModPgpGpgme - GPGME PGP - Implements ::CryptModuleSpecs
 */
struct CryptModuleSpecs CryptModPgpGpgme = {
  APPLICATION_PGP,

  pgp_gpgme_init,
  NULL, /* cleanup */
  pgp_gpgme_void_passphrase,
  pgp_gpgme_valid_passphrase,
  pgp_gpgme_decrypt_mime,
  pgp_gpgme_application_handler,
  pgp_gpgme_encrypted_handler,
  pgp_gpgme_find_keys,
  pgp_gpgme_sign_message,
  pgp_gpgme_verify_one,
  pgp_gpgme_send_menu,
  pgp_gpgme_set_sender,

  pgp_gpgme_encrypt_message,
  pgp_gpgme_make_key_attachment,
  pgp_gpgme_check_traditional,
  NULL, /* pgp_traditional_encryptsign */
  NULL, /* pgp_invoke_getkeys */
  pgp_gpgme_invoke_import,
  NULL, /* pgp_extract_key_from_attachment */

  NULL, /* smime_getkeys */
  NULL, /* smime_verify_sender */
  NULL, /* smime_build_smime_entity */
  NULL, /* smime_invoke_import */
};
// clang-format on
