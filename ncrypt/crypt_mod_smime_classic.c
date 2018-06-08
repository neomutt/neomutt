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
#include "crypt_mod.h"
#include "ncrypt.h"
#include "smime.h"

// clang-format off
struct CryptModuleSpecs crypt_mod_smime_classic = {
  APPLICATION_SMIME,

  NULL, /* init */
  smime_void_passphrase,
  smime_valid_passphrase,
  smime_decrypt_mime,
  smime_application_smime_handler,
  NULL, /* encrypted_handler */
  smime_find_keys,
  smime_sign_message,
  smime_verify_one,
  smime_send_menu,
  NULL, /* set_sender */

  NULL, /* pgp_encrypt_message */
  NULL, /* pgp_make_key_attachment */
  NULL, /* pgp_check_traditional */
  NULL, /* pgp_traditional_encryptsign */
  NULL, /* pgp_invoke_getkeys */
  NULL, /* pgp_invoke_import */
  NULL, /* pgp_extract_keys_from_attachment_list */

  smime_getkeys,
  smime_verify_sender,
  smime_build_smime_entity,
  smime_invoke_import,
};
// clang-format on
