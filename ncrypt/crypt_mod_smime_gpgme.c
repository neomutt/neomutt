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

#include "config.h"
#include <stdio.h>
#include "crypt_gpgme.h"
#include "crypt_mod.h"
#include "ncrypt.h"

struct Address;
struct Body;
struct Header;
struct State;

static void crypt_mod_smime_init(void)
{
  smime_gpgme_init();
}

static void crypt_mod_smime_void_passphrase(void)
{
  /* Handled by gpg-agent.  */
}

static int crypt_mod_smime_valid_passphrase(void)
{
  /* Handled by gpg-agent.  */
  return 1;
}

static int crypt_mod_smime_decrypt_mime(FILE *a, FILE **b, struct Body *c, struct Body **d)
{
  return smime_gpgme_decrypt_mime(a, b, c, d);
}

static int crypt_mod_smime_application_handler(struct Body *m, struct State *s)
{
  return smime_gpgme_application_handler(m, s);
}

static char *crypt_mod_smime_findkeys(struct Address *addrlist, int oppenc_mode)
{
  return smime_gpgme_findkeys(addrlist, oppenc_mode);
}

static struct Body *crypt_mod_smime_sign_message(struct Body *a)
{
  return smime_gpgme_sign_message(a);
}

static int crypt_mod_smime_verify_one(struct Body *sigbdy, struct State *s, const char *tempf)
{
  return smime_gpgme_verify_one(sigbdy, s, tempf);
}

static int crypt_mod_smime_send_menu(struct Header *msg)
{
  return smime_gpgme_send_menu(msg);
}

static struct Body *crypt_mod_smime_build_smime_entity(struct Body *a, char *certlist)
{
  return smime_gpgme_build_smime_entity(a, certlist);
}

static int crypt_mod_smime_verify_sender(struct Header *h)
{
  return smime_gpgme_verify_sender(h);
}

struct CryptModuleSpecs crypt_mod_smime_gpgme = {
  APPLICATION_SMIME,
  {
      crypt_mod_smime_init,
      crypt_mod_smime_void_passphrase,
      crypt_mod_smime_valid_passphrase,
      crypt_mod_smime_decrypt_mime,
      crypt_mod_smime_application_handler,
      NULL, /* encrypted_handler */
      crypt_mod_smime_findkeys,
      crypt_mod_smime_sign_message,
      crypt_mod_smime_verify_one,
      crypt_mod_smime_send_menu,
      NULL,

      NULL, /* pgp_encrypt_message */
      NULL, /* pgp_make_key_attachment */
      NULL, /* pgp_check_traditional */
      NULL, /* pgp_traditional_encryptsign */
      NULL, /* pgp_invoke_getkeys */
      NULL, /* pgp_invoke_import */
      NULL, /* pgp_extract_keys_from_attachment_list */

      NULL, /* smime_getkeys */
      crypt_mod_smime_verify_sender,
      crypt_mod_smime_build_smime_entity,
      NULL, /* smime_invoke_import */
  },
};
