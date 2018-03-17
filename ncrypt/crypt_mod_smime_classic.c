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

#include "config.h"
#include <stdio.h>
#include "crypt_mod.h"
#include "ncrypt.h"
#include "smime.h"

struct Address;
struct Body;
struct Envelope;
struct Header;
struct State;

static void crypt_mod_smime_void_passphrase(void)
{
  smime_void_passphrase();
}

static int crypt_mod_smime_valid_passphrase(void)
{
  return smime_valid_passphrase();
}

static int crypt_mod_smime_decrypt_mime(FILE *a, FILE **b, struct Body *c, struct Body **d)
{
  return smime_decrypt_mime(a, b, c, d);
}

static int crypt_mod_smime_application_handler(struct Body *m, struct State *s)
{
  return smime_application_smime_handler(m, s);
}

static char *crypt_mod_smime_findkeys(struct Address *addrlist, int oppenc_mode)
{
  return smime_find_keys(addrlist, oppenc_mode);
}

static struct Body *crypt_mod_smime_sign_message(struct Body *a)
{
  return smime_sign_message(a);
}

static int crypt_mod_smime_verify_one(struct Body *sigbdy, struct State *s, const char *tempf)
{
  return smime_verify_one(sigbdy, s, tempf);
}

static int crypt_mod_smime_send_menu(struct Header *msg)
{
  return smime_send_menu(msg);
}

static void crypt_mod_smime_getkeys(struct Envelope *env)
{
  smime_getkeys(env);
}

static int crypt_mod_smime_verify_sender(struct Header *h)
{
  return smime_verify_sender(h);
}

static struct Body *crypt_mod_smime_build_smime_entity(struct Body *a, char *certlist)
{
  return smime_build_smime_entity(a, certlist);
}

static void crypt_mod_smime_invoke_import(char *infile, char *mailbox)
{
  smime_invoke_import(infile, mailbox);
}

struct CryptModuleSpecs crypt_mod_smime_classic = {
  APPLICATION_SMIME,
  {
      NULL, /* init */
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

      crypt_mod_smime_getkeys,
      crypt_mod_smime_verify_sender,
      crypt_mod_smime_build_smime_entity,
      crypt_mod_smime_invoke_import,
  },
};
