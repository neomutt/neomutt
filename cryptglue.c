/*
 * Copyright (C) 2003  Werner Koch <wk@gnupg.org>
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
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */

/*
   This file dispatches the generic crytpo functions to the implemented
   backend or provides dummy stubs.  Note, that some generic functions are
   handled in crypt.c.
*/

#include "mutt.h"
#include "mutt_crypt.h"

/* Make sure those macros are not defined. */
#undef BFNC_PGP_VOID_PASSPHRASE     
#undef BFNC_PGP_DECRYPT_MIME           
#undef BFNC_PGP_APPLICATION_PGP_HANDLER
#undef BFNC_PGP_ENCRYPTED_HANDLER
#undef BFNC_PGP_INVOKE_GETKEYS
#undef BFNC_PGP_ASK_FOR_KEY
#undef BNFC_PGP_CHECK_TRADITIONAL
#undef BFNC_PGP_TRADITIONAL_ENCRYPTSIGN  
#undef BFNC_PGP_FREE_KEY
#undef BFNC_PGP_MAKE_KEY_ATTACHMENT 
#undef BFNC_PGP_FINDKEYS
#undef BFNC_PGP_SIGN_MESSAGE
#undef BFNC_PGP_ENCRYPT_MESSAGE
#undef BFNC_PGP_INVOKE_IMPORT
#undef BFNC_PGP_VERIFY_ONE
#undef BFNC_PGP_KEYID
#undef BFNC_PGP_EXTRACT_KEYS_FROM_ATTACHMENT_LIST

#undef BFNC_SMIME_VOID_PASSPHRASE 
#undef BFNC_SMIME_DECRYPT_MIME    
#undef BFNC_SMIME_APPLICATION_SMIME_HANDLER 
#undef BFNC_SMIME_GETKEYS  
#undef BFNC_SMIME_VERIFY_SENDER
#undef BFNC_SMIME_ASK_FOR_KEY
#undef BFNC_SMIME_FINDKEYS
#undef BFNC_SMIME_SIGN_MESSAGE
#undef BFNC_SMIME_BUILD_SMIME_ENTITY
#undef BFNC_SMIME_INVOKE_IMPORT
#undef BFNC_SMIME_VERIFY_ONE


/* The PGP backend */
#if defined (CRYPT_BACKEND_CLASSIC_PGP)
# include "pgp.h"
# define BFNC_PGP_VOID_PASSPHRASE         pgp_void_passphrase
# define BFNC_PGP_DECRYPT_MIME            pgp_decrypt_mime
# define BFNC_PGP_APPLICATION_PGP_HANDLER pgp_application_pgp_handler
# define BFNC_PGP_ENCRYPTED_HANDLER       pgp_encrypted_handler
# define BFNC_PGP_INVOKE_GETKEYS          pgp_invoke_getkeys
# define BFNC_PGP_ASK_FOR_KEY             pgp_ask_for_key
# define BNFC_PGP_CHECK_TRADITIONAL       pgp_check_traditional
# define BFNC_PGP_TRADITIONAL_ENCRYPTSIGN pgp_traditional_encryptsign 
# define BFNC_PGP_FREE_KEY                pgp_free_key
# define BFNC_PGP_MAKE_KEY_ATTACHMENT     pgp_make_key_attachment
# define BFNC_PGP_FINDKEYS                pgp_findKeys
# define BFNC_PGP_SIGN_MESSAGE            pgp_sign_message
# define BFNC_PGP_ENCRYPT_MESSAGE         pgp_encrypt_message
# define BFNC_PGP_INVOKE_IMPORT           pgp_invoke_import
# define BFNC_PGP_VERIFY_ONE              pgp_verify_one
# define BFNC_PGP_KEYID                   pgp_keyid
# define BFNC_PGP_EXTRACT_KEYS_FROM_ATTACHMENT_LIST \
                                       pgp_extract_keys_from_attachment_list


#elif defined (CRYPT_BACKEND_GPGME)
# include "crypt-gpgme.h"
# define BFNC_PGP_VOID_PASSPHRASE NULL /* not required */
# define BFNC_PGP_DECRYPT_MIME     gpg_pgp_decrypt_mime

#endif /* PGP backend */


/* The SMIME backend */
#ifdef CRYPT_BACKEND_CLASSIC_SMIME
# include "smime.h"
# define BFNC_SMIME_VOID_PASSPHRASE           smime_void_passphrase
# define BFNC_SMIME_DECRYPT_MIME              smime_decrypt_mime
# define BFNC_SMIME_APPLICATION_SMIME_HANDLER smime_application_smime_handler
# define BFNC_SMIME_GETKEYS                   smime_getkeys
# define BFNC_SMIME_VERIFY_SENDER             smime_verify_sender
# define BFNC_SMIME_ASK_FOR_KEY               smime_ask_for_key
# define BFNC_SMIME_FINDKEYS                  smime_findKeys
# define BFNC_SMIME_SIGN_MESSAGE              smime_sign_message
# define BFNC_SMIME_BUILD_SMIME_ENTITY        smime_build_smime_entity
# define BFNC_SMIME_INVOKE_IMPORT             smime_invoke_import
# define BFNC_SMIME_VERIFY_ONE            smime_verify_one

