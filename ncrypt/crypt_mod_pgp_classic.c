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
#include "crypt_mod.h"
#include "ncrypt.h"
#include "pgp.h"
#include "pgpinvoke.h"
#include "pgpkey.h"

// clang-format off
struct CryptModuleSpecs crypt_mod_pgp_classic = {
  APPLICATION_PGP,

  NULL, /* init */
  pgp_void_passphrase,
  pgp_valid_passphrase,
  pgp_decrypt_mime,
  pgp_application_pgp_handler,
  pgp_encrypted_handler,
  pgp_find_keys,
  pgp_sign_message,
  pgp_verify_one,
  pgp_send_menu,
  NULL, /* set_sender */

  pgp_encrypt_message,
  pgp_make_key_attachment,
  pgp_check_traditional,
  pgp_traditional_encryptsign,
  pgp_invoke_getkeys,
  pgp_invoke_import,
  pgp_extract_keys_from_attachment_list,

  NULL, /* smime_getkeys */
  NULL, /* smime_verify_sender */
  NULL, /* smime_build_smime_entity */
  NULL, /* smime_invoke_import */
};
// clang-format on
