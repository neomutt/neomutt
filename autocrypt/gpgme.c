/**
 * @file
 * Autocrypt GPGME handler
 *
 * @authors
 * Copyright (C) 2019 Kevin J. McCarthy <kevin@8t8.us>
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

/**
 * @page autocrypt_gpgme Autocrypt GPGME handler
 *
 * Autocrypt GPGME handler
 */

#include "config.h"
#include <stddef.h>
#include <gpgme.h>
#include <stdbool.h>
#include "private.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "options.h"
#include "ncrypt/lib.h"

/**
 * create_gpgme_context - Create a GPGME context
 * @param ctx GPGME context to initialise
 * @retval  0 Success
 * @retval -1 Error
 */
static int create_gpgme_context(gpgme_ctx_t *ctx)
{
  gpgme_error_t err = gpgme_new(ctx);
  if (!err)
    err = gpgme_ctx_set_engine_info(*ctx, GPGME_PROTOCOL_OpenPGP, NULL, C_AutocryptDir);
  if (err)
  {
    mutt_error(_("error creating GPGME context: %s"), gpgme_strerror(err));
    return -1;
  }

  return 0;
}

/**
 * mutt_autocrypt_gpgme_init - Initialise GPGME
 */
int mutt_autocrypt_gpgme_init(void)
{
  pgp_gpgme_init();
  return 0;
}

/**
 * export_keydata - Export Key data from GPGME into a Buffer
 * @param ctx     GPGME context
 * @param key     GPGME key
 * @param keydata Buffer for results
 * @retval  0 Success
 * @retval -1 Error
 */
static int export_keydata(gpgme_ctx_t ctx, gpgme_key_t key, struct Buffer *keydata)
{
  int rc = -1;
  gpgme_data_t dh = NULL;
  gpgme_key_t export_keys[2] = { 0 };
  size_t export_data_len;

  if (gpgme_data_new(&dh))
    goto cleanup;

    /* This doesn't seem to work */
#if 0
  if (gpgme_data_set_encoding (dh, GPGME_DATA_ENCODING_BASE64))
    goto cleanup;
#endif

  export_keys[0] = key;
  export_keys[1] = NULL;
  if (gpgme_op_export_keys(ctx, export_keys, GPGME_EXPORT_MODE_MINIMAL, dh))
    goto cleanup;

  char *export_data = gpgme_data_release_and_get_mem(dh, &export_data_len);
  dh = NULL;

  mutt_b64_buffer_encode(keydata, export_data, export_data_len);
  gpgme_free(export_data);
  export_data = NULL;

  rc = 0;

cleanup:
  gpgme_data_release(dh);
  return rc;
}

#if 0
/**
 * mutt_autocrypt_gpgme_export_key - Export a GPGME key
 * @param keyid   GPGME Key id
 * @param keydata Buffer for results
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_autocrypt_gpgme_export_key(const char *keyid, struct Buffer *keydata)
{
  int rc = -1;
  gpgme_ctx_t ctx = NULL;
  gpgme_key_t key = NULL;

  if (create_gpgme_context(&ctx))
    goto cleanup;

  if (gpgme_get_key(ctx, keyid, &key, 0))
    goto cleanup;

  if (export_keydata(ctx, key, keydata))
    goto cleanup;

  rc = 0;
cleanup:
  gpgme_key_unref(key);
  gpgme_release(ctx);
  return rc;
}
#endif

/**
 * mutt_autocrypt_gpgme_create_key - Create a GPGME key
 * @param addr    Email Address
 * @param keyid   Key id
 * @param keydata Key data
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_autocrypt_gpgme_create_key(struct Address *addr, struct Buffer *keyid,
                                    struct Buffer *keydata)
{
  int rc = -1;
  gpgme_ctx_t ctx = NULL;
  gpgme_genkey_result_t keyresult = NULL;
  gpgme_key_t primary_key = NULL;
  char buf[1024] = { 0 };

  /* GPGME says addresses should not be in idna form */
  struct Address *copy = mutt_addr_copy(addr);
  mutt_addr_to_local(copy);
  mutt_addr_write(buf, sizeof(buf), copy, false);
  mutt_addr_free(&copy);

  if (create_gpgme_context(&ctx))
    goto cleanup;

  /* L10N: Message displayed just before a GPG key is generated for a created
     autocrypt account.  */
  mutt_message(_("Generating autocrypt key..."));

  /* Primary key */
  gpgme_error_t err = gpgme_op_createkey(ctx, buf, "ed25519", 0, 0, NULL,
                                         GPGME_CREATE_NOPASSWD | GPGME_CREATE_FORCE |
                                             GPGME_CREATE_NOEXPIRE);
  if (err)
  {
    /* L10N: GPGME was unable to generate a key for some reason.
       %s is the error message returned by GPGME.  */
    mutt_error(_("Error creating autocrypt key: %s"), gpgme_strerror(err));
    goto cleanup;
  }
  keyresult = gpgme_op_genkey_result(ctx);
  if (!keyresult->fpr)
    goto cleanup;
  mutt_buffer_strcpy(keyid, keyresult->fpr);
  mutt_debug(LL_DEBUG1, "Generated key with id %s\n", mutt_b2s(keyid));

  /* Get gpgme_key_t to create the secondary key and export keydata */
  err = gpgme_get_key(ctx, mutt_b2s(keyid), &primary_key, 0);
  if (err)
    goto cleanup;

  /* Secondary key */
  err = gpgme_op_createsubkey(ctx, primary_key, "cv25519", 0, 0,
                              GPGME_CREATE_NOPASSWD | GPGME_CREATE_NOEXPIRE);
  if (err)
  {
    mutt_error(_("Error creating autocrypt key: %s"), gpgme_strerror(err));
    goto cleanup;
  }

  /* get keydata */
  if (export_keydata(ctx, primary_key, keydata))
    goto cleanup;
  mutt_debug(LL_DEBUG1, "key has keydata *%s*\n", mutt_b2s(keydata));

  rc = 0;

