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
#include <stdio.h>
#include "crypt_gpgme.h"
#include "crypt_mod.h"
#include "ncrypt.h"

static void crypt_mod_pgp_void_passphrase(void)
{
  /* Handled by gpg-agent.  */
}

static int crypt_mod_pgp_valid_passphrase(void)
{
  /* Handled by gpg-agent.  */
  return 1;
}

static struct Body *crypt_mod_pgp_make_key_attachment(char *tempf)
{
#ifdef HAVE_GPGME_OP_EXPORT_KEYS
  return pgp_gpgme_make_key_attachment(tempf);
#else
  return NULL;
#endif
}

// clang-format off
struct CryptModuleSpecs crypt_mod_pgp_gpgme = {
  APPLICATION_PGP,

  pgp_gpgme_init,
  crypt_mod_pgp_void_passphrase,
  crypt_mod_pgp_valid_passphrase,
  pgp_gpgme_decrypt_mime,
  pgp_gpgme_application_handler,
  pgp_gpgme_encrypted_handler,
  pgp_gpgme_findkeys,
  pgp_gpgme_sign_message,
  pgp_gpgme_verify_one,
  pgp_gpgme_send_menu,
  mutt_gpgme_set_sender,

  pgp_gpgme_encrypt_message,
  crypt_mod_pgp_make_key_attachment,
  pgp_gpgme_check_traditional,
  NULL, /* pgp_traditional_encryptsign  */
  NULL, /* pgp_invoke_getkeys  */
  pgp_gpgme_invoke_import,
  NULL, /* pgp_extract_keys_from_attachment_list  */

  NULL, /* smime_getkeys */
  NULL, /* smime_verify_sender */
  NULL, /* smime_build_smime_entity */
  NULL, /* smime_invoke_import */
};
// clang-format on