#elif defined (CRYPT_BACKEND_GPGME)
  /* Already included above (gpgme supports both). */ 
# define BFNC_SMIME_VOID_PASSPHRASE NULL /* not required */

#endif /* SMIME backend */


/*
    
    Generic

*/

/* Show a message that a backend will be invoked. */
void crypt_invoke_message (int type)
{
#if defined (CRYPT_BACKEND_CLASSIC_PGP) || defined(CRYPT_BACKEND_CLASSIC_SMIME)
  if ((type & APPLICATION_PGP))
    mutt_message _("Invoking PGP...");
  if ((type & APPLICATION_SMIME))
    mutt_message _("Invoking OpenSSL...");
#elif defined (CRYPT_BACKEND_GPGME)
  if ((type & APPLICATION_PGP) || (type & APPLICATION_SMIME) )
    mutt_message _("Invoking GnuPG...");
#endif
}



/* 

    PGP

*/


/* Reset a PGP passphrase */
void crypt_pgp_void_passphrase (void)
{
#ifdef BFNC_PGP_VOID_PASSPHRASE
  BFNC_PGP_VOID_PASSPHRASE ();
#endif
}

/* Decrypt a PGP/MIME message. */
int crypt_pgp_decrypt_mime (FILE *a, FILE **b, BODY *c, BODY **d)
{
#ifdef BFNC_PGP_DECRYPT_MIME
  return BFNC_PGP_DECRYPT_MIME (a, b, c, d);
#else
  return -1; /* error */
#endif
}

/* MIME handler for the application/pgp content-type. */
void crypt_pgp_application_pgp_handler (BODY *m, STATE *s)
{
#ifdef BFNC_PGP_APPLICATION_PGP_HANDLER
  BFNC_PGP_APPLICATION_PGP_HANDLER (m, s);
#endif
}

/* MIME handler for an PGP/MIME encrypted message. */
void crypt_pgp_encrypted_handler (BODY *a, STATE *s)
{
#ifdef BFNC_PGP_ENCRYPTED_HANDLER
  BFNC_PGP_ENCRYPTED_HANDLER (a, s);
#endif
}

/* fixme: needs documentation. */
void crypt_pgp_invoke_getkeys (ADDRESS *addr)
{
#ifdef BFNC_PGP_INVOKE_GETKEYS
  BFNC_PGP_INVOKE_GETKEYS (addr);
#endif
}

/* Ask for a PGP key. */
pgp_key_t crypt_pgp_ask_for_key (char *tag, char *whatfor,
                                 short abilities, pgp_ring_t keyring)
{
#ifdef BFNC_PGP_ASK_FOR_KEY
  return BFNC_PGP_ASK_FOR_KEY (tag, whatfor, abilities, keyring);
#else
  return NULL;
#endif
}


/* Check for a traditional PGP message in body B. */
int crypt_pgp_check_traditional (FILE *fp, BODY *b, int tagged_only)
{
#ifdef BNFC_PGP_CHECK_TRADITIONAL
  return BNFC_PGP_CHECK_TRADITIONAL (fp, b, tagged_only);
#else
  return 0; /* no */
#endif
}

/* fixme: needs documentation. */
BODY *crypt_pgp_traditional_encryptsign (BODY *a, int flags, char *keylist)
{
#ifdef BFNC_PGP_TRADITIONAL_ENCRYPTSIGN  
  return BFNC_PGP_TRADITIONAL_ENCRYPTSIGN (a, flags, keylist);
#else
  return NULL;
#endif
}

/* Release pgp key KPP. */
void crypt_pgp_free_key (pgp_key_t *kpp)
{
#ifdef BFNC_PGP_FREE_KEY
  BFNC_PGP_FREE_KEY (kpp);
#endif
}


/* Generate a PGP public key attachment. */
BODY *crypt_pgp_make_key_attachment (char *tempf)
{
#ifdef BFNC_PGP_MAKE_KEY_ATTACHMENT 
  return BFNC_PGP_MAKE_KEY_ATTACHMENT (tempf);
#else
  return NULL; /* error */ 
#endif
}

/* This routine attempts to find the keyids of the recipients of a
   message.  It returns NULL if any of the keys can not be found.  */
char *crypt_pgp_findkeys (ADDRESS *to, ADDRESS *cc, ADDRESS *bcc)
{
#ifdef BFNC_PGP_FINDKEYS
  return BFNC_PGP_FINDKEYS (to, cc, bcc);
#else
  return NULL;
#endif
}

/* Create a new body with a PGP signed message from A. */
BODY *crypt_pgp_sign_message (BODY *a)
{
#ifdef BFNC_PGP_SIGN_MESSAGE
  return BFNC_PGP_SIGN_MESSAGE (a);
#else
  return NULL;
#endif
}

