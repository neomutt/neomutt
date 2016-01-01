/*
 * Copyright (C) 2003 Werner Koch <wk@gnupg.org>
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

/*
   This file dispatches the generic crypto functions to the
   implemented backend or provides dummy stubs.  Note, that some
   generic functions are handled in crypt.c.
*/

/* Note: This file has been changed to make use of the new module
   system.  Consequently there's a 1:1 mapping between the functions
   contained in this file and the functions implemented by the crypto
   modules.  */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mutt_crypt.h"

#include "crypt-mod.h"

/*
    
    Generic

*/

#ifdef CRYPT_BACKEND_CLASSIC_PGP
extern struct crypt_module_specs crypt_mod_pgp_classic;
#endif

#ifdef CRYPT_BACKEND_CLASSIC_SMIME
extern struct crypt_module_specs crypt_mod_smime_classic;
#endif

#ifdef CRYPT_BACKEND_GPGME
extern struct crypt_module_specs crypt_mod_pgp_gpgme;
extern struct crypt_module_specs crypt_mod_smime_gpgme;
#endif

void crypt_init (void)
{
#ifdef CRYPT_BACKEND_CLASSIC_PGP
  if (
#ifdef CRYPT_BACKEND_GPGME
      (! option (OPTCRYPTUSEGPGME))
#else
       1
#endif
      )
    crypto_module_register (&crypt_mod_pgp_classic);
#endif

#ifdef CRYPT_BACKEND_CLASSIC_SMIME
  if (
#ifdef CRYPT_BACKEND_GPGME
      (! option (OPTCRYPTUSEGPGME))
#else
       1
#endif
      )
    crypto_module_register (&crypt_mod_smime_classic);
#endif

  if (option (OPTCRYPTUSEGPGME))
    {
#ifdef CRYPT_BACKEND_GPGME
      crypto_module_register (&crypt_mod_pgp_gpgme);
      crypto_module_register (&crypt_mod_smime_gpgme);
#else
      mutt_message (_("\"crypt_use_gpgme\" set"
                      " but not built with GPGME support."));
      if (mutt_any_key_to_continue (NULL) == -1)
	mutt_exit(1);
#endif
    }

#if defined CRYPT_BACKEND_CLASSIC_PGP || defined CRYPT_BACKEND_CLASSIC_SMIME || defined CRYPT_BACKEND_GPGME
  if (CRYPT_MOD_CALL_CHECK (PGP, init))
    (CRYPT_MOD_CALL (PGP, init)) ();

  if (CRYPT_MOD_CALL_CHECK (SMIME, init))
    (CRYPT_MOD_CALL (SMIME, init)) ();
#endif
}


/* Show a message that a backend will be invoked. */
void crypt_invoke_message (int type)
{
  if ((WithCrypto & APPLICATION_PGP) && (type & APPLICATION_PGP))
    mutt_message _("Invoking PGP...");
  else if ((WithCrypto & APPLICATION_SMIME) && (type & APPLICATION_SMIME))
    mutt_message _("Invoking S/MIME...");
}



/* 

    PGP

*/


/* Reset a PGP passphrase */
void crypt_pgp_void_passphrase (void)
{
  if (CRYPT_MOD_CALL_CHECK (PGP, void_passphrase))
    (CRYPT_MOD_CALL (PGP, void_passphrase)) ();
}

int crypt_pgp_valid_passphrase (void)
{
  if (CRYPT_MOD_CALL_CHECK (PGP, valid_passphrase))
    return (CRYPT_MOD_CALL (PGP, valid_passphrase)) ();

  return 0;
}


/* Decrypt a PGP/MIME message. */
int crypt_pgp_decrypt_mime (FILE *a, FILE **b, BODY *c, BODY **d)
{
  if (CRYPT_MOD_CALL_CHECK (PGP, decrypt_mime))
    return (CRYPT_MOD_CALL (PGP, decrypt_mime)) (a, b, c, d);

  return -1;
}

/* MIME handler for the application/pgp content-type. */
int crypt_pgp_application_pgp_handler (BODY *m, STATE *s)
{
  if (CRYPT_MOD_CALL_CHECK (PGP, application_handler))
    return (CRYPT_MOD_CALL (PGP, application_handler)) (m, s);
  
  return -1;
}

