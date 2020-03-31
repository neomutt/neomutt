/**
 * @file
 * Wrappers for calls to CLI SMIME
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
 * @page crypt_crypt_mod_smime Wrappers for calls to CLI SMIME
 *
 * Wrappers for calls to CLI SMIME
 */

#include "config.h"
#include <stdio.h>
#include "lib.h"
#include "crypt_mod.h"
#ifdef CRYPT_BACKEND_CLASSIC_SMIME
#include "smime.h"
#endif

/**
 * smime_class_init - Initialise smime
 */
static void smime_class_init(void)
{
  smime_init();
}

/**
 * smime_class_cleanup - Clean up smime
 */
static void smime_class_cleanup(void)
{
  smime_cleanup();
}

// clang-format off
/**
 * CryptModSmimeClassic - CLI SMIME - Implements ::CryptModuleSpecs
 */
struct CryptModuleSpecs CryptModSmimeClassic = {
  APPLICATION_SMIME,

  smime_class_init,
  smime_class_cleanup,
  smime_class_void_passphrase,
  smime_class_valid_passphrase,
  smime_class_decrypt_mime,
  smime_class_application_handler,
  NULL, /* encrypted_handler */
  smime_class_find_keys,
  smime_class_sign_message,
  smime_class_verify_one,
  smime_class_send_menu,
  NULL, /* set_sender */

  NULL, /* pgp_encrypt_message */
  NULL, /* pgp_make_key_attachment */
  NULL, /* pgp_check_traditional */
  NULL, /* pgp_traditional_encryptsign */
  NULL, /* pgp_invoke_getkeys */
  NULL, /* pgp_invoke_import */
  NULL, /* pgp_extract_key_from_attachment */

  smime_class_getkeys,
  smime_class_verify_sender,
  smime_class_build_smime_entity,
  smime_class_invoke_import,
};
// clang-format on