cleanup:
  gpgme_key_unref(primary_key);
  gpgme_release(ctx);
  return rc;
}

/**
 * mutt_autocrypt_gpgme_select_key - Select a Autocrypt key
 * @param[in]  keyid   Key id to select
 * @param[out] keydata Buffer for resulting Key data
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_autocrypt_gpgme_select_key(struct Buffer *keyid, struct Buffer *keydata)
{
  int rc = -1;
  gpgme_ctx_t ctx = NULL;
  gpgme_key_t key = NULL;

  OptAutocryptGpgme = true;
  if (mutt_gpgme_select_secret_key(keyid))
    goto cleanup;

  if (create_gpgme_context(&ctx))
    goto cleanup;

  if (gpgme_get_key(ctx, mutt_b2s(keyid), &key, 0))
    goto cleanup;

  if (key->revoked || key->expired || key->disabled || key->invalid ||
      !key->can_encrypt || !key->can_sign)
  {
    /* L10N: After selecting a key for an autocrypt account,
       this is displayed if the key was revoked/expired/disabled/invalid
       or can't be used for both signing and encryption.
       %s is the key fingerprint.  */
    mutt_error(_("The key %s is not usable for autocrypt"), mutt_b2s(keyid));
    goto cleanup;
  }

  if (export_keydata(ctx, key, keydata))
    goto cleanup;

  rc = 0;

cleanup:
  OptAutocryptGpgme = false;
  gpgme_key_unref(key);
  gpgme_release(ctx);
  return rc;
}

/**
 * mutt_autocrypt_gpgme_select_or_create_key - Ask the user to select or create an Autocrypt key
 * @param addr    Email Address
 * @param keyid   Key id
 * @param keydata Key data
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_autocrypt_gpgme_select_or_create_key(struct Address *addr, struct Buffer *keyid,
                                              struct Buffer *keydata)
{
  int rc = -1;

  /* L10N: During autocrypt account creation, this prompt asks the
     user whether they want to create a new GPG key for the account,
     or select an existing account from the keyring.  */
  const char *prompt = _("(c)reate new, or (s)elect existing GPG key?");
  /* L10N: The letters corresponding to the
     "(c)reate new, or (s)elect existing GPG key?" prompt.  */
  const char *letters = _("cs");

  int choice = mutt_multi_choice(prompt, letters);
  switch (choice)
  {
    case 2: /* select existing */
      rc = mutt_autocrypt_gpgme_select_key(keyid, keydata);
      if (rc == 0)
        break;

      /* L10N: During autocrypt account creation, if selecting an existing key fails
         for some reason, we prompt to see if they want to create a key instead.  */
      if (mutt_yesorno(_("Create a new GPG key for this account, instead?"), MUTT_YES) == MUTT_NO)
        break;
      /* fallthrough */

    case 1: /* create new */
      rc = mutt_autocrypt_gpgme_create_key(addr, keyid, keydata);
  }

  return rc;
}

/**
 * mutt_autocrypt_gpgme_import_key - Read a key from GPGME
 * @param keydata Buffer for key data
 * @param keyid   Buffer for key id
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_autocrypt_gpgme_import_key(const char *keydata, struct Buffer *keyid)
{
  int rc = -1;
  gpgme_ctx_t ctx = NULL;
  gpgme_data_t dh = NULL;

  if (create_gpgme_context(&ctx))
    goto cleanup;

  struct Buffer *raw_keydata = mutt_buffer_pool_get();
  if (!mutt_b64_buffer_decode(raw_keydata, keydata))
    goto cleanup;

  if (gpgme_data_new_from_mem(&dh, mutt_b2s(raw_keydata), mutt_buffer_len(raw_keydata), 0))
    goto cleanup;

  if (gpgme_op_import(ctx, dh))
    goto cleanup;

  gpgme_import_result_t result = gpgme_op_import_result(ctx);
  if (!result->imports || !result->imports->fpr)
    goto cleanup;
  mutt_buffer_strcpy(keyid, result->imports->fpr);

  rc = 0;

cleanup:
  gpgme_data_release(dh);
  gpgme_release(ctx);
  mutt_buffer_pool_release(&raw_keydata);
  return rc;
}

/**
 * mutt_autocrypt_gpgme_is_valid_key - Is a key id valid?
 * @param keyid Key id to check
 * @retval true If key id is valid
 */
bool mutt_autocrypt_gpgme_is_valid_key(const char *keyid)
{
  bool rc = false;
  gpgme_ctx_t ctx = NULL;
  gpgme_key_t key = NULL;

  if (!keyid)
    return false;

  if (create_gpgme_context(&ctx))
    goto cleanup;

  if (gpgme_get_key(ctx, keyid, &key, 0))
    goto cleanup;

  rc = true;
  if (key->revoked || key->expired || key->disabled || key->invalid || !key->can_encrypt)
    rc = false;

cleanup:
  gpgme_key_unref(key);
  gpgme_release(ctx);
  return rc;
}