/* MIME handler for an PGP/MIME encrypted message. */
int crypt_pgp_encrypted_handler (BODY *a, STATE *s)
{
  if (CRYPT_MOD_CALL_CHECK (PGP, encrypted_handler))
    return (CRYPT_MOD_CALL (PGP, encrypted_handler)) (a, s);
  
  return -1;
}

/* fixme: needs documentation. */
void crypt_pgp_invoke_getkeys (ADDRESS *addr)
{
  if (CRYPT_MOD_CALL_CHECK (PGP, pgp_invoke_getkeys))
    (CRYPT_MOD_CALL (PGP, pgp_invoke_getkeys)) (addr);
}

/* Check for a traditional PGP message in body B. */
int crypt_pgp_check_traditional (FILE *fp, BODY *b, int tagged_only)
{
  if (CRYPT_MOD_CALL_CHECK (PGP, pgp_check_traditional))
    return (CRYPT_MOD_CALL (PGP, pgp_check_traditional)) (fp, b, tagged_only);

  return 0;
}

/* fixme: needs documentation. */
BODY *crypt_pgp_traditional_encryptsign (BODY *a, int flags, char *keylist)
{
  if (CRYPT_MOD_CALL_CHECK (PGP, pgp_traditional_encryptsign))
    return (CRYPT_MOD_CALL (PGP, pgp_traditional_encryptsign)) (a, flags, keylist);

  return NULL;
}

/* Generate a PGP public key attachment. */
BODY *crypt_pgp_make_key_attachment (char *tempf)
{
  if (CRYPT_MOD_CALL_CHECK (PGP, pgp_make_key_attachment))
    return (CRYPT_MOD_CALL (PGP, pgp_make_key_attachment)) (tempf);

  return NULL;
}

/* This routine attempts to find the keyids of the recipients of a
   message.  It returns NULL if any of the keys can not be found.
   If oppenc_mode is true, only keys that can be determined without
   prompting will be used.  */
char *crypt_pgp_findkeys (ADDRESS *adrlist, int oppenc_mode)
{
  if (CRYPT_MOD_CALL_CHECK (PGP, findkeys))
    return (CRYPT_MOD_CALL (PGP, findkeys)) (adrlist, oppenc_mode);

  return NULL;
}

/* Create a new body with a PGP signed message from A. */
BODY *crypt_pgp_sign_message (BODY *a)
{
  if (CRYPT_MOD_CALL_CHECK (PGP, sign_message))
    return (CRYPT_MOD_CALL (PGP, sign_message)) (a);

  return NULL;
}

/* Warning: A is no longer freed in this routine, you need to free it
   later.  This is necessary for $fcc_attach. */
BODY *crypt_pgp_encrypt_message (BODY *a, char *keylist, int sign)
{
  if (CRYPT_MOD_CALL_CHECK (PGP, pgp_encrypt_message))
    return (CRYPT_MOD_CALL (PGP, pgp_encrypt_message)) (a, keylist, sign);

  return NULL;
}

/* Invoke the PGP command to import a key. */
void crypt_pgp_invoke_import (const char *fname)
{
  if (CRYPT_MOD_CALL_CHECK (PGP, pgp_invoke_import))
    (CRYPT_MOD_CALL (PGP, pgp_invoke_import)) (fname);
}

/* fixme: needs documentation */
int crypt_pgp_verify_one (BODY *sigbdy, STATE *s, const char *tempf)
{
  if (CRYPT_MOD_CALL_CHECK (PGP, verify_one))
    return (CRYPT_MOD_CALL (PGP, verify_one)) (sigbdy, s, tempf);

  return -1;
}


int crypt_pgp_send_menu (HEADER *msg, int *redraw)
{
  if (CRYPT_MOD_CALL_CHECK (PGP, send_menu))
    return (CRYPT_MOD_CALL (PGP, send_menu)) (msg, redraw);

  return 0;
}


/* fixme: needs documentation */
void crypt_pgp_extract_keys_from_attachment_list (FILE *fp, int tag, BODY *top)
{
  if (CRYPT_MOD_CALL_CHECK (PGP, pgp_extract_keys_from_attachment_list))
    (CRYPT_MOD_CALL (PGP, pgp_extract_keys_from_attachment_list)) (fp, tag, top);
}

void crypt_pgp_set_sender (const char *sender)
{
  if (CRYPT_MOD_CALL_CHECK (PGP, set_sender))
    (CRYPT_MOD_CALL (PGP, set_sender)) (sender);
}




/* 

   S/MIME 

*/


