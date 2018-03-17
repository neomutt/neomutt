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

#include "config.h"
#include <stdio.h>
#include "crypt_mod.h"
#include "ncrypt.h"
#include "pgp.h"
#include "pgpinvoke.h"
#include "pgpkey.h"

struct Address;
struct Body;
struct Header;
struct State;

static void crypt_mod_pgp_void_passphrase(void)
{
  pgp_void_passphrase();
}

static int crypt_mod_pgp_valid_passphrase(void)
{
  return pgp_valid_passphrase();
}

static int crypt_mod_pgp_decrypt_mime(FILE *a, FILE **b, struct Body *c, struct Body **d)
{
  return pgp_decrypt_mime(a, b, c, d);
}

static int crypt_mod_pgp_application_handler(struct Body *m, struct State *s)
{
  return pgp_application_pgp_handler(m, s);
}

static char *crypt_mod_pgp_findkeys(struct Address *addrlist, int oppenc_mode)
{
  return pgp_find_keys(addrlist, oppenc_mode);
}

static struct Body *crypt_mod_pgp_sign_message(struct Body *a)
{
  return pgp_sign_message(a);
}

static int crypt_mod_pgp_verify_one(struct Body *sigbdy, struct State *s, const char *tempf)
{
  return pgp_verify_one(sigbdy, s, tempf);
}

static int crypt_mod_pgp_send_menu(struct Header *msg)
{
  return pgp_send_menu(msg);
}

static struct Body *crypt_mod_pgp_encrypt_message(struct Body *a, char *keylist, int sign)
{
  return pgp_encrypt_message(a, keylist, sign);
}

static struct Body *crypt_mod_pgp_make_key_attachment(char *tempf)
{
  return pgp_make_key_attachment(tempf);
}

static int crypt_mod_pgp_check_traditional(FILE *fp, struct Body *b, int just_one)
{
  return pgp_check_traditional(fp, b, just_one);
}

static struct Body *crypt_mod_pgp_traditional_encryptsign(struct Body *a,
                                                          int flags, char *keylist)
{
  return pgp_traditional_encryptsign(a, flags, keylist);
}

static int crypt_mod_pgp_encrypted_handler(struct Body *m, struct State *s)
{
  return pgp_encrypted_handler(m, s);
}

static void crypt_mod_pgp_invoke_getkeys(struct Address *addr)
{
  pgp_invoke_getkeys(addr);
}

static void crypt_mod_pgp_invoke_import(const char *fname)
{
  pgp_invoke_import(fname);
}

static void crypt_mod_pgp_extract_keys_from_attachment_list(FILE *fp, int tag,
                                                            struct Body *top)
{
  pgp_extract_keys_from_attachment_list(fp, tag, top);
}

struct CryptModuleSpecs crypt_mod_pgp_classic = {
  APPLICATION_PGP,
  {
      NULL, /* init */
      crypt_mod_pgp_void_passphrase,
      crypt_mod_pgp_valid_passphrase,
      crypt_mod_pgp_decrypt_mime,
      crypt_mod_pgp_application_handler,
      crypt_mod_pgp_encrypted_handler,
      crypt_mod_pgp_findkeys,
      crypt_mod_pgp_sign_message,
      crypt_mod_pgp_verify_one,
      crypt_mod_pgp_send_menu,
      NULL,

      crypt_mod_pgp_encrypt_message,
      crypt_mod_pgp_make_key_attachment,
      crypt_mod_pgp_check_traditional,
      crypt_mod_pgp_traditional_encryptsign,
      crypt_mod_pgp_invoke_getkeys,
      crypt_mod_pgp_invoke_import,
      crypt_mod_pgp_extract_keys_from_attachment_list,

      NULL, /* smime_getkeys */
      NULL, /* smime_verify_sender */
      NULL, /* smime_build_smime_entity */
      NULL, /* smime_invoke_import */
  },
};
