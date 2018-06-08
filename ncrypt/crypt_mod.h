/**
 * @file
 * Register crypto modules
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef _NCRYPT_CRYPT_MOD_H
#define _NCRYPT_CRYPT_MOD_H

#include <stdbool.h>
#include <stdio.h>

struct Address;
struct Body;
struct Envelope;
struct Header;
struct State;

/**
 * struct CryptModuleSpecs - Crypto API
 *
 * A structure to describe a crypto module.
 */
struct CryptModuleSpecs
{
  int identifier; /**< Identifying bit */

  /* Common/General functions */
  void         (*init)(void);
  void         (*void_passphrase)(void);
  int          (*valid_passphrase)(void);
  int          (*decrypt_mime)(FILE *fpin, FILE **fpout, struct Body *b, struct Body **cur);
  int          (*application_handler)(struct Body *m, struct State *s);
  int          (*encrypted_handler)(struct Body *m, struct State *s);
  char *       (*findkeys)(struct Address *addrlist, bool oppenc_mode);
  struct Body *(*sign_message)(struct Body *a);
  int          (*verify_one)(struct Body *sigbdy, struct State *s, const char *tempf);
  int          (*send_menu)(struct Header *msg);
  void         (*set_sender)(const char *sender);

  /* PGP specific functions */
  struct Body *(*pgp_encrypt_message)(struct Body *a, char *keylist, int sign);
  struct Body *(*pgp_make_key_attachment)(char *tempf);
  int          (*pgp_check_traditional)(FILE *fp, struct Body *b, int just_one);
  struct Body *(*pgp_traditional_encryptsign)(struct Body *a, int flags, char *keylist);
  void         (*pgp_invoke_getkeys)(struct Address *addr);
  void         (*pgp_invoke_import)(const char *fname);
  void         (*pgp_extract_keys_from_attachment_list)(FILE *fp, int tag, struct Body *top);

  /* S/MIME specific functions */
  void         (*smime_getkeys)(struct Envelope *env);
  int          (*smime_verify_sender)(struct Header *h);
  struct Body *(*smime_build_smime_entity)(struct Body *a, char *certlist);
  void         (*smime_invoke_import)(char *infile, char *mailbox);
};

/* High Level crypto module interface */
void crypto_module_register(struct CryptModuleSpecs *specs);
struct CryptModuleSpecs *crypto_module_lookup(int identifier);

#endif /* _NCRYPT_CRYPT_MOD_H */