/* Reset an SMIME passphrase */
void crypt_smime_void_passphrase (void)
{
  if (CRYPT_MOD_CALL_CHECK (SMIME, void_passphrase))
    (CRYPT_MOD_CALL (SMIME, void_passphrase)) ();
}

int crypt_smime_valid_passphrase (void)
{
  if (CRYPT_MOD_CALL_CHECK (SMIME, valid_passphrase))
    return (CRYPT_MOD_CALL (SMIME, valid_passphrase)) ();

  return 0;
}

/* Decrypt am S/MIME message. */
int crypt_smime_decrypt_mime (FILE *a, FILE **b, BODY *c, BODY **d)
{
  if (CRYPT_MOD_CALL_CHECK (SMIME, decrypt_mime))
    return (CRYPT_MOD_CALL (SMIME, decrypt_mime)) (a, b, c, d);

  return -1;
}

/* MIME handler for the application/smime content-type. */
int crypt_smime_application_smime_handler (BODY *m, STATE *s)
{
  if (CRYPT_MOD_CALL_CHECK (SMIME, application_handler))
    return (CRYPT_MOD_CALL (SMIME, application_handler)) (m, s);
  
  return -1;
}

/* MIME handler for an PGP/MIME encrypted message. */
void crypt_smime_encrypted_handler (BODY *a, STATE *s)
{
  if (CRYPT_MOD_CALL_CHECK (SMIME, encrypted_handler))
    (CRYPT_MOD_CALL (SMIME, encrypted_handler)) (a, s);
}

/* fixme: Needs documentation. */
void crypt_smime_getkeys (ENVELOPE *env)
{
  if (CRYPT_MOD_CALL_CHECK (SMIME, smime_getkeys))
    (CRYPT_MOD_CALL (SMIME, smime_getkeys)) (env);
}

/* Check that the sender matches. */
int crypt_smime_verify_sender(HEADER *h)
{
  if (CRYPT_MOD_CALL_CHECK (SMIME, smime_verify_sender))
    return (CRYPT_MOD_CALL (SMIME, smime_verify_sender)) (h);

  return 1;
}

/* This routine attempts to find the keyids of the recipients of a
   message.  It returns NULL if any of the keys can not be found.
   If oppenc_mode is true, only keys that can be determined without
   prompting will be used.  */
char *crypt_smime_findkeys (ADDRESS *adrlist, int oppenc_mode)
{
  if (CRYPT_MOD_CALL_CHECK (SMIME, findkeys))
    return (CRYPT_MOD_CALL (SMIME, findkeys)) (adrlist, oppenc_mode);

  return NULL;
}

/* fixme: Needs documentation. */
BODY *crypt_smime_sign_message (BODY *a)
{
  if (CRYPT_MOD_CALL_CHECK (SMIME, sign_message))
    return (CRYPT_MOD_CALL (SMIME, sign_message)) (a);

  return NULL;
}

/* fixme: needs documentation. */
BODY *crypt_smime_build_smime_entity (BODY *a, char *certlist)
{
  if (CRYPT_MOD_CALL_CHECK (SMIME, smime_build_smime_entity))
    return (CRYPT_MOD_CALL (SMIME, smime_build_smime_entity)) (a, certlist);

  return NULL;
}

/* Add a certificate and update index file (externally). */
void crypt_smime_invoke_import (char *infile, char *mailbox)
{
  if (CRYPT_MOD_CALL_CHECK (SMIME, smime_invoke_import))
    (CRYPT_MOD_CALL (SMIME, smime_invoke_import)) (infile, mailbox);
}

/* fixme: needs documentation */
int crypt_smime_verify_one (BODY *sigbdy, STATE *s, const char *tempf)
{
  if (CRYPT_MOD_CALL_CHECK (SMIME, verify_one))
    return (CRYPT_MOD_CALL (SMIME, verify_one)) (sigbdy, s, tempf);

  return -1;
}

int crypt_smime_send_menu (HEADER *msg, int *redraw)
{
  if (CRYPT_MOD_CALL_CHECK (SMIME, send_menu))
    return (CRYPT_MOD_CALL (SMIME, send_menu)) (msg, redraw);

  return 0;
}

void crypt_smime_set_sender (const char *sender)
{
  if (CRYPT_MOD_CALL_CHECK (SMIME, set_sender))
    (CRYPT_MOD_CALL (SMIME, set_sender)) (sender);
}
