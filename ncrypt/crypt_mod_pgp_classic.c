/**
 * @file
 * Wrappers for calls to CLI PGP
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
 * @page crypt_crypt_mod_pgp Wrappers for calls to CLI PGP
 *
 * Wrappers for calls to CLI PGP
 */

#include "config.h"
#include <stdio.h>
#include "lib.h"
#include "crypt_mod.h"
#include "pgpinvoke.h"
#include "pgpkey.h"
#ifdef CRYPT_BACKEND_CLASSIC_PGP
#include "pgp.h"
#endif

// clang-format off
/**
 * CryptModPgpClassic - CLI PGP - Implements ::CryptModuleSpecs
 */
struct CryptModuleSpecs CryptModPgpClassic = {
  APPLICATION_PGP,

  NULL, /* init */
  NULL, /* cleanup */
  pgp_class_void_passphrase,
  pgp_class_valid_passphrase,
  pgp_class_decrypt_mime,
  pgp_class_application_handler,
  pgp_class_encrypted_handler,
  pgp_class_find_keys,
  pgp_class_sign_message,
  pgp_class_verify_one,
  pgp_class_send_menu,
  NULL, /* set_sender */

  pgp_class_encrypt_message,
  pgp_class_make_key_attachment,
  pgp_class_check_traditional,
  pgp_class_traditional_encryptsign,
  pgp_class_invoke_getkeys,
  pgp_class_invoke_import,
  pgp_class_extract_key_from_attachment,

  NULL, /* smime_getkeys */
  NULL, /* smime_verify_sender */
  NULL, /* smime_build_smime_entity */
  NULL, /* smime_invoke_import */
};
// clang-format on
