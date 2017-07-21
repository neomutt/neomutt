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

#include <stdio.h>
#include "ncrypt.h"

struct Address;
struct Body;
struct Envelope;
struct Header;
struct State;

/*
    Type definitions for crypto module functions.
 */
typedef void (*crypt_func_void_passphrase_t)(void);
typedef int (*crypt_func_valid_passphrase_t)(void);

typedef int (*crypt_func_decrypt_mime_t)(FILE *a, FILE **b, struct Body *c, struct Body **d);

typedef int (*crypt_func_application_handler_t)(struct Body *m, struct State *s);
typedef int (*crypt_func_encrypted_handler_t)(struct Body *m, struct State *s);

typedef void (*crypt_func_pgp_invoke_getkeys_t)(struct Address *addr);
typedef int (*crypt_func_pgp_check_traditional_t)(FILE *fp, struct Body *b, int tagged_only);
typedef struct Body *(*crypt_func_pgp_traditional_encryptsign_t)(struct Body *a, int flags, char *keylist);
typedef struct Body *(*crypt_func_pgp_make_key_attachment_t)(char *tempf);
typedef char *(*crypt_func_findkeys_t)(struct Address *adrlist, int oppenc_mode);
typedef struct Body *(*crypt_func_sign_message_t)(struct Body *a);
typedef struct Body *(*crypt_func_pgp_encrypt_message_t)(struct Body *a, char *keylist, int sign);
typedef void (*crypt_func_pgp_invoke_import_t)(const char *fname);
typedef int (*crypt_func_verify_one_t)(struct Body *sigbdy, struct State *s, const char *tempf);
typedef void (*crypt_func_pgp_extract_keys_from_attachment_list_t)(FILE *fp, int tag,
                                                                   struct Body *top);

typedef int (*crypt_func_send_menu_t)(struct Header *msg);

/* (SMIME) */
typedef void (*crypt_func_smime_getkeys_t)(struct Envelope *env);
typedef int (*crypt_func_smime_verify_sender_t)(struct Header *h);

typedef struct Body *(*crypt_func_smime_build_smime_entity_t)(struct Body *a, char *certlist);

typedef void (*crypt_func_smime_invoke_import_t)(char *infile, char *mailbox);

typedef void (*crypt_func_init_t)(void);

typedef void (*crypt_func_set_sender_t)(const char *sender);

/**
 * struct CryptModuleFunctions - Crypto API for signing/verifying/encrypting
 *
 * A structure to keep all crypto module functions together.
 */
typedef struct CryptModuleFunctions
{
  /* Common/General functions.  */
  crypt_func_init_t                init;
  crypt_func_void_passphrase_t     void_passphrase;
  crypt_func_valid_passphrase_t    valid_passphrase;
  crypt_func_decrypt_mime_t        decrypt_mime;
  crypt_func_application_handler_t application_handler;
  crypt_func_encrypted_handler_t   encrypted_handler;
  crypt_func_findkeys_t            findkeys;
  crypt_func_sign_message_t        sign_message;
  crypt_func_verify_one_t          verify_one;
  crypt_func_send_menu_t           send_menu;
  crypt_func_set_sender_t          set_sender;

  /* PGP specific functions.  */
  crypt_func_pgp_encrypt_message_t                   pgp_encrypt_message;
  crypt_func_pgp_make_key_attachment_t               pgp_make_key_attachment;
  crypt_func_pgp_check_traditional_t                 pgp_check_traditional;
  crypt_func_pgp_traditional_encryptsign_t           pgp_traditional_encryptsign;
  crypt_func_pgp_invoke_getkeys_t                    pgp_invoke_getkeys;
  crypt_func_pgp_invoke_import_t                     pgp_invoke_import;
  crypt_func_pgp_extract_keys_from_attachment_list_t pgp_extract_keys_from_attachment_list;

  /* S/MIME specific functions.  */
  crypt_func_smime_getkeys_t            smime_getkeys;
  crypt_func_smime_verify_sender_t      smime_verify_sender;
  crypt_func_smime_build_smime_entity_t smime_build_smime_entity;
  crypt_func_smime_invoke_import_t      smime_invoke_import;
} crypt_module_functions_t;

/**
 * struct CryptModuleSpecs - Crypto API
 *
 * A structure to describe a crypto module.
 */
typedef struct CryptModuleSpecs
{
  int identifier; /**< Identifying bit */
  crypt_module_functions_t functions;
} * crypt_module_specs_t;

/*
   High Level crypto module interface.
 */

void crypto_module_register(crypt_module_specs_t specs);
crypt_module_specs_t crypto_module_lookup(int identifier);

/** If the crypto module identifier by IDENTIFIER has been registered,
   call its function FUNC.  Do nothing else.  This may be used as an
   expression. */
#define CRYPT_MOD_CALL_CHECK(identifier, func)                                 \
  (crypto_module_lookup(APPLICATION_##identifier) &&                           \
   (crypto_module_lookup(APPLICATION_##identifier))->functions.func)

/** Call the function FUNC in the crypto module identified by
   IDENTIFIER. This may be used as an expression. */
#define CRYPT_MOD_CALL(identifier, func)                                       \
  *(crypto_module_lookup(APPLICATION_##identifier))->functions.func

#endif /* _NCRYPT_CRYPT_MOD_H */
