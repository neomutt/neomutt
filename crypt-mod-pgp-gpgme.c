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

/* 
    This is a crytpo module wrapping the gpgme based pgp code.
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef CRYPT_BACKEND_GPGME

#include "crypt-mod.h"
#include "crypt-gpgme.h"

static void crypt_mod_pgp_init (void)
{
  pgp_gpgme_init ();
}

static void crypt_mod_pgp_void_passphrase (void)
{
  /* Handled by gpg-agent.  */
}

static int crypt_mod_pgp_valid_passphrase (void)
{
  /* Handled by gpg-agent.  */
  return 1;
}

static int crypt_mod_pgp_decrypt_mime (FILE *a, FILE **b, BODY *c, BODY **d)
{
  return pgp_gpgme_decrypt_mime (a, b, c, d);
}

static int crypt_mod_pgp_application_handler (BODY *m, STATE *s)
{
  return pgp_gpgme_application_handler (m, s);
}

static int crypt_mod_pgp_encrypted_handler (BODY *m, STATE *s)
{
  return pgp_gpgme_encrypted_handler (m, s);
}

static int crypt_mod_pgp_check_traditional (FILE *fp, BODY *b, int tagged_only)
{
  return pgp_gpgme_check_traditional (fp, b, tagged_only);
}

static void crypt_mod_pgp_invoke_import (const char *fname)
{
  pgp_gpgme_invoke_import (fname);
}

static char *crypt_mod_pgp_findkeys (ADDRESS *to, ADDRESS *cc, ADDRESS *bcc)
{
  return pgp_gpgme_findkeys (to, cc, bcc);
}

static BODY *crypt_mod_pgp_sign_message (BODY *a)
{
  return pgp_gpgme_sign_message (a);
}

static int crypt_mod_pgp_verify_one (BODY *sigbdy, STATE *s, const char *tempf)
{
  return pgp_gpgme_verify_one (sigbdy, s, tempf);
}

static int crypt_mod_pgp_send_menu (HEADER *msg, int *redraw)
{
  return pgp_gpgme_send_menu (msg, redraw);
}

static BODY *crypt_mod_pgp_encrypt_message (BODY *a, char *keylist, int sign)
{
  return pgp_gpgme_encrypt_message (a, keylist, sign);
}

static void crypt_mod_pgp_set_sender (const char *sender)
{
  gpgme_set_sender (sender);
}

struct crypt_module_specs crypt_mod_pgp_gpgme =
  { APPLICATION_PGP,
    {
      /* Common.  */
      crypt_mod_pgp_init,
      crypt_mod_pgp_void_passphrase,
      crypt_mod_pgp_valid_passphrase,
      crypt_mod_pgp_decrypt_mime,
      crypt_mod_pgp_application_handler,
      crypt_mod_pgp_encrypted_handler,
      crypt_mod_pgp_findkeys,
      crypt_mod_pgp_sign_message,
      crypt_mod_pgp_verify_one,
      crypt_mod_pgp_send_menu,
      crypt_mod_pgp_set_sender,

      /* PGP specific.  */
      crypt_mod_pgp_encrypt_message,
      NULL,			/* pgp_make_key_attachment, */
      crypt_mod_pgp_check_traditional,
      NULL, 			/* pgp_traditional_encryptsign  */
      NULL, /* pgp_invoke_getkeys  */
      crypt_mod_pgp_invoke_import,
      NULL, /* pgp_extract_keys_from_attachment_list  */

      NULL,			/* smime_getkeys */
      NULL,			/* smime_verify_sender */
      NULL,			/* smime_build_smime_entity */
      NULL,			/* smime_invoke_import */
    }
  };

#endif
