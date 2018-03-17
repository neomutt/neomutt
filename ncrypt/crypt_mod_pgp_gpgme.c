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

#include "config.h"
#include <stdio.h>
#include "crypt_gpgme.h"
#include "crypt_mod.h"
#include "ncrypt.h"

struct Address;
struct Body;
struct Header;
struct State;

static void crypt_mod_pgp_init(void)
{
  pgp_gpgme_init();
}

static void crypt_mod_pgp_void_passphrase(void)
{
  /* Handled by gpg-agent.  */
}

static int crypt_mod_pgp_valid_passphrase(void)
{
  /* Handled by gpg-agent.  */
  return 1;
}

static int crypt_mod_pgp_decrypt_mime(FILE *a, FILE **b, struct Body *c, struct Body **d)
{
  return pgp_gpgme_decrypt_mime(a, b, c, d);
}

static int crypt_mod_pgp_application_handler(struct Body *m, struct State *s)
{
  return pgp_gpgme_application_handler(m, s);
}

static int crypt_mod_pgp_encrypted_handler(struct Body *m, struct State *s)
{
  return pgp_gpgme_encrypted_handler(m, s);
}

static int crypt_mod_pgp_check_traditional(FILE *fp, struct Body *b, int just_one)
{
  return pgp_gpgme_check_traditional(fp, b, just_one);
}

static void crypt_mod_pgp_invoke_import(const char *fname)
{
  pgp_gpgme_invoke_import(fname);
}

static char *crypt_mod_pgp_findkeys(struct Address *addrlist, int oppenc_mode)
{
  return pgp_gpgme_findkeys(addrlist, oppenc_mode);
}

static struct Body *crypt_mod_pgp_sign_message(struct Body *a)
{
  return pgp_gpgme_sign_message(a);
}

static int crypt_mod_pgp_verify_one(struct Body *sigbdy, struct State *s, const char *tempf)
{
  return pgp_gpgme_verify_one(sigbdy, s, tempf);
}

static int crypt_mod_pgp_send_menu(struct Header *msg)
{
  return pgp_gpgme_send_menu(msg);
}

static struct Body *crypt_mod_pgp_encrypt_message(struct Body *a, char *keylist, int sign)
{
  return pgp_gpgme_encrypt_message(a, keylist, sign);
}

#ifdef HAVE_GPGME_OP_EXPORT_KEYS
static struct Body *crypt_mod_pgp_make_key_attachment(char *tempf)
{
  return pgp_gpgme_make_key_attachment(tempf);
}
#endif

static void crypt_mod_pgp_set_sender(const char *sender)
{
  mutt_gpgme_set_sender(sender);
}

struct CryptModuleSpecs crypt_mod_pgp_gpgme = {
  APPLICATION_PGP,
  {
      /* Common.  */
      crypt_mod_pgp_init, crypt_mod_pgp_void_passphrase, crypt_mod_pgp_valid_passphrase,
      crypt_mod_pgp_decrypt_mime, crypt_mod_pgp_application_handler,
      crypt_mod_pgp_encrypted_handler, crypt_mod_pgp_findkeys, crypt_mod_pgp_sign_message,
      crypt_mod_pgp_verify_one, crypt_mod_pgp_send_menu, crypt_mod_pgp_set_sender,

      /* PGP specific.  */
      crypt_mod_pgp_encrypt_message,
#ifdef HAVE_GPGME_OP_EXPORT_KEYS
      crypt_mod_pgp_make_key_attachment,
#else
      NULL,
#endif
      crypt_mod_pgp_check_traditional, NULL, /* pgp_traditional_encryptsign  */
      NULL,                                  /* pgp_invoke_getkeys  */
      crypt_mod_pgp_invoke_import, NULL, /* pgp_extract_keys_from_attachment_list  */

      NULL, /* smime_getkeys */
      NULL, /* smime_verify_sender */
      NULL, /* smime_build_smime_entity */
      NULL, /* smime_invoke_import */
  },
};
