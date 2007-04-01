/*
 * Copyright (C) 2004 g10 Code GmbH
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef CRYPTOGRAPHY_H
#define CRYPTOGRAPHY_H

#include "mutt.h"
#include "mutt_crypt.h"

#define CRYPTO_SUPPORT(identifier) (WithCrypto & APPLICATION_ ## identifier)


/* 
    Type defintions for crypto module functions.
 */
typedef void (*crypt_func_void_passphrase_t) (void);
typedef int (*crypt_func_valid_passphrase_t)  (void);

typedef int (*crypt_func_decrypt_mime_t) (FILE *a, FILE **b,
                                          BODY *c, BODY **d);

typedef int (*crypt_func_application_handler_t) (BODY *m, STATE *s);
typedef int (*crypt_func_encrypted_handler_t) (BODY *m, STATE *s);

typedef void (*crypt_func_pgp_invoke_getkeys_t) (ADDRESS *addr);
typedef int (*crypt_func_pgp_check_traditional_t) (FILE *fp, BODY *b,
                                                   int tagged_only);
typedef BODY *(*crypt_func_pgp_traditional_encryptsign_t) (BODY *a, int flags,
                                                           char *keylist);
typedef BODY *(*crypt_func_pgp_make_key_attachment_t) (char *tempf);
typedef char *(*crypt_func_findkeys_t) (ADDRESS *to,
                                        ADDRESS *cc, ADDRESS *bcc);
typedef BODY *(*crypt_func_sign_message_t) (BODY *a);
typedef BODY *(*crypt_func_pgp_encrypt_message_t) (BODY *a, char *keylist,
                                                   int sign);
typedef void (*crypt_func_pgp_invoke_import_t) (const char *fname);
typedef int (*crypt_func_verify_one_t) (BODY *sigbdy, STATE *s,
                                        const char *tempf);
typedef void (*crypt_func_pgp_extract_keys_from_attachment_list_t) 
                                           (FILE *fp, int tag, BODY *top);

typedef int (*crypt_func_send_menu_t) (HEADER *msg, int *redraw);

 /* (SMIME) */
typedef void (*crypt_func_smime_getkeys_t) (ENVELOPE *env);
typedef int (*crypt_func_smime_verify_sender_t) (HEADER *h);

typedef BODY *(*crypt_func_smime_build_smime_entity_t) (BODY *a,
                                                        char *certlist);

typedef void (*crypt_func_smime_invoke_import_t) (char *infile, char *mailbox);

typedef void (*crypt_func_init_t) (void);

typedef void (*crypt_func_set_sender_t) (const char *sender);

/*
   A structure to keep all crypto module fucntions together.
 */
typedef struct crypt_module_functions
{
  /* Common/General functions.  */
  crypt_func_init_t init;
  crypt_func_void_passphrase_t void_passphrase;
  crypt_func_valid_passphrase_t valid_passphrase;
  crypt_func_decrypt_mime_t decrypt_mime;
  crypt_func_application_handler_t application_handler;
  crypt_func_encrypted_handler_t encrypted_handler;
  crypt_func_findkeys_t findkeys;
  crypt_func_sign_message_t sign_message;
  crypt_func_verify_one_t verify_one;
  crypt_func_send_menu_t send_menu;
  crypt_func_set_sender_t set_sender;

  /* PGP specific functions.  */
  crypt_func_pgp_encrypt_message_t pgp_encrypt_message;
  crypt_func_pgp_make_key_attachment_t pgp_make_key_attachment;
  crypt_func_pgp_check_traditional_t pgp_check_traditional;
  crypt_func_pgp_traditional_encryptsign_t pgp_traditional_encryptsign;
  crypt_func_pgp_invoke_getkeys_t pgp_invoke_getkeys;
  crypt_func_pgp_invoke_import_t pgp_invoke_import;
  crypt_func_pgp_extract_keys_from_attachment_list_t
                                 pgp_extract_keys_from_attachment_list;

  /* S/MIME specific functions.  */

  crypt_func_smime_getkeys_t smime_getkeys;
  crypt_func_smime_verify_sender_t smime_verify_sender;
  crypt_func_smime_build_smime_entity_t smime_build_smime_entity;
  crypt_func_smime_invoke_import_t smime_invoke_import;
} crypt_module_functions_t;


/*
   A structure to decribe a crypto module. 
 */
typedef struct crypt_module_specs
{
  int identifier;			/* Identifying bit.  */
  crypt_module_functions_t functions;
} *crypt_module_specs_t;



/* 
   High Level crypto module interface. 
 */

void crypto_module_register (crypt_module_specs_t specs);
crypt_module_specs_t crypto_module_lookup (int identifier);

/* If the crypto module identifier by IDENTIFIER has been registered,
   call its function FUNC.  Do nothing else.  This may be used as an
   expression. */
#define CRYPT_MOD_CALL_CHECK(identifier, func) \
  (crypto_module_lookup (APPLICATION_ ## identifier) \
   && (crypto_module_lookup (APPLICATION_ ## identifier))->functions.func)

/* Call the function FUNC in the crypto module identified by
   IDENTIFIER. This may be used as an expression. */
#define CRYPT_MOD_CALL(identifier, func) \
  *(crypto_module_lookup (APPLICATION_ ## identifier))->functions.func

#endif