/* Warning: A is no longer freed in this routine, you need to free it
   later.  This is necessary for $fcc_attach. */
BODY *crypt_pgp_encrypt_message (BODY *a, char *keylist, int sign)
{
#ifdef BFNC_PGP_ENCRYPT_MESSAGE
  return BFNC_PGP_ENCRYPT_MESSAGE (a, keylist, sign);
#else
  return NULL;
#endif
}

/* Invoke the PGP command to import a key. */
void crypt_pgp_invoke_import (const char *fname)
{
#ifdef BFNC_PGP_INVOKE_IMPORT
  BFNC_PGP_INVOKE_IMPORT (fname);
#endif
}

/* fixme: needs documentation */
int crypt_pgp_verify_one (BODY *sigbdy, STATE *s, const char *tempf)
{
#ifdef BFNC_PGP_VERIFY_ONE
  return BFNC_PGP_VERIFY_ONE (sigbdy, s, tempf);
#else
  return -1;
#endif
}


/* Access the keyID in K. */
char *crypt_pgp_keyid (pgp_key_t k)
{
#ifdef BFNC_PGP_KEYID
  return pgp_keyid (k);
#else
  return "?";
#endif
}

/* fixme: needs documentation */
void crypt_pgp_extract_keys_from_attachment_list (FILE *fp, int tag, BODY *top)
{
#ifdef BFNC_PGP_EXTRACT_KEYS_FROM_ATTACHMENT_LIST
  BFNC_PGP_EXTRACT_KEYS_FROM_ATTACHMENT_LIST (fp, tag, top);
#endif
}



/* 

   S/MIME 

*/


/* Reset an SMIME passphrase */
void crypt_smime_void_passphrase (void)
{
#ifdef BFNC_SMIME_VOID_PASSPHRASE
  BFNC_SMIME_VOID_PASSPHRASE ();
#endif
}


/* Decrypt am S/MIME message. */
int crypt_smime_decrypt_mime (FILE *a, FILE **b, BODY *c, BODY **d)
{
#ifdef BFNC_SMIME_DECRYPT_MIME
  return BFNC_SMIME_DECRYPT_MIME (a, b, c, d);
#else
  return -1; /* error */
#endif
}

/* MIME handler for the application/smime content-type. */
void crypt_smime_application_smime_handler (BODY *m, STATE *s)
{
#ifdef BFNC_SMIME_APPLICATION_SMIME_HANDLER
  BFNC_SMIME_APPLICATION_SMIME_HANDLER (m, s);
#endif
}

/* fixme: Needs documentation. */
void crypt_smime_getkeys (ENVELOPE *env)
{
#ifdef BFNC_SMIME_GETKEYS  
  BFNC_SMIME_GETKEYS (env);
#endif
}

/* Check that the sender matches. */
int crypt_smime_verify_sender(HEADER *h)
{
#ifdef BFNC_SMIME_VERIFY_SENDER
  return BFNC_SMIME_VERIFY_SENDER (h);
#else
  return 1; /* yes */
#endif
}

/* Ask for an SMIME key. */
char *crypt_smime_ask_for_key (char *prompt, char *mailbox, short public)
{
#ifdef BFNC_SMIME_ASK_FOR_KEY
  return BFNC_SMIME_ASK_FOR_KEY (prompt, mailbox, public);
#else
  return NULL; /* error */
#endif
}


/* This routine attempts to find the keyids of the recipients of a
   message.  It returns NULL if any of the keys can not be found.  */
char *crypt_smime_findkeys (ADDRESS *to, ADDRESS *cc, ADDRESS *bcc)
{
#ifdef BFNC_SMIME_FINDKEYS
  return BFNC_SMIME_FINDKEYS (to, cc, bcc);
#else
  return NULL;
#endif
}

/* fixme: Needs documentation. */
BODY *crypt_smime_sign_message (BODY *a)
{
#ifdef BFNC_SMIME_SIGN_MESSAGE
  return BFNC_SMIME_SIGN_MESSAGE (a);
#else
  return NULL;
#endif
}

/* fixme: needs documentation. */
BODY *crypt_smime_build_smime_entity (BODY *a, char *certlist)
{
#ifdef BFNC_SMIME_BUILD_SMIME_ENTITY
  return BFNC_SMIME_BUILD_SMIME_ENTITY (a, certlist);
#else
  return NULL;
#endif
}

/* Add a certificate and update index file (externally). */
void crypt_smime_invoke_import (char *infile, char *mailbox)
{
#ifdef BFNC_SMIME_INVOKE_IMPORT
  BFNC_SMIME_INVOKE_IMPORT (infile, mailbox);
#endif
}

/* fixme: needs documentation */
int crypt_smime_verify_one (BODY *sigbdy, STATE *s, const char *tempf)
{
#ifdef BFNC_SMIME_VERIFY_ONE
  return BFNC_SMIME_VERIFY_ONE (sigbdy, s, tempf);
#else
  return -1;
#endif
}
