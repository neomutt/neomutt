/**
 * @file
 * Wrapper for PGP/SMIME calls to GPGME
 *
 * @authors
 * Copyright (C) 1996-1997,2007 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (C) 1998-2000 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2001 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2001 Oliver Ehli <elmy@acm.org>
 * Copyright (C) 2002-2004, 2018 g10 Code GmbH
 * Copyright (C) 2010,2012-2013 Michael R. Elkins <me@sigpipe.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
 * @page crypt_crypt_gpgme Wrapper for PGP/SMIME calls to GPGME
 *
 * Wrapper for PGP/SMIME calls to GPGME
 */

#include "config.h"
#include <errno.h>
#include <gpg-error.h>
#include <gpgme.h>
#include <langinfo.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "private.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "crypt_gpgme.h"
#include "lib.h"
#include "attach/lib.h"
#include "editor/lib.h"
#include "history/lib.h"
#include "question/lib.h"
#include "send/lib.h"
#include "crypt.h"
#include "globals.h"
#include "handler.h"
#include "hook.h"
#include "mutt_logging.h"
#ifdef USE_AUTOCRYPT
#include "autocrypt/lib.h"
#endif

// clang-format off
/* Values used for comparing addresses. */
#define CRYPT_KV_VALID    (1 << 0)
#define CRYPT_KV_ADDR     (1 << 1)
#define CRYPT_KV_STRING   (1 << 2)
#define CRYPT_KV_STRONGID (1 << 3)
#define CRYPT_KV_MATCH    (CRYPT_KV_ADDR | CRYPT_KV_STRING)
// clang-format on

/**
 * struct CryptCache - Internal cache for GPGME
 */
struct CryptCache
{
  char *what;
  char *dflt;
  struct CryptCache *next;
};

/// Cache of GPGME keys
static struct CryptCache *IdDefaults = NULL;
/// PGP Key to sign with
static gpgme_key_t SignatureKey = NULL;
/// Email address of the sender
static char *CurrentSender = NULL;

#define PKA_NOTATION_NAME "pka-address@gnupg.org"

#define _LINE_COMPARE(_x, _y) line_compare(_x, sizeof(_x) - 1, _y)
#define MESSAGE(_y) _LINE_COMPARE("MESSAGE-----", _y)
#define SIGNED_MESSAGE(_y) _LINE_COMPARE("SIGNED MESSAGE-----", _y)
#define PUBLIC_KEY_BLOCK(_y) _LINE_COMPARE("PUBLIC KEY BLOCK-----", _y)
#define BEGIN_PGP_SIGNATURE(_y)                                                \
  _LINE_COMPARE("-----BEGIN PGP SIGNATURE-----", _y)

/**
 * is_pka_notation - Is this the standard pka email address
 * @param notation GPGME notation
 * @retval true It is the standard pka email address
 */
static bool is_pka_notation(gpgme_sig_notation_t notation)
{
  return mutt_str_equal(notation->name, PKA_NOTATION_NAME);
}

/**
 * redraw_if_needed - Accommodate for a redraw if needed
 * @param ctx GPGME handle
 */
static void redraw_if_needed(gpgme_ctx_t ctx)
{
  const char *s = gpgme_get_ctx_flag(ctx, "redraw");
  if (!s /* flag not known */ || *s /* flag true */)
  {
    mutt_need_hard_redraw();
  }
}

/**
 * crypt_keyid - Find the ID for the key
 * @param k Key to use
 * @retval ptr ID string for the key
 *
 * Return the keyID for the key K.
 * Note that this string is valid as long as K is valid
 */
const char *crypt_keyid(struct CryptKeyInfo *k)
{
  const char *s = "????????";

  if (k->kobj && k->kobj->subkeys)
  {
    s = k->kobj->subkeys->keyid;
    const bool c_pgp_long_ids = cs_subset_bool(NeoMutt->sub, "pgp_long_ids");
    if ((!c_pgp_long_ids) && (strlen(s) == 16))
    {
      /* Return only the short keyID.  */
      s += 8;
    }
  }

  return s;
}

/**
 * crypt_long_keyid - Find the Long ID for the key
 * @param k Key to use
 * @retval ptr Long ID string for the key
 *
 * Return the long keyID for the key K.
 */
static const char *crypt_long_keyid(struct CryptKeyInfo *k)
{
  const char *s = "????????????????";

  if (k->kobj && k->kobj->subkeys)
  {
    s = k->kobj->subkeys->keyid;
  }

  return s;
}

/**
 * crypt_short_keyid - Get the short keyID for a key
 * @param k Key to use
 * @retval ptr Short key string
 */
static const char *crypt_short_keyid(struct CryptKeyInfo *k)
{
  const char *s = "????????";

  if (k->kobj && k->kobj->subkeys)
  {
    s = k->kobj->subkeys->keyid;
    if (strlen(s) == 16)
      s += 8;
  }

  return s;
}

/**
 * crypt_fpr - Get the hexstring fingerprint from a key
 * @param k Key to use
 * @retval ptr Hexstring fingerprint
 */
static const char *crypt_fpr(struct CryptKeyInfo *k)
{
  const char *s = "";

  if (k->kobj && k->kobj->subkeys)
    s = k->kobj->subkeys->fpr;

  return s;
}

/**
 * crypt_fpr_or_lkeyid - Find the fingerprint of a key
 * @param k Key to examine
 * @retval ptr Fingerprint if available, otherwise the long keyid
 */
const char *crypt_fpr_or_lkeyid(struct CryptKeyInfo *k)
{
  const char *s = "????????????????";

  if (k->kobj && k->kobj->subkeys)
  {
    if (k->kobj->subkeys->fpr)
      s = k->kobj->subkeys->fpr;
    else
      s = k->kobj->subkeys->keyid;
  }

  return s;
}

/**
 * crypt_copy_key - Return a copy of KEY
 * @param key Key to copy
 * @retval ptr Copy of key
 */
struct CryptKeyInfo *crypt_copy_key(struct CryptKeyInfo *key)
{
  struct CryptKeyInfo *k = NULL;

  k = mutt_mem_calloc(1, sizeof(*k));
  k->kobj = key->kobj;
  gpgme_key_ref(key->kobj);
  k->idx = key->idx;
  k->uid = key->uid;
  k->flags = key->flags;
  k->validity = key->validity;

  return k;
}

/**
 * crypt_key_free - Release all the keys in a list
 * @param[out] keylist List of keys
 */
static void crypt_key_free(struct CryptKeyInfo **keylist)
{
  if (!keylist)
    return;

  struct CryptKeyInfo *k = NULL;

  while (*keylist)
  {
    k = *keylist;
    *keylist = (*keylist)->next;

    gpgme_key_unref(k->kobj);
    FREE(&k);
  }
}

/**
 * crypt_id_is_strong - Is the key strong
 * @param key Key to test
 * @retval true Validity of key is sufficient
 */
bool crypt_id_is_strong(struct CryptKeyInfo *key)
{
  if (!key)
    return false;

  bool is_strong = false;

  if ((key->flags & KEYFLAG_ISX509))
    return true;

  switch (key->validity)
  {
    case GPGME_VALIDITY_MARGINAL:
    case GPGME_VALIDITY_NEVER:
    case GPGME_VALIDITY_UNDEFINED:
    case GPGME_VALIDITY_UNKNOWN:
      is_strong = false;
      break;

    case GPGME_VALIDITY_FULL:
    case GPGME_VALIDITY_ULTIMATE:
      is_strong = true;
      break;
  }

  return is_strong;
}

/**
 * crypt_id_is_valid - Is key ID valid
 * @param key Key to test
 * @retval true Key is valid
 *
 * When the key is not marked as unusable
 */
int crypt_id_is_valid(struct CryptKeyInfo *key)
{
  if (!key)
    return 0;

  return !(key->flags & KEYFLAG_CANTUSE);
}

/**
 * crypt_id_matches_addr - Does the key ID match the address
 * @param addr   First email address
 * @param u_addr Second email address
 * @param key    Key to use
 * @retval num Flags, e.g. #CRYPT_KV_VALID
 *
 * Return a bit vector describing how well the addresses ADDR and U_ADDR match
 * and whether KEY is valid.
 */
static int crypt_id_matches_addr(struct Address *addr, struct Address *u_addr,
                                 struct CryptKeyInfo *key)
{
  int rc = 0;

  if (crypt_id_is_valid(key))
    rc |= CRYPT_KV_VALID;

  if (crypt_id_is_strong(key))
    rc |= CRYPT_KV_STRONGID;

  if (addr && u_addr)
  {
    if (addr->mailbox && u_addr->mailbox && buf_istr_equal(addr->mailbox, u_addr->mailbox))
    {
      rc |= CRYPT_KV_ADDR;
    }

    if (addr->personal && u_addr->personal &&
        buf_istr_equal(addr->personal, u_addr->personal))
    {
      rc |= CRYPT_KV_STRING;
    }
  }

  return rc;
}

/**
 * create_gpgme_context - Create a new GPGME context
 * @param for_smime If true, protocol of the context is set to CMS
 * @retval ptr New GPGME context
 */
gpgme_ctx_t create_gpgme_context(bool for_smime)
{
  gpgme_ctx_t ctx = NULL;

  gpgme_error_t err = gpgme_new(&ctx);

#ifdef USE_AUTOCRYPT
  const char *const c_autocrypt_dir = cs_subset_path(NeoMutt->sub, "autocrypt_dir");
  if (!err && OptAutocryptGpgme)
    err = gpgme_ctx_set_engine_info(ctx, GPGME_PROTOCOL_OpenPGP, NULL, c_autocrypt_dir);
#endif

  if (err != 0)
  {
    mutt_error(_("error creating GPGME context: %s"), gpgme_strerror(err));
    mutt_exit(1);
  }

  if (for_smime)
  {
    err = gpgme_set_protocol(ctx, GPGME_PROTOCOL_CMS);
    if (err != 0)
    {
      mutt_error(_("error enabling CMS protocol: %s"), gpgme_strerror(err));
      mutt_exit(1);
    }
  }

  return ctx;
}

/**
 * create_gpgme_data - Create a new GPGME data object
 * @retval ptr GPGPE data object
 *
 * This is a wrapper to die on error.
 *
 * @note Call gpgme_data_release() to free the data object
 */
static gpgme_data_t create_gpgme_data(void)
{
  gpgme_data_t data = NULL;

  gpgme_error_t err = gpgme_data_new(&data);
  if (err != 0)
  {
    mutt_error(_("error creating GPGME data object: %s"), gpgme_strerror(err));
    mutt_exit(1);
  }
  return data;
}

/**
 * body_to_data_object - Create GPGME object from the mail body
 * @param b       Body to use
 * @param convert If true, lines are converted to CR-LF if required
 * @retval ptr Newly created GPGME data object
 */
static gpgme_data_t body_to_data_object(struct Body *b, bool convert)
{
  int err = 0;
  gpgme_data_t data = NULL;

  struct Buffer *tempfile = buf_pool_get();
  buf_mktemp(tempfile);
  FILE *fp_tmp = mutt_file_fopen(buf_string(tempfile), "w+");
  if (!fp_tmp)
  {
    mutt_perror("%s", buf_string(tempfile));
    goto cleanup;
  }

  mutt_write_mime_header(b, fp_tmp, NeoMutt->sub);
  fputc('\n', fp_tmp);
  mutt_write_mime_body(b, fp_tmp, NeoMutt->sub);

  if (convert)
  {
    int c, hadcr = 0;
    unsigned char buf[1];

    data = create_gpgme_data();
    rewind(fp_tmp);
    while ((c = fgetc(fp_tmp)) != EOF)
    {
      if (c == '\r')
      {
        hadcr = 1;
      }
      else
      {
        if ((c == '\n') && !hadcr)
        {
          buf[0] = '\r';
          gpgme_data_write(data, buf, 1);
        }

        hadcr = 0;
      }
      /* FIXME: This is quite suboptimal */
      buf[0] = c;
      gpgme_data_write(data, buf, 1);
    }
    mutt_file_fclose(&fp_tmp);
    gpgme_data_seek(data, 0, SEEK_SET);
  }
  else
  {
    mutt_file_fclose(&fp_tmp);
    err = gpgme_data_new_from_file(&data, buf_string(tempfile), 1);
    if (err != 0)
    {
      mutt_error(_("error allocating data object: %s"), gpgme_strerror(err));
      gpgme_data_release(data);
      data = NULL;
      /* fall through to unlink the tempfile */
    }
  }
  unlink(buf_string(tempfile));

cleanup:
  buf_pool_release(&tempfile);
  return data;
}

/**
 * file_to_data_object - Create GPGME data object from file
 * @param fp     File to read from
 * @param offset Offset to start reading from
 * @param length Length of data to read
 * @retval ptr Newly created GPGME data object
 */
static gpgme_data_t file_to_data_object(FILE *fp, long offset, size_t length)
{
  gpgme_data_t data = NULL;

  int err = gpgme_data_new_from_filepart(&data, NULL, fp, offset, length);
  if (err != 0)
  {
    mutt_error(_("error allocating data object: %s"), gpgme_strerror(err));
    return NULL;
  }

  return data;
}

/**
 * data_object_to_stream - Write a GPGME data object to a file
 * @param data GPGME data object
 * @param fp   File to write to
 * @retval  0 Success
 * @retval -1 Error
 */
static int data_object_to_stream(gpgme_data_t data, FILE *fp)
{
  char buf[4096] = { 0 };
  ssize_t nread;

  int err = ((gpgme_data_seek(data, 0, SEEK_SET) == -1) ? gpgme_error_from_errno(errno) : 0);
  if (err != 0)
  {
    mutt_error(_("error rewinding data object: %s"), gpgme_strerror(err));
    return -1;
  }

  while ((nread = gpgme_data_read(data, buf, sizeof(buf))) > 0)
  {
    /* fixme: we are not really converting CRLF to LF but just
     * skipping CR. Doing it correctly needs a more complex logic */
    for (char *p = buf; nread; p++, nread--)
    {
      if (*p != '\r')
        putc(*p, fp);
    }

    if (ferror(fp))
    {
      mutt_perror(_("[tempfile]"));
      return -1;
    }
  }
  if (nread == -1)
  {
    mutt_error(_("error reading data object: %s"), strerror(errno));
    return -1;
  }
  return 0;
}

/**
 * data_object_to_tempfile - Copy a data object to a temporary file
 * @param[in]  data   GPGME data object
 * @param[out] fp_ret Temporary file
 * @retval ptr Name of temporary file
 *
 * If fp_ret is passed in, the file will be rewound, left open, and returned
 * via that parameter.
 *
 * @note The caller must free the returned file name
 */
static char *data_object_to_tempfile(gpgme_data_t data, FILE **fp_ret)
{
  ssize_t nread = 0;
  char *rv = NULL;
  struct Buffer *tempf = buf_pool_get();

  buf_mktemp(tempf);

  FILE *fp = mutt_file_fopen(buf_string(tempf), "w+");
  if (!fp)
  {
    mutt_perror(_("Can't create temporary file"));
    goto cleanup;
  }

  int err = ((gpgme_data_seek(data, 0, SEEK_SET) == -1) ? gpgme_error_from_errno(errno) : 0);
  if (err == 0)
  {
    char buf[4096] = { 0 };

    while ((nread = gpgme_data_read(data, buf, sizeof(buf))) > 0)
    {
      if (fwrite(buf, nread, 1, fp) != 1)
      {
        mutt_perror("%s", buf_string(tempf));
        mutt_file_fclose(&fp);
        unlink(buf_string(tempf));
        goto cleanup;
      }
    }
  }
  if (fp_ret)
    rewind(fp);
  else
    mutt_file_fclose(&fp);
  if (nread == -1)
  {
    mutt_error(_("error reading data object: %s"), gpgme_strerror(err));
    unlink(buf_string(tempf));
    mutt_file_fclose(&fp);
    goto cleanup;
  }
  if (fp_ret)
    *fp_ret = fp;
  rv = buf_strdup(tempf);

cleanup:
  buf_pool_release(&tempf);
  return rv;
}

/**
 * create_recipient_string - Create a string of recipients
 * @param keylist    Keys, space-separated
 * @param recpstring Buffer to store the recipients
 * @param use_smime  Use SMIME
 */
static void create_recipient_string(const char *keylist, struct Buffer *recpstring, int use_smime)
{
  unsigned int n = 0;

  const char *s = keylist;
  do
  {
    while (*s == ' ')
      s++;
    if (*s != '\0')
    {
      if (n == 0)
      {
        if (!use_smime)
          buf_addstr(recpstring, "--\n");
      }
      else
      {
        buf_addch(recpstring, '\n');
      }
      n++;

      while ((*s != '\0') && (*s != ' '))
        buf_addch(recpstring, *s++);
    }
  } while (*s != '\0');
}

/**
 * set_signer_from_address - Try to set the context's signer from the address
 * @param ctx       GPGME handle
 * @param address   Address to try to set as a signer
 * @param for_smime Use S/MIME
 * @retval true     Address was set as a signer
 * @retval false    Address could not be set as a signer
 */
static bool set_signer_from_address(gpgme_ctx_t ctx, const char *address, bool for_smime)
{
  gpgme_error_t err;
  gpgme_key_t key = NULL, key2 = NULL;
  gpgme_ctx_t listctx = create_gpgme_context(for_smime);
  err = gpgme_op_keylist_start(listctx, address, 1);
  if (err == 0)
    err = gpgme_op_keylist_next(listctx, &key);
  if (err)
  {
    gpgme_release(listctx);
    mutt_error(_("secret key '%s' not found: %s"), address, gpgme_strerror(err));
    return false;
  }

  char *fpr = "fpr1";
  if (key->subkeys)
    fpr = key->subkeys->fpr ? key->subkeys->fpr : key->subkeys->keyid;
  while (gpgme_op_keylist_next(listctx, &key2) == 0)
  {
    char *fpr2 = "fpr2";
    if (key2->subkeys)
      fpr2 = key2->subkeys->fpr ? key2->subkeys->fpr : key2->subkeys->keyid;
    if (!mutt_str_equal(fpr, fpr2))
    {
      gpgme_key_unref(key);
      gpgme_key_unref(key2);
      gpgme_release(listctx);
      mutt_error(_("ambiguous specification of secret key '%s'"), address);
      return false;
    }
    else
    {
      gpgme_key_unref(key2);
    }
  }
  gpgme_op_keylist_end(listctx);
  gpgme_release(listctx);

  gpgme_signers_clear(ctx);
  err = gpgme_signers_add(ctx, key);
  gpgme_key_unref(key);
  if (err)
  {
    mutt_error(_("error setting secret key '%s': %s"), address, gpgme_strerror(err));
    return false;
  }
  return true;
}

/**
 * set_signer - Make sure that the correct signer is set
 * @param ctx       GPGME handle
 * @param al        From AddressList
 * @param for_smime Use S/MIME
 * @retval  0 Success
 * @retval -1 Error
 */
static int set_signer(gpgme_ctx_t ctx, const struct AddressList *al, bool for_smime)
{
  const char *signid = NULL;

  const char *const c_smime_sign_as = cs_subset_string(NeoMutt->sub, "smime_sign_as");
  const char *const c_pgp_sign_as = cs_subset_string(NeoMutt->sub, "pgp_sign_as");
  const char *const c_pgp_default_key = cs_subset_string(NeoMutt->sub, "pgp_default_key");
  const char *const c_smime_default_key = cs_subset_string(NeoMutt->sub, "smime_default_key");
  if (for_smime)
    signid = c_smime_sign_as ? c_smime_sign_as : c_smime_default_key;
#ifdef USE_AUTOCRYPT
  else if (OptAutocryptGpgme)
    signid = AutocryptSignAs;
#endif
  else
    signid = c_pgp_sign_as ? c_pgp_sign_as : c_pgp_default_key;

  /* Try getting the signing key from config entries */
  if (signid && set_signer_from_address(ctx, signid, for_smime))
  {
    return 0;
  }

  /* Try getting the signing key from the From line */
  if (al)
  {
    struct Address *a;
    TAILQ_FOREACH(a, al, entries)
    {
      if (a->mailbox && set_signer_from_address(ctx, buf_string(a->mailbox), for_smime))
      {
        return 0;
      }
    }
  }

  return (!signid && !al) ? 0 : -1;
}

/**
 * set_pka_sig_notation - Set the signature notation
 * @param ctx GPGME context
 * @retval num GPGME error code, e.g. GPG_ERR_NO_ERROR
 */
static gpgme_error_t set_pka_sig_notation(gpgme_ctx_t ctx)
{
  gpgme_error_t err = gpgme_sig_notation_add(ctx, PKA_NOTATION_NAME, CurrentSender, 0);
  if (err)
  {
    mutt_error(_("error setting PKA signature notation: %s"), gpgme_strerror(err));
  }

  return err;
}

/**
 * encrypt_gpgme_object - Encrypt the GPGPME data object
 * @param plaintext       GPGME data object with plain text message
 * @param keylist         List of keys to encrypt to
 * @param use_smime       If true, use SMIME
 * @param combined_signed If true, sign and encrypt the message (PGP only)
 * @param from            The From header line
 * @retval ptr Name of temporary file containing encrypted text
 */
static char *encrypt_gpgme_object(gpgme_data_t plaintext, char *keylist, bool use_smime,
                                  bool combined_signed, const struct AddressList *from)
{
  gpgme_error_t err;
  gpgme_ctx_t ctx = NULL;
  gpgme_data_t ciphertext = NULL;
  char *outfile = NULL;

  struct Buffer *recpstring = buf_pool_get();
  create_recipient_string(keylist, recpstring, use_smime);
  if (buf_is_empty(recpstring))
  {
    buf_pool_release(&recpstring);
    return NULL;
  }

  ctx = create_gpgme_context(use_smime);
  if (!use_smime)
    gpgme_set_armor(ctx, 1);

  ciphertext = create_gpgme_data();

  if (combined_signed)
  {
    if (set_signer(ctx, from, use_smime))
      goto cleanup;

    const bool c_crypt_use_pka = cs_subset_bool(NeoMutt->sub, "crypt_use_pka");
    if (c_crypt_use_pka)
    {
      err = set_pka_sig_notation(ctx);
      if (err != 0)
        goto cleanup;
    }

    err = gpgme_op_encrypt_sign_ext(ctx, NULL, buf_string(recpstring),
                                    GPGME_ENCRYPT_ALWAYS_TRUST, plaintext, ciphertext);
  }
  else
  {
    err = gpgme_op_encrypt_ext(ctx, NULL, buf_string(recpstring),
                               GPGME_ENCRYPT_ALWAYS_TRUST, plaintext, ciphertext);
  }

  redraw_if_needed(ctx);
  if (err != 0)
  {
    mutt_error(_("error encrypting data: %s"), gpgme_strerror(err));
    goto cleanup;
  }

  outfile = data_object_to_tempfile(ciphertext, NULL);

cleanup:
  buf_pool_release(&recpstring);
  gpgme_release(ctx);
  gpgme_data_release(ciphertext);
  return outfile;
}

/**
 * get_micalg - Find the "micalg" parameter from the last GPGME operation
 * @param ctx       GPGME handle
 * @param use_smime If set, use SMIME instead of PGP
 * @param buf       Buffer for the result
 * @param buflen    Length of buffer
 * @retval  0 Success
 * @retval -1 Error
 *
 * Find the "Message Integrity Check algorithm" from the last GPGME operation.
 * It is expected that this operation was a sign operation.
 */
static int get_micalg(gpgme_ctx_t ctx, int use_smime, char *buf, size_t buflen)
{
  gpgme_sign_result_t result = NULL;
  const char *algorithm_name = NULL;

  if (buflen < 5)
    return -1;

  *buf = '\0';
  result = gpgme_op_sign_result(ctx);
  if (result && result->signatures)
  {
    algorithm_name = gpgme_hash_algo_name(result->signatures->hash_algo);
    if (algorithm_name)
    {
      if (use_smime)
      {
        /* convert GPGME raw hash name to RFC2633 format */
        snprintf(buf, buflen, "%s", algorithm_name);
        mutt_str_lower(buf);
      }
      else
      {
        /* convert GPGME raw hash name to RFC3156 format */
        snprintf(buf, buflen, "pgp-%s", algorithm_name);
        mutt_str_lower(buf + 4);
      }
    }
  }

  return (buf[0] != '\0') ? 0 : -1;
}

/**
 * print_time - Print the date/time according to the locale
 * @param t     Timestamp
 * @param state State to write to
 */
static void print_time(time_t t, struct State *state)
{
  char p[256] = { 0 };
  mutt_date_localtime_format(p, sizeof(p), nl_langinfo(D_T_FMT), t);
  state_puts(state, p);
}

/**
 * sign_message - Sign a message
 * @param b         Message to sign
 * @param from      The From header line
 * @param use_smime If set, use SMIME instead of PGP
 * @retval ptr  new Body
 * @retval NULL error
 */
static struct Body *sign_message(struct Body *b, const struct AddressList *from, bool use_smime)
{
  struct Body *b_sign = NULL;
  char *sigfile = NULL;
  int err = 0;
  char buf[100] = { 0 };
  gpgme_ctx_t ctx = NULL;
  gpgme_data_t message = NULL, signature = NULL;
  gpgme_sign_result_t sigres = NULL;

  crypt_convert_to_7bit(b); /* Signed data _must_ be in 7-bit format. */

  message = body_to_data_object(b, true);
  if (!message)
    return NULL;
  signature = create_gpgme_data();

  ctx = create_gpgme_context(use_smime);
  if (!use_smime)
    gpgme_set_armor(ctx, 1);

  if (set_signer(ctx, from, use_smime))
  {
    gpgme_data_release(signature);
    gpgme_data_release(message);
    gpgme_release(ctx);
    return NULL;
  }

  const bool c_crypt_use_pka = cs_subset_bool(NeoMutt->sub, "crypt_use_pka");
  if (c_crypt_use_pka)
  {
    err = set_pka_sig_notation(ctx);
    if (err != 0)
    {
      gpgme_data_release(signature);
      gpgme_data_release(message);
      gpgme_release(ctx);
      return NULL;
    }
  }

  err = gpgme_op_sign(ctx, message, signature, GPGME_SIG_MODE_DETACH);
  redraw_if_needed(ctx);
  gpgme_data_release(message);
  if (err != 0)
  {
    gpgme_data_release(signature);
    gpgme_release(ctx);
    mutt_error(_("error signing data: %s"), gpgme_strerror(err));
    return NULL;
  }
  /* Check for zero signatures generated.  This can occur when $pgp_sign_as is
   * unset and there is no default key specified in ~/.gnupg/gpg.conf */
  sigres = gpgme_op_sign_result(ctx);
  if (!sigres->signatures)
  {
    gpgme_data_release(signature);
    gpgme_release(ctx);
    mutt_error(_("$pgp_sign_as unset and no default key specified in ~/.gnupg/gpg.conf"));
    return NULL;
  }

  sigfile = data_object_to_tempfile(signature, NULL);
  gpgme_data_release(signature);
  if (!sigfile)
  {
    gpgme_release(ctx);
    return NULL;
  }

  b_sign = mutt_body_new();
  b_sign->type = TYPE_MULTIPART;
  b_sign->subtype = mutt_str_dup("signed");
  b_sign->encoding = ENC_7BIT;
  b_sign->use_disp = false;
  b_sign->disposition = DISP_INLINE;

  mutt_generate_boundary(&b_sign->parameter);
  mutt_param_set(&b_sign->parameter, "protocol",
                 use_smime ? "application/pkcs7-signature" : "application/pgp-signature");
  /* Get the micalg from GPGME.  Old gpgme versions don't support this
   * for S/MIME so we assume sha-1 in this case. */
  if (get_micalg(ctx, use_smime, buf, sizeof(buf)) == 0)
    mutt_param_set(&b_sign->parameter, "micalg", buf);
  else if (use_smime)
    mutt_param_set(&b_sign->parameter, "micalg", "sha1");
  gpgme_release(ctx);

  b_sign->parts = b;
  b = b_sign;

  b_sign->parts->next = mutt_body_new();
  b_sign = b_sign->parts->next;
  b_sign->type = TYPE_APPLICATION;
  if (use_smime)
  {
    b_sign->subtype = mutt_str_dup("pkcs7-signature");
    mutt_param_set(&b_sign->parameter, "name", "smime.p7s");
    b_sign->encoding = ENC_BASE64;
    b_sign->use_disp = true;
    b_sign->disposition = DISP_ATTACH;
    b_sign->d_filename = mutt_str_dup("smime.p7s");
  }
  else
  {
    b_sign->subtype = mutt_str_dup("pgp-signature");
    mutt_param_set(&b_sign->parameter, "name", "signature.asc");
    b_sign->use_disp = false;
    b_sign->disposition = DISP_NONE;
    b_sign->encoding = ENC_7BIT;
  }
  b_sign->filename = sigfile;
  b_sign->unlink = true; /* ok to remove this file after sending. */

  return b;
}

/**
 * pgp_gpgme_sign_message - Cryptographically sign the Body of a message - Implements CryptModuleSpecs::sign_message() - @ingroup crypto_sign_message
 */
struct Body *pgp_gpgme_sign_message(struct Body *b, const struct AddressList *from)
{
  return sign_message(b, from, false);
}

/**
 * smime_gpgme_sign_message - Cryptographically sign the Body of a message - Implements CryptModuleSpecs::sign_message() - @ingroup crypto_sign_message
 */
struct Body *smime_gpgme_sign_message(struct Body *b, const struct AddressList *from)
{
  return sign_message(b, from, true);
}

/**
 * pgp_gpgme_encrypt_message - PGP encrypt an email - Implements CryptModuleSpecs::pgp_encrypt_message() - @ingroup crypto_pgp_encrypt_message
 */
struct Body *pgp_gpgme_encrypt_message(struct Body *b, char *keylist, bool sign,
                                       const struct AddressList *from)
{
  if (sign)
    crypt_convert_to_7bit(b);
  gpgme_data_t plaintext = body_to_data_object(b, false);
  if (!plaintext)
    return NULL;

  char *outfile = encrypt_gpgme_object(plaintext, keylist, false, sign, from);
  gpgme_data_release(plaintext);
  if (!outfile)
    return NULL;

  struct Body *b_enc = mutt_body_new();
  b_enc->type = TYPE_MULTIPART;
  b_enc->subtype = mutt_str_dup("encrypted");
  b_enc->encoding = ENC_7BIT;
  b_enc->use_disp = false;
  b_enc->disposition = DISP_INLINE;

  mutt_generate_boundary(&b_enc->parameter);
  mutt_param_set(&b_enc->parameter, "protocol", "application/pgp-encrypted");

  b_enc->parts = mutt_body_new();
  b_enc->parts->type = TYPE_APPLICATION;
  b_enc->parts->subtype = mutt_str_dup("pgp-encrypted");
  b_enc->parts->encoding = ENC_7BIT;

  b_enc->parts->next = mutt_body_new();
  b_enc->parts->next->type = TYPE_APPLICATION;
  b_enc->parts->next->subtype = mutt_str_dup("octet-stream");
  b_enc->parts->next->encoding = ENC_7BIT;
  b_enc->parts->next->filename = outfile;
  b_enc->parts->next->use_disp = true;
  b_enc->parts->next->disposition = DISP_ATTACH;
  b_enc->parts->next->unlink = true; /* delete after sending the message */
  b_enc->parts->next->d_filename = mutt_str_dup("msg.asc"); /* non pgp/mime
                                                           can save */

  return b_enc;
}

/**
 * smime_gpgme_build_smime_entity - Encrypt the email body to all recipients - Implements CryptModuleSpecs::smime_build_smime_entity() - @ingroup crypto_smime_build_smime_entity
 */
struct Body *smime_gpgme_build_smime_entity(struct Body *b, char *keylist)
{
  /* OpenSSL converts line endings to crlf when encrypting.  Some clients
   * depend on this for signed+encrypted messages: they do not convert line
   * endings between decrypting and checking the signature.  */
  gpgme_data_t plaintext = body_to_data_object(b, true);
  if (!plaintext)
    return NULL;

  char *outfile = encrypt_gpgme_object(plaintext, keylist, true, false, NULL);
  gpgme_data_release(plaintext);
  if (!outfile)
    return NULL;

  struct Body *b_enc = mutt_body_new();
  b_enc->type = TYPE_APPLICATION;
  b_enc->subtype = mutt_str_dup("pkcs7-mime");
  mutt_param_set(&b_enc->parameter, "name", "smime.p7m");
  mutt_param_set(&b_enc->parameter, "smime-type", "enveloped-data");
  b_enc->encoding = ENC_BASE64; /* The output of OpenSSL SHOULD be binary */
  b_enc->use_disp = true;
  b_enc->disposition = DISP_ATTACH;
  b_enc->d_filename = mutt_str_dup("smime.p7m");
  b_enc->filename = outfile;
  b_enc->unlink = true; /* delete after sending the message */
  b_enc->parts = 0;
  b_enc->next = 0;

  return b_enc;
}

/**
 * show_sig_summary - Show a signature summary
 * @param sum Flags, e.g. GPGME_SIGSUM_KEY_REVOKED
 * @param ctx   GPGME handle
 * @param key   Set of keys
 * @param idx   Index into key set
 * @param state State to use
 * @param sig   GPGME signature
 * @retval 0 Success
 * @retval 1 There is a severe warning
 *
 * Display the common attributes of the signature summary SUM.
 */
static int show_sig_summary(unsigned long sum, gpgme_ctx_t ctx, gpgme_key_t key,
                            int idx, struct State *state, gpgme_signature_t sig)
{
  if (!key)
    return 1;

  bool severe = false;

  if ((sum & GPGME_SIGSUM_KEY_REVOKED))
  {
    state_puts(state, _("Warning: One of the keys has been revoked\n"));
    severe = true;
  }

  if ((sum & GPGME_SIGSUM_KEY_EXPIRED))
  {
    time_t at = key->subkeys->expires ? key->subkeys->expires : 0;
    if (at)
    {
      state_puts(state, _("Warning: The key used to create the signature expired at: "));
      print_time(at, state);
      state_puts(state, "\n");
    }
    else
    {
      state_puts(state, _("Warning: At least one certification key has expired\n"));
    }
  }

  if ((sum & GPGME_SIGSUM_SIG_EXPIRED))
  {
    gpgme_signature_t sig2 = NULL;
    unsigned int i;

    gpgme_verify_result_t result = gpgme_op_verify_result(ctx);

    for (sig2 = result->signatures, i = 0; sig2 && (i < idx); sig2 = sig2->next, i++)
      ; // do nothing

    state_puts(state, _("Warning: The signature expired at: "));
    print_time(sig2 ? sig2->exp_timestamp : 0, state);
    state_puts(state, "\n");
  }

  if ((sum & GPGME_SIGSUM_KEY_MISSING))
  {
    state_puts(state, _("Can't verify due to a missing key or certificate\n"));
  }

  if ((sum & GPGME_SIGSUM_CRL_MISSING))
  {
    state_puts(state, _("The CRL is not available\n"));
    severe = true;
  }

  if ((sum & GPGME_SIGSUM_CRL_TOO_OLD))
  {
    state_puts(state, _("Available CRL is too old\n"));
    severe = true;
  }

  if ((sum & GPGME_SIGSUM_BAD_POLICY))
    state_puts(state, _("A policy requirement was not met\n"));

  if ((sum & GPGME_SIGSUM_SYS_ERROR))
  {
    const char *t0 = NULL, *t1 = NULL;
    gpgme_verify_result_t result = NULL;
    gpgme_signature_t sig2 = NULL;
    unsigned int i;

    state_puts(state, _("A system error occurred"));

    /* Try to figure out some more detailed system error information. */
    result = gpgme_op_verify_result(ctx);
    for (sig2 = result->signatures, i = 0; sig2 && (i < idx); sig2 = sig2->next, i++)
      ; // do nothing

    if (sig2)
    {
      t0 = "";
      t1 = sig2->wrong_key_usage ? "Wrong_Key_Usage" : "";
    }

    if (t0 || t1)
    {
      state_puts(state, ": ");
      if (t0)
        state_puts(state, t0);
      if (t1 && !(t0 && (mutt_str_equal(t0, t1))))
      {
        if (t0)
          state_puts(state, ",");
        state_puts(state, t1);
      }
    }
    state_puts(state, "\n");
  }

  const bool c_crypt_use_pka = cs_subset_bool(NeoMutt->sub, "crypt_use_pka");
  if (c_crypt_use_pka)
  {
    if ((sig->pka_trust == 1) && sig->pka_address)
    {
      state_puts(state, _("WARNING: PKA entry does not match signer's address: "));
      state_puts(state, sig->pka_address);
      state_puts(state, "\n");
    }
    else if ((sig->pka_trust == 2) && sig->pka_address)
    {
      state_puts(state, _("PKA verified signer's address is: "));
      state_puts(state, sig->pka_address);
      state_puts(state, "\n");
    }
  }

  return severe;
}

/**
 * show_fingerprint - Write a key's fingerprint
 * @param key   GPGME key
 * @param state State to write to
 */
static void show_fingerprint(gpgme_key_t key, struct State *state)
{
  if (!key)
    return;

  const char *prefix = _("Fingerprint: ");

  const char *s = key->subkeys ? key->subkeys->fpr : NULL;
  if (!s)
    return;
  bool is_pgp = (key->protocol == GPGME_PROTOCOL_OpenPGP);

  char *buf = mutt_mem_malloc(strlen(prefix) + strlen(s) * 4 + 2);
  strcpy(buf, prefix);
  char *p = buf + strlen(buf);
  if (is_pgp && (strlen(s) == 40))
  { /* PGP v4 style formatted. */
    for (int i = 0; *s && s[1] && s[2] && s[3] && s[4]; s += 4, i++)
    {
      *p++ = s[0];
      *p++ = s[1];
      *p++ = s[2];
      *p++ = s[3];
      *p++ = ' ';
      if (i == 4)
        *p++ = ' ';
    }
  }
  else
  {
    for (int i = 0; *s && s[1] && s[2]; s += 2, i++)
    {
      *p++ = s[0];
      *p++ = s[1];
      *p++ = is_pgp ? ' ' : ':';
      if (is_pgp && (i == 7))
        *p++ = ' ';
    }
  }

  /* just in case print remaining odd digits */
  for (; *s; s++)
    *p++ = *s;
  *p++ = '\n';
  *p = '\0';
  state_puts(state, buf);
  FREE(&buf);
}

/**
 * show_one_sig_validity - Show the validity of a key used for one signature
 * @param ctx   GPGME handle
 * @param idx   Index of signature to check
 * @param state State to use
 */
static void show_one_sig_validity(gpgme_ctx_t ctx, int idx, struct State *state)
{
  gpgme_signature_t sig = NULL;
  const char *txt = NULL;

  gpgme_verify_result_t result = gpgme_op_verify_result(ctx);
  if (result)
    for (sig = result->signatures; sig && (idx > 0); sig = sig->next, idx--)
      ; // do nothing

  switch (sig ? sig->validity : 0)
  {
    case GPGME_VALIDITY_UNKNOWN:
      txt = _("WARNING: We have NO indication whether the key belongs to the person named as shown above\n");
      break;
    case GPGME_VALIDITY_UNDEFINED:
      break;
    case GPGME_VALIDITY_NEVER:
      txt = _("WARNING: The key does NOT BELONG to the person named as shown above\n");
      break;
    case GPGME_VALIDITY_MARGINAL:
      txt = _("WARNING: It is NOT certain that the key belongs to the person named as shown above\n");
      break;
    case GPGME_VALIDITY_FULL:
    case GPGME_VALIDITY_ULTIMATE:
      txt = NULL;
      break;
  }
  if (txt)
    state_puts(state, txt);
}

/**
 * print_smime_keyinfo - Print key info about an SMIME key
 * @param msg   Prefix message to write
 * @param sig   GPGME signature
 * @param key   GPGME key
 * @param state State to write to
 */
static void print_smime_keyinfo(const char *msg, gpgme_signature_t sig,
                                gpgme_key_t key, struct State *state)
{
  int msgwid;

  state_puts(state, msg);
  state_puts(state, " ");
  /* key is NULL when not present in the user's keyring */
  if (key)
  {
    bool aka = false;
    for (gpgme_user_id_t uids = key->uids; uids; uids = uids->next)
    {
      if (uids->revoked)
        continue;
      if (aka)
      {
        msgwid = mutt_strwidth(msg) - mutt_strwidth(_("aka: ")) + 1;
        if (msgwid < 0)
          msgwid = 0;
        for (int i = 0; i < msgwid; i++)
          state_puts(state, " ");
        state_puts(state, _("aka: "));
      }
      state_puts(state, uids->uid);
      state_puts(state, "\n");

      aka = true;
    }
  }
  else
  {
    if (sig->fpr)
    {
      state_puts(state, _("KeyID "));
      state_puts(state, sig->fpr);
    }
    else
    {
      /* L10N: You will see this message in place of "KeyID "
         if the S/MIME key has no ID. This is quite an error. */
      state_puts(state, _("no signature fingerprint available"));
    }
    state_puts(state, "\n");
  }

  /* timestamp is 0 when verification failed.
   * "Jan 1 1970" is not the created date. */
  if (sig->timestamp)
  {
    msgwid = mutt_strwidth(msg) - mutt_strwidth(_("created: ")) + 1;
    if (msgwid < 0)
      msgwid = 0;
    for (int i = 0; i < msgwid; i++)
      state_puts(state, " ");
    state_puts(state, _("created: "));
    print_time(sig->timestamp, state);
    state_puts(state, "\n");
  }
}

/**
 * show_one_sig_status - Show information about one signature
 * @param ctx   GPGME handle of a successful verification
 * @param idx   Index
 * @param state State to use
 * @retval  0 Normal procession
 * @retval  1 A bad signature
 * @retval  2 A signature with a warning
 * @retval -1 No more signature
 *
 * The index should start at 0 and increment for each call/signature.
 */
static int show_one_sig_status(gpgme_ctx_t ctx, int idx, struct State *state)
{
  const char *fpr = NULL;
  gpgme_key_t key = NULL;
  bool anybad = false, anywarn = false;
  gpgme_signature_t sig = NULL;
  gpgme_error_t err = GPG_ERR_NO_ERROR;

  gpgme_verify_result_t result = gpgme_op_verify_result(ctx);
  if (result)
  {
    /* FIXME: this code should use a static variable and remember
     * the current position in the list of signatures, IMHO.
     * -moritz.  */
    int i;
    for (i = 0, sig = result->signatures; sig && (i < idx); i++, sig = sig->next)
      ; // do nothing

    if (!sig)
      return -1; /* Signature not found.  */

    if (SignatureKey)
    {
      gpgme_key_unref(SignatureKey);
      SignatureKey = NULL;
    }

    fpr = sig->fpr;
    const unsigned int sum = sig->summary;

    if (gpg_err_code(sig->status) != GPG_ERR_NO_ERROR)
      anybad = true;

    if (gpg_err_code(sig->status) != GPG_ERR_NO_PUBKEY)
    {
      err = gpgme_get_key(ctx, fpr, &key, 0); /* secret key?  */
      if (err == 0)
      {
        if (!SignatureKey)
          SignatureKey = key;
      }
      else
      {
        key = NULL; /* Old GPGME versions did not set KEY to NULL on
                         error.   Do it here to avoid a double free. */
      }
    }
    else
    {
      /* pubkey not present */
    }

    if (!state || !state->fp_out || !(state->flags & STATE_DISPLAY))
    {
      ; /* No state information so no way to print anything. */
    }
    else if (err != 0)
    {
      char buf[1024] = { 0 };
      snprintf(buf, sizeof(buf), _("Error getting key information for KeyID %s: %s\n"),
               fpr, gpgme_strerror(err));
      state_puts(state, buf);
      anybad = true;
    }
    else if ((sum & GPGME_SIGSUM_GREEN))
    {
      print_smime_keyinfo(_("Good signature from:"), sig, key, state);
      if (show_sig_summary(sum, ctx, key, idx, state, sig))
        anywarn = true;
      show_one_sig_validity(ctx, idx, state);
    }
    else if ((sum & GPGME_SIGSUM_RED))
    {
      print_smime_keyinfo(_("*BAD* signature from:"), sig, key, state);
      show_sig_summary(sum, ctx, key, idx, state, sig);
    }
    else if (!anybad && key && (key->protocol == GPGME_PROTOCOL_OpenPGP))
    { /* We can't decide (yellow) but this is a PGP key with a good
           signature, so we display what a PGP user expects: The name,
           fingerprint and the key validity (which is neither fully or
           ultimate). */
      print_smime_keyinfo(_("Good signature from:"), sig, key, state);
      show_one_sig_validity(ctx, idx, state);
      show_fingerprint(key, state);
      if (show_sig_summary(sum, ctx, key, idx, state, sig))
        anywarn = true;
    }
    else /* can't decide (yellow) */
    {
      print_smime_keyinfo(_("Problem signature from:"), sig, key, state);
      /* 0 indicates no expiration */
      if (sig->exp_timestamp)
      {
        /* L10N: This is trying to match the width of the
           "Problem signature from:" translation just above. */
        state_puts(state, _("               expires: "));
        print_time(sig->exp_timestamp, state);
        state_puts(state, "\n");
      }
      show_sig_summary(sum, ctx, key, idx, state, sig);
      anywarn = true;
    }

    if (key != SignatureKey)
      gpgme_key_unref(key);
  }

  return anybad ? 1 : anywarn ? 2 : 0;
}

/**
 * verify_one - Do the actual verification step
 * @param b        Body part containing signature
 * @param state    State to read from
 * @param tempfile Temporary file to read
 * @param is_smime Is the key S/MIME?
 * @retval  0 Success
 * @retval  1 Bad signature
 * @retval  2 Warnings
 * @retval -1 Error
 *
 * With is_smime set to true we assume S/MIME.
 */
static int verify_one(struct Body *b, struct State *state, const char *tempfile, bool is_smime)
{
  int badsig = -1;
  int anywarn = 0;
  gpgme_ctx_t ctx = NULL;
  gpgme_data_t message = NULL;

  gpgme_data_t signature = file_to_data_object(state->fp_in, b->offset, b->length);
  if (!signature)
    return -1;

  /* We need to tell GPGME about the encoding because the backend can't
   * auto-detect plain base-64 encoding which is used by S/MIME. */
  if (is_smime)
    gpgme_data_set_encoding(signature, GPGME_DATA_ENCODING_BASE64);

  int err = gpgme_data_new_from_file(&message, tempfile, 1);
  if (err != 0)
  {
    gpgme_data_release(signature);
    mutt_error(_("error allocating data object: %s"), gpgme_strerror(err));
    return -1;
  }
  ctx = create_gpgme_context(is_smime);

  /* Note: We don't need a current time output because GPGME avoids
   * such an attack by separating the meta information from the data. */
  state_attach_puts(state, _("[-- Begin signature information --]\n"));

  err = gpgme_op_verify(ctx, signature, message, NULL);
  gpgme_data_release(message);
  gpgme_data_release(signature);

  redraw_if_needed(ctx);
  if (err != 0)
  {
    char buf[200] = { 0 };

    snprintf(buf, sizeof(buf) - 1, _("Error: verification failed: %s\n"),
             gpgme_strerror(err));
    state_puts(state, buf);
  }
  else
  { /* Verification succeeded, see what the result is. */
    gpgme_verify_result_t verify_result = NULL;

    if (SignatureKey)
    {
      gpgme_key_unref(SignatureKey);
      SignatureKey = NULL;
    }

    verify_result = gpgme_op_verify_result(ctx);
    if (verify_result && verify_result->signatures)
    {
      bool anybad = false;
      int res;
      for (int idx = 0; (res = show_one_sig_status(ctx, idx, state)) != -1; idx++)
      {
        if (res == 1)
          anybad = true;
        else if (res == 2)
          anywarn = 2;
      }
      if (!anybad)
        badsig = 0;
    }
  }

  if (badsig == 0)
  {
    gpgme_verify_result_t result = NULL;
    gpgme_sig_notation_t notation = NULL;
    gpgme_signature_t sig = NULL;

    result = gpgme_op_verify_result(ctx);
    if (result)
    {
      for (sig = result->signatures; sig; sig = sig->next)
      {
        int non_pka_notations = 0;
        for (notation = sig->notations; notation; notation = notation->next)
          if (!is_pka_notation(notation))
            non_pka_notations++;

        if (non_pka_notations)
        {
          char buf[128] = { 0 };
          snprintf(buf, sizeof(buf),
                   _("*** Begin Notation (signature by: %s) ***\n"), sig->fpr);
          state_puts(state, buf);
          for (notation = sig->notations; notation; notation = notation->next)
          {
            if (is_pka_notation(notation))
              continue;

            if (notation->name)
            {
              state_puts(state, notation->name);
              state_puts(state, "=");
            }
            if (notation->value)
            {
              state_puts(state, notation->value);
              if (!(*notation->value && (notation->value[strlen(notation->value) - 1] == '\n')))
                state_puts(state, "\n");
            }
          }
          state_puts(state, _("*** End Notation ***\n"));
        }
      }
    }
  }

  gpgme_release(ctx);

  state_attach_puts(state, _("[-- End signature information --]\n\n"));
  mutt_debug(LL_DEBUG1, "returning %d\n", badsig);

  return badsig ? 1 : anywarn ? 2 : 0;
}

/**
 * pgp_gpgme_verify_one - Check a signed MIME part against a signature - Implements CryptModuleSpecs::verify_one() - @ingroup crypto_verify_one
 */
int pgp_gpgme_verify_one(struct Body *b, struct State *state, const char *tempfile)
{
  return verify_one(b, state, tempfile, false);
}

/**
 * smime_gpgme_verify_one - Check a signed MIME part against a signature - Implements CryptModuleSpecs::verify_one() - @ingroup crypto_verify_one
 */
int smime_gpgme_verify_one(struct Body *b, struct State *state, const char *tempfile)
{
  return verify_one(b, state, tempfile, true);
}

/**
 * decrypt_part - Decrypt a PGP or SMIME message
 * @param[in]  b           Body of message
 * @param[in]  state       State to use
 * @param[in]  fp_out      File to write to
 * @param[in]  is_smime    True if an SMIME message
 * @param[out] r_is_signed Flag, R_IS_SIGNED (PGP only)
 * @retval ptr Newly allocated Body
 *
 * For PGP returns a flag in R_IS_SIGNED to indicate whether this is a combined
 * encrypted and signed message, for S/MIME it returns true when it is not a
 * encrypted but a signed message.
 */
static struct Body *decrypt_part(struct Body *b, struct State *state,
                                 FILE *fp_out, bool is_smime, int *r_is_signed)
{
  if (!b || !state || !fp_out)
    return NULL;

  struct Body *tattach = NULL;
  int err = 0;
  gpgme_data_t ciphertext = NULL, plaintext = NULL;
  bool maybe_signed = false;
  bool anywarn = false;
  int sig_stat = 0;

  if (r_is_signed)
    *r_is_signed = 0;

  gpgme_ctx_t ctx = NULL;
restart:
  ctx = create_gpgme_context(is_smime);

  if (b->length < 0)
    return NULL;
  /* Make a data object from the body, create context etc. */
  ciphertext = file_to_data_object(state->fp_in, b->offset, b->length);
  if (!ciphertext)
    goto cleanup;
  plaintext = create_gpgme_data();

  /* Do the decryption or the verification in case of the S/MIME hack. */
  if ((!is_smime) || maybe_signed)
  {
    if (!is_smime)
      err = gpgme_op_decrypt_verify(ctx, ciphertext, plaintext);
    else if (maybe_signed)
      err = gpgme_op_verify(ctx, ciphertext, NULL, plaintext);

    if (err == GPG_ERR_NO_ERROR)
    {
      /* Check whether signatures have been verified.  */
      gpgme_verify_result_t verify_result = gpgme_op_verify_result(ctx);
      if (verify_result->signatures)
        sig_stat = 1;
    }
  }
  else
  {
    err = gpgme_op_decrypt(ctx, ciphertext, plaintext);
  }
  gpgme_data_release(ciphertext);
  ciphertext = NULL;
  if (err != 0)
  {
#ifdef USE_AUTOCRYPT
    /* Abort right away and silently.
     * Autocrypt will retry on the normal keyring. */
    if (OptAutocryptGpgme)
      goto cleanup;
#endif
    if (is_smime && !maybe_signed && (gpg_err_code(err) == GPG_ERR_NO_DATA))
    {
      /* Check whether this might be a signed message despite what the mime
       * header told us.  Retry then.  gpgsm returns the error information
       * "unsupported Algorithm '?'" but GPGME will not store this unknown
       * algorithm, thus we test that it has not been set. */
      gpgme_decrypt_result_t result = NULL;

      result = gpgme_op_decrypt_result(ctx);
      if (!result->unsupported_algorithm)
      {
        maybe_signed = true;
        gpgme_data_release(plaintext);
        plaintext = NULL;
        /* gpgsm ends the session after an error; restart it */
        gpgme_release(ctx);
        goto restart;
      }
    }
    redraw_if_needed(ctx);
    if ((state->flags & STATE_DISPLAY))
    {
      char buf[200] = { 0 };

      snprintf(buf, sizeof(buf) - 1,
               _("[-- Error: decryption failed: %s --]\n\n"), gpgme_strerror(err));
      state_attach_puts(state, buf);
    }
    goto cleanup;
  }
  redraw_if_needed(ctx);

  /* Read the output from GPGME, and make sure to change CRLF to LF,
   * otherwise read_mime_header has a hard time parsing the message.  */
  if (data_object_to_stream(plaintext, fp_out))
  {
    goto cleanup;
  }
  gpgme_data_release(plaintext);
  plaintext = NULL;

  if (sig_stat)
  {
    int res, idx;
    int anybad = 0;

    if (r_is_signed)
      *r_is_signed = -1; /* A signature exists. */

    if ((state->flags & STATE_DISPLAY))
    {
      state_attach_puts(state, _("[-- Begin signature information --]\n"));
    }
    for (idx = 0; (res = show_one_sig_status(ctx, idx, state)) != -1; idx++)
    {
      if (res == 1)
        anybad = 1;
      else if (res == 2)
        anywarn = true;
    }
    if (!anybad && idx && r_is_signed && *r_is_signed)
      *r_is_signed = anywarn ? 2 : 1; /* Good signature. */

    if ((state->flags & STATE_DISPLAY))
    {
      state_attach_puts(state, _("[-- End signature information --]\n\n"));
    }
  }
  gpgme_release(ctx);
  ctx = NULL;

  fflush(fp_out);
  rewind(fp_out);
  const long size = mutt_file_get_size_fp(fp_out);
  if (size == 0)
  {
    goto cleanup;
  }
  tattach = mutt_read_mime_header(fp_out, 0);
  if (tattach)
  {
    /* Need to set the length of this body part.  */
    tattach->length = size - tattach->offset;

    tattach->warnsig = anywarn;

    /* See if we need to recurse on this MIME part.  */
    mutt_parse_part(fp_out, tattach);
  }

cleanup:
  gpgme_data_release(ciphertext);
  gpgme_data_release(plaintext);
  gpgme_release(ctx);

  return tattach;
}

/**
 * pgp_gpgme_decrypt_mime - Decrypt an encrypted MIME part - Implements CryptModuleSpecs::decrypt_mime() - @ingroup crypto_decrypt_mime
 */
int pgp_gpgme_decrypt_mime(FILE *fp_in, FILE **fp_out, struct Body *b, struct Body **b_dec)
{
  struct State state = { 0 };
  struct Body *first_part = b;
  int is_signed = 0;
  bool need_decode = false;
  LOFF_T saved_offset = 0;
  size_t saved_length = 0;
  FILE *fp_decoded = NULL;
  int rc = 0;

  first_part->goodsig = false;
  first_part->warnsig = false;

  if (mutt_is_valid_multipart_pgp_encrypted(b))
  {
    b = b->parts->next;
    /* Some clients improperly encode the octetstream part. */
    if (b->encoding != ENC_7BIT)
      need_decode = true;
  }
  else if (mutt_is_malformed_multipart_pgp_encrypted(b))
  {
    b = b->parts->next->next;
    need_decode = true;
  }
  else
  {
    return -1;
  }

  state.fp_in = fp_in;

  if (need_decode)
  {
    saved_offset = b->offset;
    saved_length = b->length;

    fp_decoded = mutt_file_mkstemp();
    if (!fp_decoded)
    {
      mutt_perror(_("Can't create temporary file"));
      return -1;
    }

    if (!mutt_file_seek(state.fp_in, b->offset, SEEK_SET))
    {
      rc = -1;
      goto bail;
    }
    state.fp_out = fp_decoded;

    mutt_decode_attachment(b, &state);

    fflush(fp_decoded);
    b->length = ftello(fp_decoded);
    b->offset = 0;
    rewind(fp_decoded);
    state.fp_in = fp_decoded;
    state.fp_out = 0;
  }

  *fp_out = mutt_file_mkstemp();
  if (!*fp_out)
  {
    mutt_perror(_("Can't create temporary file"));
    rc = -1;
    goto bail;
  }

  *b_dec = decrypt_part(b, &state, *fp_out, false, &is_signed);
  if (*b_dec)
  {
    rewind(*fp_out);
    if (is_signed > 0)
      first_part->goodsig = true;
  }
  else
  {
    rc = -1;
    mutt_file_fclose(fp_out);
  }

bail:
  if (need_decode)
  {
    b->length = saved_length;
    b->offset = saved_offset;
    mutt_file_fclose(&fp_decoded);
  }

  return rc;
}

/**
 * smime_gpgme_decrypt_mime - Decrypt an encrypted MIME part - Implements CryptModuleSpecs::decrypt_mime() - @ingroup crypto_decrypt_mime
 */
int smime_gpgme_decrypt_mime(FILE *fp_in, FILE **fp_out, struct Body *b, struct Body **b_dec)
{
  struct State state = { 0 };
  int is_signed;
  LOFF_T saved_b_offset;
  size_t saved_b_length;

  if (mutt_is_application_smime(b) == SEC_NO_FLAGS)
    return -1;

  if (b->parts)
    return -1;

  /* Decode the body - we need to pass binary CMS to the
   * backend.  The backend allows for Base64 encoded data but it does
   * not allow for QP which I have seen in some messages.  So better
   * do it here. */
  saved_b_offset = b->offset;
  saved_b_length = b->length;
  state.fp_in = fp_in;
  if (!mutt_file_seek(state.fp_in, b->offset, SEEK_SET))
  {
    return -1;
  }
  FILE *fp_tmp = mutt_file_mkstemp();
  if (!fp_tmp)
  {
    mutt_perror(_("Can't create temporary file"));
    return -1;
  }

  state.fp_out = fp_tmp;
  mutt_decode_attachment(b, &state);
  fflush(fp_tmp);
  b->length = ftello(state.fp_out);
  b->offset = 0;
  rewind(fp_tmp);

  memset(&state, 0, sizeof(state));
  state.fp_in = fp_tmp;
  state.fp_out = 0;
  *fp_out = mutt_file_mkstemp();
  if (!*fp_out)
  {
    mutt_perror(_("Can't create temporary file"));
    return -1;
  }

  *b_dec = decrypt_part(b, &state, *fp_out, true, &is_signed);
  if (*b_dec)
    (*b_dec)->goodsig = is_signed > 0;
  b->length = saved_b_length;
  b->offset = saved_b_offset;
  mutt_file_fclose(&fp_tmp);
  rewind(*fp_out);
  if (*b_dec && !is_signed && !(*b_dec)->parts && mutt_is_application_smime(*b_dec))
  {
    /* Assume that this is a opaque signed s/mime message.  This is an ugly way
     * of doing it but we have anyway a problem with arbitrary encoded S/MIME
     * messages: Only the outer part may be encrypted.  The entire mime parsing
     * should be revamped, probably by keeping the temporary files so that we
     * don't need to decrypt them all the time.  Inner parts of an encrypted
     * part can then point into this file and there won't ever be a need to
     * decrypt again.  This needs a partial rewrite of the MIME engine. */
    struct Body *bb = *b_dec;

    saved_b_offset = bb->offset;
    saved_b_length = bb->length;
    memset(&state, 0, sizeof(state));
    state.fp_in = *fp_out;
    if (!mutt_file_seek(state.fp_in, bb->offset, SEEK_SET))
    {
      return -1;
    }
    FILE *fp_tmp2 = mutt_file_mkstemp();
    if (!fp_tmp2)
    {
      mutt_perror(_("Can't create temporary file"));
      return -1;
    }

    state.fp_out = fp_tmp2;
    mutt_decode_attachment(bb, &state);
    fflush(fp_tmp2);
    bb->length = ftello(state.fp_out);
    bb->offset = 0;
    rewind(fp_tmp2);
    mutt_file_fclose(fp_out);

    memset(&state, 0, sizeof(state));
    state.fp_in = fp_tmp2;
    state.fp_out = 0;
    *fp_out = mutt_file_mkstemp();
    if (!*fp_out)
    {
      mutt_perror(_("Can't create temporary file"));
      return -1;
    }

    struct Body *b_tmp = decrypt_part(bb, &state, *fp_out, true, &is_signed);
    if (b_tmp)
      b_tmp->goodsig = is_signed > 0;
    bb->length = saved_b_length;
    bb->offset = saved_b_offset;
    mutt_file_fclose(&fp_tmp2);
    rewind(*fp_out);
    mutt_body_free(b_dec);
    *b_dec = b_tmp;
  }
  return *b_dec ? 0 : -1;
}

/**
 * pgp_gpgme_extract_keys - Write PGP keys to a file
 * @param[in]  keydata GPGME key data
 * @param[out] fp      Temporary file created with key info
 * @retval  0 Success
 * @retval -1 Error
 */
static int pgp_gpgme_extract_keys(gpgme_data_t keydata, FILE **fp)
{
  gpgme_ctx_t tmpctx = NULL;
  gpgme_error_t err;
  gpgme_key_t key = NULL;
  gpgme_user_id_t uid = NULL;
  gpgme_subkey_t subkey = NULL;
  const char *shortid = NULL;
  size_t len;
  char date[256] = { 0 };
  bool more;
  int rc = -1;
  time_t tt;

  *fp = mutt_file_mkstemp();
  if (!*fp)
  {
    mutt_perror(_("Can't create temporary file"));
    return -1;
  }

  tmpctx = create_gpgme_context(false);

  err = gpgme_op_keylist_from_data_start(tmpctx, keydata, 0);
  while (err == 0)
  {
    err = gpgme_op_keylist_next(tmpctx, &key);
    if (err != 0)
      break;
    uid = key->uids;
    subkey = key->subkeys;
    more = false;
    while (subkey)
    {
      shortid = subkey->keyid;
      len = mutt_str_len(subkey->keyid);
      if (len > 8)
        shortid += len - 8;
      tt = subkey->timestamp;
      mutt_date_localtime_format(date, sizeof(date), "%Y-%m-%d", tt);

      if (more)
      {
        fprintf(*fp, "sub %5.5s %u/%8s %s\n", gpgme_pubkey_algo_name(subkey->pubkey_algo),
                subkey->length, shortid, date);
      }
      else
      {
        fprintf(*fp, "pub %5.5s %u/%8s %s %s\n", gpgme_pubkey_algo_name(subkey->pubkey_algo),
                subkey->length, shortid, date, (uid ? uid->uid : ""));
      }
      subkey = subkey->next;
      more = true;
    }
    gpgme_key_unref(key);
  }
  if (gpg_err_code(err) != GPG_ERR_EOF)
  {
    mutt_debug(LL_DEBUG1, "Error listing keys\n");
    goto err_fp;
  }

  rc = 0;

err_fp:
  if (rc)
    mutt_file_fclose(fp);

  gpgme_release(tmpctx);

  return rc;
}

/**
 * line_compare - Compare two strings ignore line endings
 * @param a String a
 * @param n Maximum length to compare
 * @param b String b
 * @retval  0 Strings match
 * @retval -1 Strings differ
 *
 * Check that \a b is a complete line containing \a a followed by either LF or
 * CRLF.
 */
static int line_compare(const char *a, size_t n, const char *b)
{
  if (mutt_strn_equal(a, b, n))
  {
    /* at this point we know that 'b' is at least 'n' chars long */
    if ((b[n] == '\n') || ((b[n] == '\r') && (b[n + 1] == '\n')))
      return true;
  }
  return false;
}

/**
 * pgp_check_traditional_one_body - Check one inline PGP body part
 * @param fp File to read from
 * @param b  Body of the email
 * @retval true  Success
 * @retval false Error
 */
static int pgp_check_traditional_one_body(FILE *fp, struct Body *b)
{
  char buf[8192] = { 0 };
  bool rc = false;

  bool sgn = false;
  bool enc = false;

  if (b->type != TYPE_TEXT)
    return 0;

  struct Buffer *tempfile = buf_pool_get();
  buf_mktemp(tempfile);
  if (mutt_decode_save_attachment(fp, b, buf_string(tempfile), STATE_NO_FLAGS,
                                  MUTT_SAVE_NO_FLAGS) != 0)
  {
    unlink(buf_string(tempfile));
    goto cleanup;
  }

  FILE *fp_tmp = fopen(buf_string(tempfile), "r");
  if (!fp_tmp)
  {
    unlink(buf_string(tempfile));
    goto cleanup;
  }

  while (fgets(buf, sizeof(buf), fp_tmp))
  {
    size_t plen = mutt_str_startswith(buf, "-----BEGIN PGP ");
    if (plen != 0)
    {
      if (MESSAGE(buf + plen))
      {
        enc = true;
        break;
      }
      else if (SIGNED_MESSAGE(buf + plen))
      {
        sgn = true;
        break;
      }
    }
  }
  mutt_file_fclose(&fp_tmp);
  unlink(buf_string(tempfile));

  if (!enc && !sgn)
    goto cleanup;

  /* fix the content type */

  mutt_param_set(&b->parameter, "format", "fixed");
  mutt_param_set(&b->parameter, "x-action", enc ? "pgp-encrypted" : "pgp-signed");

  rc = true;

cleanup:
  buf_pool_release(&tempfile);
  return rc;
}

/**
 * pgp_gpgme_check_traditional - Look for inline (non-MIME) PGP content - Implements CryptModuleSpecs::pgp_check_traditional() - @ingroup crypto_pgp_check_traditional
 */
bool pgp_gpgme_check_traditional(FILE *fp, struct Body *b, bool just_one)
{
  bool rc = false;
  for (; b; b = b->next)
  {
    if (!just_one && is_multipart(b))
    {
      rc = (pgp_gpgme_check_traditional(fp, b->parts, false) || rc);
    }
    else if (b->type == TYPE_TEXT)
    {
      SecurityFlags r = mutt_is_application_pgp(b);
      if (r)
        rc = (rc || r);
      else
        rc = (pgp_check_traditional_one_body(fp, b) || rc);
    }

    if (just_one)
      break;
  }
  return rc;
}

/**
 * pgp_gpgme_invoke_import - Import a key from a message into the user's public key ring - Implements CryptModuleSpecs::pgp_invoke_import() - @ingroup crypto_pgp_invoke_import
 */
void pgp_gpgme_invoke_import(const char *fname)
{
  gpgme_ctx_t ctx = create_gpgme_context(false);
  gpgme_data_t keydata = NULL;
  gpgme_import_result_t impres = NULL;
  gpgme_import_status_t st = NULL;
  bool any;

  FILE *fp_in = mutt_file_fopen(fname, "r");
  if (!fp_in)
  {
    mutt_perror("%s", fname);
    goto leave;
  }
  /* Note that the stream, "fp_in", needs to be kept open while the keydata
   * is used.  */
  gpgme_error_t err = gpgme_data_new_from_stream(&keydata, fp_in);
  if (err != GPG_ERR_NO_ERROR)
  {
    mutt_error(_("error allocating data object: %s"), gpgme_strerror(err));
    goto leave;
  }

  err = gpgme_op_import(ctx, keydata);
  if (err != 0)
  {
    mutt_error(_("Error importing key: %s"), gpgme_strerror(err));
    goto leave;
  }

  /* Print infos about the imported keys to stdout.  */
  impres = gpgme_op_import_result(ctx);
  if (!impres)
  {
    fputs("oops: no import result returned\n", stdout);
    goto leave;
  }

  for (st = impres->imports; st; st = st->next)
  {
    if (st->result)
      continue;
    printf("key %s imported (", NONULL(st->fpr));
    /* Note that we use the singular even if it is possible that
     * several uids etc are new.  This simply looks better.  */
    any = false;
    if (st->status & GPGME_IMPORT_SECRET)
    {
      printf("secret parts");
      any = true;
    }
    if ((st->status & GPGME_IMPORT_NEW))
    {
      printf("%snew key", any ? ", " : "");
      any = true;
    }
    if ((st->status & GPGME_IMPORT_UID))
    {
      printf("%snew uid", any ? ", " : "");
      any = true;
    }
    if ((st->status & GPGME_IMPORT_SIG))
    {
      printf("%snew sig", any ? ", " : "");
      any = true;
    }
    if ((st->status & GPGME_IMPORT_SUBKEY))
    {
      printf("%snew subkey", any ? ", " : "");
      any = true;
    }
    printf("%s)\n", any ? "" : "not changed");
    /* Fixme: Should we lookup each imported key and print more infos? */
  }
  /* Now print keys which failed the import.  Unfortunately in most
   * cases gpg will bail out early and not tell GPGME about.  */
  /* FIXME: We could instead use the new GPGME_AUDITLOG_DIAG to show
   * the actual gpg diagnostics.  But I fear that would clutter the
   * output too much.  Maybe a dedicated prompt or option to do this
   * would be helpful.  */
  for (st = impres->imports; st; st = st->next)
  {
    if (st->result == 0)
      continue;
    printf("key %s import failed: %s\n", NONULL(st->fpr), gpgme_strerror(st->result));
  }
  fflush(stdout);

leave:
  gpgme_release(ctx);
  gpgme_data_release(keydata);
  mutt_file_fclose(&fp_in);
}

/**
 * copy_clearsigned - Copy a clearsigned message
 * @param data    GPGME data object
 * @param state   State to use
 * @param charset Charset of message
 *
 * strip the signature and PGP's dash-escaping.
 *
 * XXX charset handling: We assume that it is safe to do character set
 * decoding first, dash decoding second here, while we do it the other way
 * around in the main handler.
 *
 * (Note that we aren't worse than Outlook & Cie in this, and also note that we
 * can successfully handle anything produced by any existing versions of neomutt.)
 */
static void copy_clearsigned(gpgme_data_t data, struct State *state, char *charset)
{
  char buf[8192] = { 0 };
  bool complete, armor_header;
  FILE *fp = NULL;

  char *fname = data_object_to_tempfile(data, &fp);
  if (!fname)
  {
    mutt_file_fclose(&fp);
    return;
  }
  unlink(fname);
  FREE(&fname);

  /* fromcode comes from the MIME Content-Type charset label. It might
   * be a wrong label, so we want the ability to do corrections via
   * charset-hooks. Therefore we set flags to MUTT_ICONV_HOOK_FROM.  */
  struct FgetConv *fc = mutt_ch_fgetconv_open(fp, charset, cc_charset(), MUTT_ICONV_HOOK_FROM);

  for (complete = true, armor_header = true;
       mutt_ch_fgetconvs(buf, sizeof(buf), fc); complete = (strchr(buf, '\n')))
  {
    if (!complete)
    {
      if (!armor_header)
        state_puts(state, buf);
      continue;
    }

    if (BEGIN_PGP_SIGNATURE(buf))
      break;

    if (armor_header)
    {
      if (buf[0] == '\n')
        armor_header = false;
      continue;
    }

    if (state->prefix)
      state_puts(state, state->prefix);

    if ((buf[0] == '-') && (buf[1] == ' '))
      state_puts(state, buf + 2);
    else
      state_puts(state, buf);
  }

  mutt_ch_fgetconv_close(&fc);
  mutt_file_fclose(&fp);
}

/**
 * pgp_gpgme_application_handler - Manage the MIME type "application/pgp" or "application/smime" - Implements CryptModuleSpecs::application_handler() - @ingroup crypto_application_handler
 */
int pgp_gpgme_application_handler(struct Body *b, struct State *state)
{
  int needpass = -1;
  bool pgp_keyblock = false;
  bool clearsign = false;
  long bytes;
  LOFF_T last_pos;
  char buf[8192] = { 0 };
  FILE *fp_out = NULL;

  gpgme_error_t err = 0;
  gpgme_data_t armored_data = NULL;

  bool maybe_goodsig = true;
  bool have_any_sigs = false;

  char body_charset[256] = { 0 }; /* Only used for clearsigned messages. */

  mutt_debug(LL_DEBUG2, "Entering handler\n");

  /* For clearsigned messages we won't be able to get a character set
   * but we know that this may only be text thus we assume Latin-1 here. */
  if (!mutt_body_get_charset(b, body_charset, sizeof(body_charset)))
    mutt_str_copy(body_charset, "iso-8859-1", sizeof(body_charset));

  if (!mutt_file_seek(state->fp_in, b->offset, SEEK_SET))
  {
    return -1;
  }
  last_pos = b->offset;

  for (bytes = b->length; bytes > 0;)
  {
    if (!fgets(buf, sizeof(buf), state->fp_in))
      break;

    LOFF_T offset = ftello(state->fp_in);
    bytes -= (offset - last_pos); /* don't rely on mutt_str_len(buf) */
    last_pos = offset;

    size_t plen = mutt_str_startswith(buf, "-----BEGIN PGP ");
    if (plen != 0)
    {
      clearsign = false;

      if (MESSAGE(buf + plen))
      {
        needpass = 1;
      }
      else if (SIGNED_MESSAGE(buf + plen))
      {
        clearsign = true;
        needpass = 0;
      }
      else if (PUBLIC_KEY_BLOCK(buf + plen))
      {
        needpass = 0;
        pgp_keyblock = true;
      }
      else
      {
        /* XXX we may wish to recode here */
        if (state->prefix)
          state_puts(state, state->prefix);
        state_puts(state, buf);
        continue;
      }

      have_any_sigs = (have_any_sigs || (clearsign && (state->flags & STATE_VERIFY)));

      /* Copy PGP material to an data container */
      armored_data = file_to_data_object(state->fp_in, b->offset, b->length);
      /* Invoke PGP if needed */
      if (pgp_keyblock)
      {
        pgp_gpgme_extract_keys(armored_data, &fp_out);
      }
      else if (!clearsign || (state->flags & STATE_VERIFY))
      {
        gpgme_data_t plaintext = create_gpgme_data();
        gpgme_ctx_t ctx = create_gpgme_context(false);

        if (clearsign)
        {
          err = gpgme_op_verify(ctx, armored_data, NULL, plaintext);
        }
        else
        {
          err = gpgme_op_decrypt_verify(ctx, armored_data, plaintext);
          if (gpg_err_code(err) == GPG_ERR_NO_DATA)
          {
            /* Decrypt verify can't handle signed only messages. */
            gpgme_data_seek(armored_data, 0, SEEK_SET);
            /* Must release plaintext so that we supply an uninitialized object. */
            gpgme_data_release(plaintext);
            plaintext = create_gpgme_data();
            err = gpgme_op_verify(ctx, armored_data, NULL, plaintext);
          }
        }
        redraw_if_needed(ctx);

        if (err != 0)
        {
          char errbuf[200] = { 0 };

          snprintf(errbuf, sizeof(errbuf) - 1,
                   _("Error: decryption/verification failed: %s\n"), gpgme_strerror(err));
          state_puts(state, errbuf);
        }
        else
        {
          /* Decryption/Verification succeeded */

          mutt_message(_("PGP message successfully decrypted"));

          bool sig_stat = false;
          char *tmpfname = NULL;

          /* Check whether signatures have been verified.  */
          gpgme_verify_result_t verify_result = gpgme_op_verify_result(ctx);
          if (verify_result->signatures)
            sig_stat = true;

          have_any_sigs = false;
          maybe_goodsig = false;
          if ((state->flags & STATE_DISPLAY) && sig_stat)
          {
            int res, idx;
            bool anybad = false;

            state_attach_puts(state, _("[-- Begin signature information --]\n"));
            have_any_sigs = true;
            for (idx = 0; (res = show_one_sig_status(ctx, idx, state)) != -1; idx++)
            {
              if (res == 1)
                anybad = true;
            }
            if (!anybad && idx)
              maybe_goodsig = true;

            state_attach_puts(state, _("[-- End signature information --]\n\n"));
          }

          tmpfname = data_object_to_tempfile(plaintext, &fp_out);
          if (tmpfname)
          {
            unlink(tmpfname);
            FREE(&tmpfname);
          }
          else
          {
            mutt_file_fclose(&fp_out);
            state_puts(state, _("Error: copy data failed\n"));
          }
        }
        gpgme_data_release(plaintext);
        gpgme_release(ctx);
      }

      /* Now, copy cleartext to the screen.  NOTE - we expect that PGP
       * outputs utf-8 cleartext.  This may not always be true, but it
       * seems to be a reasonable guess.  */
      if (state->flags & STATE_DISPLAY)
      {
        if (needpass)
          state_attach_puts(state, _("[-- BEGIN PGP MESSAGE --]\n\n"));
        else if (pgp_keyblock)
          state_attach_puts(state, _("[-- BEGIN PGP PUBLIC KEY BLOCK --]\n"));
        else
          state_attach_puts(state, _("[-- BEGIN PGP SIGNED MESSAGE --]\n\n"));
      }

      if (clearsign)
      {
        copy_clearsigned(armored_data, state, body_charset);
      }
      else if (fp_out)
      {
        int c;
        rewind(fp_out);
        struct FgetConv *fc = mutt_ch_fgetconv_open(fp_out, "utf-8", cc_charset(),
                                                    MUTT_ICONV_NO_FLAGS);
        while ((c = mutt_ch_fgetconv(fc)) != EOF)
        {
          state_putc(state, c);
          if ((c == '\n') && state->prefix)
            state_puts(state, state->prefix);
        }
        mutt_ch_fgetconv_close(&fc);
      }

      if (state->flags & STATE_DISPLAY)
      {
        state_putc(state, '\n');
        if (needpass)
          state_attach_puts(state, _("[-- END PGP MESSAGE --]\n"));
        else if (pgp_keyblock)
          state_attach_puts(state, _("[-- END PGP PUBLIC KEY BLOCK --]\n"));
        else
          state_attach_puts(state, _("[-- END PGP SIGNED MESSAGE --]\n"));
      }

      gpgme_data_release(armored_data);
      mutt_file_fclose(&fp_out);
    }
    else
    {
      /* A traditional PGP part may mix signed and unsigned content */
      /* XXX we may wish to recode here */
      if (state->prefix)
        state_puts(state, state->prefix);
      state_puts(state, buf);
    }
  }

  b->goodsig = (maybe_goodsig && have_any_sigs);

  if (needpass == -1)
  {
    state_attach_puts(state, _("[-- Error: could not find beginning of PGP message --]\n\n"));
    return 1;
  }
  mutt_debug(LL_DEBUG2, "Leaving handler\n");

  return err;
}

/**
 * pgp_gpgme_encrypted_handler - Manage a PGP or S/MIME encrypted MIME part - Implements CryptModuleSpecs::encrypted_handler() - @ingroup crypto_encrypted_handler
 *
 * This handler is passed the application/octet-stream directly.
 * The caller must propagate b->goodsig to its parent.
 */
int pgp_gpgme_encrypted_handler(struct Body *b, struct State *state)
{
  int is_signed;
  int rc = 0;

  mutt_debug(LL_DEBUG2, "Entering handler\n");

  FILE *fp_out = mutt_file_mkstemp();
  if (!fp_out)
  {
    mutt_perror(_("Can't create temporary file"));
    if (state->flags & STATE_DISPLAY)
    {
      state_attach_puts(state, _("[-- Error: could not create temporary file --]\n"));
    }
    return -1;
  }

  struct Body *tattach = decrypt_part(b, state, fp_out, false, &is_signed);
  if (tattach)
  {
    tattach->goodsig = is_signed > 0;

    if (state->flags & STATE_DISPLAY)
    {
      state_attach_puts(state, is_signed ?
                                   _("[-- The following data is PGP/MIME signed and encrypted --]\n\n") :
                                   _("[-- The following data is PGP/MIME encrypted --]\n\n"));
      mutt_protected_headers_handler(tattach, state);
    }

    /* Store any protected headers in the parent so they can be
     * accessed for index updates after the handler recursion is done.
     * This is done before the handler to prevent a nested encrypted
     * handler from freeing the headers. */
    mutt_env_free(&b->mime_headers);
    b->mime_headers = tattach->mime_headers;
    tattach->mime_headers = NULL;

    FILE *fp_save = state->fp_in;
    state->fp_in = fp_out;
    rc = mutt_body_handler(tattach, state);
    state->fp_in = fp_save;

    /* Embedded multipart signed protected headers override the
     * encrypted headers.  We need to do this after the handler so
     * they can be printed in the pager. */
    if (mutt_is_multipart_signed(tattach) && tattach->parts && tattach->parts->mime_headers)
    {
      mutt_env_free(&b->mime_headers);
      b->mime_headers = tattach->parts->mime_headers;
      tattach->parts->mime_headers = NULL;
    }

    /* if a multipart/signed is the _only_ sub-part of a
     * multipart/encrypted, cache signature verification
     * status.  */
    if (mutt_is_multipart_signed(tattach) && !tattach->next)
      b->goodsig |= tattach->goodsig;

    if (state->flags & STATE_DISPLAY)
    {
      state_puts(state, "\n");
      state_attach_puts(state, is_signed ?
                                   _("[-- End of PGP/MIME signed and encrypted data --]\n") :
                                   _("[-- End of PGP/MIME encrypted data --]\n"));
    }

    mutt_body_free(&tattach);
    mutt_message(_("PGP message successfully decrypted"));
  }
  else
  {
#ifdef USE_AUTOCRYPT
    if (!OptAutocryptGpgme)
#endif
    {
      mutt_error(_("Could not decrypt PGP message"));
    }
    rc = -1;
  }

  mutt_file_fclose(&fp_out);
  mutt_debug(LL_DEBUG2, "Leaving handler\n");

  return rc;
}

/**
 * smime_gpgme_application_handler - Manage the MIME type "application/pgp" or "application/smime" - Implements CryptModuleSpecs::application_handler() - @ingroup crypto_application_handler
 */
int smime_gpgme_application_handler(struct Body *b, struct State *state)
{
  int is_signed = 0;
  int rc = 0;

  mutt_debug(LL_DEBUG2, "Entering handler\n");

  /* clear out any mime headers before the handler, so they can't be spoofed. */
  mutt_env_free(&b->mime_headers);
  b->warnsig = false;
  FILE *fp_out = mutt_file_mkstemp();
  if (!fp_out)
  {
    mutt_perror(_("Can't create temporary file"));
    if (state->flags & STATE_DISPLAY)
    {
      state_attach_puts(state, _("[-- Error: could not create temporary file --]\n"));
    }
    return -1;
  }

  struct Body *tattach = decrypt_part(b, state, fp_out, true, &is_signed);
  if (tattach)
  {
    tattach->goodsig = is_signed > 0;

    if (state->flags & STATE_DISPLAY)
    {
      state_attach_puts(state, is_signed ?
                                   _("[-- The following data is S/MIME signed --]\n\n") :
                                   _("[-- The following data is S/MIME encrypted --]\n\n"));
      mutt_protected_headers_handler(tattach, state);
    }

    /* Store any protected headers in the parent so they can be
     * accessed for index updates after the handler recursion is done.
     * This is done before the handler to prevent a nested encrypted
     * handler from freeing the headers. */
    mutt_env_free(&b->mime_headers);
    b->mime_headers = tattach->mime_headers;
    tattach->mime_headers = NULL;

    FILE *fp_save = state->fp_in;
    state->fp_in = fp_out;
    rc = mutt_body_handler(tattach, state);
    state->fp_in = fp_save;

    /* Embedded multipart signed protected headers override the
     * encrypted headers.  We need to do this after the handler so
     * they can be printed in the pager. */
    if (mutt_is_multipart_signed(tattach) && tattach->parts && tattach->parts->mime_headers)
    {
      mutt_env_free(&b->mime_headers);
      b->mime_headers = tattach->parts->mime_headers;
      tattach->parts->mime_headers = NULL;
    }

    /* if a multipart/signed is the _only_ sub-part of a multipart/encrypted,
     * cache signature verification status.  */
    if (mutt_is_multipart_signed(tattach) && !tattach->next)
    {
      b->goodsig = tattach->goodsig;
      if (!b->goodsig)
        b->warnsig = tattach->warnsig;
    }
    else if (tattach->goodsig)
    {
      b->goodsig = true;
      b->warnsig = tattach->warnsig;
    }

    if (state->flags & STATE_DISPLAY)
    {
      state_puts(state, "\n");
      state_attach_puts(state, is_signed ? _("[-- End of S/MIME signed data --]\n") :
                                           _("[-- End of S/MIME encrypted data --]\n"));
    }

    mutt_body_free(&tattach);
  }

  mutt_file_fclose(&fp_out);
  mutt_debug(LL_DEBUG2, "Leaving handler\n");

  return rc;
}

/**
 * key_check_cap - Check the capabilities of a key
 * @param key GPGME key
 * @param cap Flags, e.g. #KEY_CAP_CAN_ENCRYPT
 * @retval >0 Key has the capabilities
 */
unsigned int key_check_cap(gpgme_key_t key, enum KeyCap cap)
{
  unsigned int rc = 0;

  switch (cap)
  {
    case KEY_CAP_CAN_ENCRYPT:
      rc = key->can_encrypt;
      if (rc == 0)
      {
        for (gpgme_subkey_t subkey = key->subkeys; subkey; subkey = subkey->next)
        {
          rc = subkey->can_encrypt;
          if (rc != 0)
            break;
        }
      }
      break;
    case KEY_CAP_CAN_SIGN:
      rc = key->can_sign;
      if (rc == 0)
      {
        for (gpgme_subkey_t subkey = key->subkeys; subkey; subkey = subkey->next)
        {
          rc = subkey->can_sign;
          if (rc != 0)
            break;
        }
      }
      break;
    case KEY_CAP_CAN_CERTIFY:
      rc = key->can_certify;
      if (rc == 0)
      {
        for (gpgme_subkey_t subkey = key->subkeys; subkey; subkey = subkey->next)
        {
          rc = subkey->can_certify;
          if (rc != 0)
            break;
        }
      }
      break;
  }

  return rc;
}

/**
 * list_to_pattern - Convert STailQ to GPGME-compatible pattern
 * @param list List of strings to convert
 * @retval ptr GPGME-compatible pattern
 *
 * We need to convert spaces in an item into a '+' and '%' into "%25".
 *
 * @note The caller must free the returned pattern
 */
static char *list_to_pattern(struct ListHead *list)
{
  char *pattern = NULL, *p = NULL;
  const char *s = NULL;
  size_t n;

  n = 0;
  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, list, entries)
  {
    for (s = np->data; *s; s++)
    {
      if ((*s == '%') || (*s == '+'))
        n += 2;
      n++;
    }
    n++; /* delimiter or end of string */
  }
  n++; /* make sure to allocate at least one byte */
  p = mutt_mem_calloc(1, n);
  pattern = p;
  STAILQ_FOREACH(np, list, entries)
  {
    s = np->data;
    if (*s)
    {
      if (np != STAILQ_FIRST(list))
        *p++ = ' ';
      for (s = np->data; *s; s++)
      {
        if (*s == '%')
        {
          *p++ = '%';
          *p++ = '2';
          *p++ = '5';
        }
        else if (*s == '+')
        {
          *p++ = '%';
          *p++ = '2';
          *p++ = 'B';
        }
        else if (*s == ' ')
        {
          *p++ = '+';
        }
        else
        {
          *p++ = *s;
        }
      }
    }
  }
  *p = '\0';
  return pattern;
}

/**
 * get_candidates - Get a list of keys which are candidates for the selection
 * @param hints  List of strings to match
 * @param app    Application type, e.g. #APPLICATION_PGP
 * @param secret If true, only match secret keys
 * @retval ptr  Key List
 * @retval NULL Error
 *
 * Select by looking at the HINTS list.
 */
static struct CryptKeyInfo *get_candidates(struct ListHead *hints, SecurityFlags app, int secret)
{
  struct CryptKeyInfo *db = NULL, *k = NULL, **kend = NULL;
  gpgme_error_t err;
  gpgme_ctx_t ctx = NULL;
  gpgme_key_t key = NULL;
  int idx;
  gpgme_user_id_t uid = NULL;

  char *pattern = list_to_pattern(hints);
  if (!pattern)
    return NULL;

  ctx = create_gpgme_context(0);
  db = NULL;
  kend = &db;

  if ((app & APPLICATION_PGP))
  {
    /* It's all a mess.  That old GPGME expects different things depending on
     * the protocol.  For gpg we don't need percent escaped pappert but simple
     * strings passed in an array to the keylist_ext_start function. */
    size_t n = 0;
    struct ListNode *np = NULL;
    STAILQ_FOREACH(np, hints, entries)
    {
      if (np->data && *np->data)
        n++;
    }
    if (n == 0)
      goto no_pgphints;

    char **patarr = mutt_mem_calloc(n + 1, sizeof(*patarr));
    n = 0;
    STAILQ_FOREACH(np, hints, entries)
    {
      if (np->data && *np->data)
        patarr[n++] = mutt_str_dup(np->data);
    }
    patarr[n] = NULL;
    err = gpgme_op_keylist_ext_start(ctx, (const char **) patarr, secret, 0);
    for (n = 0; patarr[n]; n++)
      FREE(&patarr[n]);
    FREE(&patarr);
    if (err != 0)
    {
      mutt_error(_("gpgme_op_keylist_start failed: %s"), gpgme_strerror(err));
      gpgme_release(ctx);
      FREE(&pattern);
      return NULL;
    }

    while ((err = gpgme_op_keylist_next(ctx, &key)) == 0)
    {
      KeyFlags flags = KEYFLAG_NO_FLAGS;

      if (key_check_cap(key, KEY_CAP_CAN_ENCRYPT))
        flags |= KEYFLAG_CANENCRYPT;
      if (key_check_cap(key, KEY_CAP_CAN_SIGN))
        flags |= KEYFLAG_CANSIGN;

      if (key->revoked)
        flags |= KEYFLAG_REVOKED;
      if (key->expired)
        flags |= KEYFLAG_EXPIRED;
      if (key->disabled)
        flags |= KEYFLAG_DISABLED;

      for (idx = 0, uid = key->uids; uid; idx++, uid = uid->next)
      {
        k = mutt_mem_calloc(1, sizeof(*k));
        k->kobj = key;
        gpgme_key_ref(k->kobj);
        k->idx = idx;
        k->uid = uid->uid;
        k->flags = flags;
        if (uid->revoked)
          k->flags |= KEYFLAG_REVOKED;
        k->validity = uid->validity;
        *kend = k;
        kend = &k->next;
      }
      gpgme_key_unref(key);
    }
    if (gpg_err_code(err) != GPG_ERR_EOF)
      mutt_error(_("gpgme_op_keylist_next failed: %s"), gpgme_strerror(err));
    gpgme_op_keylist_end(ctx);
  no_pgphints:;
  }

  if ((app & APPLICATION_SMIME))
  {
    /* and now look for x509 certificates */
    gpgme_set_protocol(ctx, GPGME_PROTOCOL_CMS);
    err = gpgme_op_keylist_start(ctx, pattern, 0);
    if (err != 0)
    {
      mutt_error(_("gpgme_op_keylist_start failed: %s"), gpgme_strerror(err));
      gpgme_release(ctx);
      FREE(&pattern);
      return NULL;
    }

    while ((err = gpgme_op_keylist_next(ctx, &key)) == 0)
    {
      KeyFlags flags = KEYFLAG_ISX509;

      if (key_check_cap(key, KEY_CAP_CAN_ENCRYPT))
        flags |= KEYFLAG_CANENCRYPT;
      if (key_check_cap(key, KEY_CAP_CAN_SIGN))
        flags |= KEYFLAG_CANSIGN;

      if (key->revoked)
        flags |= KEYFLAG_REVOKED;
      if (key->expired)
        flags |= KEYFLAG_EXPIRED;
      if (key->disabled)
        flags |= KEYFLAG_DISABLED;

      for (idx = 0, uid = key->uids; uid; idx++, uid = uid->next)
      {
        k = mutt_mem_calloc(1, sizeof(*k));
        k->kobj = key;
        gpgme_key_ref(k->kobj);
        k->idx = idx;
        k->uid = uid->uid;
        k->flags = flags;
        if (uid->revoked)
          k->flags |= KEYFLAG_REVOKED;
        k->validity = uid->validity;
        *kend = k;
        kend = &k->next;
      }
      gpgme_key_unref(key);
    }
    if (gpg_err_code(err) != GPG_ERR_EOF)
      mutt_error(_("gpgme_op_keylist_next failed: %s"), gpgme_strerror(err));
    gpgme_op_keylist_end(ctx);
  }

  gpgme_release(ctx);
  FREE(&pattern);
  return db;
}

/**
 * crypt_add_string_to_hints - Split a string and add the parts to a List
 * @param[in]  str   String to parse
 * @param[out] hints List of string parts
 *
 * The string str is split by whitespace and punctuation and the parts added to
 * hints.  This list is later used to match addresses.
 */
static void crypt_add_string_to_hints(const char *str, struct ListHead *hints)
{
  char *scratch = mutt_str_dup(str);
  if (!scratch)
    return;

  for (char *t = strtok(scratch, " ,.:\"()<>\n"); t; t = strtok(NULL, " ,.:\"()<>\n"))
  {
    if (strlen(t) > 3)
      mutt_list_insert_tail(hints, mutt_str_dup(t));
  }

  FREE(&scratch);
}

/**
 * crypt_getkeybyaddr - Find a key by email address
 * @param[in]  a            Address to match
 * @param[in]  abilities    Abilities to match, see #KeyFlags
 * @param[in]  app          Application type, e.g. #APPLICATION_PGP
 * @param[out] forced_valid Set to true if user overrode key's validity
 * @param[in]  oppenc_mode  If true, use opportunistic encryption
 * @retval ptr Matching key
 */
static struct CryptKeyInfo *crypt_getkeybyaddr(struct Address *a,
                                               KeyFlags abilities, unsigned int app,
                                               bool *forced_valid, bool oppenc_mode)
{
  struct ListHead hints = STAILQ_HEAD_INITIALIZER(hints);

  int multi = false;
  int this_key_has_strong = false;
  int this_key_has_addr_match = false;
  int match = false;

  struct CryptKeyInfo *keys = NULL, *k = NULL;
  struct CryptKeyInfo *the_strong_valid_key = NULL;
  struct CryptKeyInfo *a_valid_addrmatch_key = NULL;
  struct CryptKeyInfo *matches = NULL;
  struct CryptKeyInfo **matches_endp = &matches;

  if (a && a->mailbox)
    mutt_list_insert_tail(&hints, buf_strdup(a->mailbox));
  if (a && a->personal)
    crypt_add_string_to_hints(buf_string(a->personal), &hints);

  if (!oppenc_mode)
    mutt_message(_("Looking for keys matching \"%s\"..."), a ? buf_string(a->mailbox) : "");
  keys = get_candidates(&hints, app, (abilities & KEYFLAG_CANSIGN));

  mutt_list_free(&hints);

  if (!keys)
    return NULL;

  mutt_debug(LL_DEBUG5, "looking for %s <%s>\n",
             a ? buf_string(a->personal) : "", a ? buf_string(a->mailbox) : "");

  for (k = keys; k; k = k->next)
  {
    mutt_debug(LL_DEBUG5, "  looking at key: %s '%.15s'\n", crypt_keyid(k), k->uid);

    if (abilities && !(k->flags & abilities))
    {
      mutt_debug(LL_DEBUG2, "  insufficient abilities: Has %x, want %x\n", k->flags, abilities);
      continue;
    }

    this_key_has_strong = false; /* strong and valid match */
    this_key_has_addr_match = false;
    match = false; /* any match */

    struct AddressList alist = TAILQ_HEAD_INITIALIZER(alist);
    mutt_addrlist_parse(&alist, k->uid);
    struct Address *ka = NULL;
    TAILQ_FOREACH(ka, &alist, entries)
    {
      int validity = crypt_id_matches_addr(a, ka, k);

      if (validity & CRYPT_KV_MATCH) /* something matches */
      {
        match = true;

        if ((validity & CRYPT_KV_VALID) && (validity & CRYPT_KV_ADDR))
        {
          if (validity & CRYPT_KV_STRONGID)
          {
            if (the_strong_valid_key && (the_strong_valid_key->kobj != k->kobj))
              multi = true;
            this_key_has_strong = true;
          }
          else
          {
            this_key_has_addr_match = true;
          }
        }
      }
    }
    mutt_addrlist_clear(&alist);

    if (match)
    {
      struct CryptKeyInfo *tmp = crypt_copy_key(k);
      *matches_endp = tmp;
      matches_endp = &tmp->next;

      if (this_key_has_strong)
        the_strong_valid_key = tmp;
      else if (this_key_has_addr_match)
        a_valid_addrmatch_key = tmp;
    }
  }

  crypt_key_free(&keys);

  if (matches)
  {
    if (oppenc_mode)
    {
      const bool c_crypt_opportunistic_encrypt_strong_keys =
          cs_subset_bool(NeoMutt->sub, "crypt_opportunistic_encrypt_strong_keys");
      if (the_strong_valid_key)
        k = crypt_copy_key(the_strong_valid_key);
      else if (a_valid_addrmatch_key && !c_crypt_opportunistic_encrypt_strong_keys)
        k = crypt_copy_key(a_valid_addrmatch_key);
      else
        k = NULL;
    }
    else if (the_strong_valid_key && !multi)
    {
      /* There was precisely one strong match on a valid ID.
       * Proceed without asking the user.  */
      k = crypt_copy_key(the_strong_valid_key);
    }
    else
    {
      /* Else: Ask the user.  */
      k = dlg_gpgme(matches, a, NULL, app, forced_valid);
    }

    crypt_key_free(&matches);
  }
  else
  {
    k = NULL;
  }

  return k;
}

/**
 * crypt_getkeybystr - Find a key by string
 * @param[in]  p            String to match
 * @param[in]  abilities    Abilities to match, see #KeyFlags
 * @param[in]  app          Application type, e.g. #APPLICATION_PGP
 * @param[out] forced_valid Set to true if user overrode key's validity
 * @retval ptr Matching key
 */
static struct CryptKeyInfo *crypt_getkeybystr(const char *p, KeyFlags abilities,
                                              unsigned int app, bool *forced_valid)
{
  struct ListHead hints = STAILQ_HEAD_INITIALIZER(hints);
  struct CryptKeyInfo *matches = NULL;
  struct CryptKeyInfo **matches_endp = &matches;
  struct CryptKeyInfo *k = NULL;
  const char *ps = NULL, *pl = NULL, *phint = NULL;

  mutt_message(_("Looking for keys matching \"%s\"..."), p);

  const char *pfcopy = crypt_get_fingerprint_or_id(p, &phint, &pl, &ps);
  crypt_add_string_to_hints(phint, &hints);
  struct CryptKeyInfo *keys = get_candidates(&hints, app, (abilities & KEYFLAG_CANSIGN));
  mutt_list_free(&hints);

  if (!keys)
  {
    FREE(&pfcopy);
    return NULL;
  }

  for (k = keys; k; k = k->next)
  {
    if (abilities && !(k->flags & abilities))
      continue;

    mutt_debug(LL_DEBUG5, "matching \"%s\" against key %s, \"%s\": ", p,
               crypt_long_keyid(k), k->uid);

    if ((*p == '\0') || (pfcopy && mutt_istr_equal(pfcopy, crypt_fpr(k))) ||
        (pl && mutt_istr_equal(pl, crypt_long_keyid(k))) ||
        (ps && mutt_istr_equal(ps, crypt_short_keyid(k))) || mutt_istr_find(k->uid, p))
    {
      mutt_debug(LL_DEBUG5, "match\n");

      struct CryptKeyInfo *tmp = crypt_copy_key(k);
      *matches_endp = tmp;
      matches_endp = &tmp->next;
    }
    else
    {
      mutt_debug(LL_DEBUG5, "no match\n");
    }
  }

  FREE(&pfcopy);
  crypt_key_free(&keys);

  if (matches)
  {
    k = dlg_gpgme(matches, NULL, p, app, forced_valid);
    crypt_key_free(&matches);
    return k;
  }

  return NULL;
}

/**
 * crypt_ask_for_key - Ask the user for a key
 * @param[in]  tag          Prompt to display
 * @param[in]  whatfor      Label to use (OPTIONAL)
 * @param[in]  abilities    Flags, see #KeyFlags
 * @param[in]  app          Application type, e.g. #APPLICATION_PGP
 * @param[out] forced_valid Set to true if user overrode key's validity
 * @retval ptr Copy of the selected key
 *
 * If whatfor is not null use it as default and store it under that label as
 * the next default.
 */
static struct CryptKeyInfo *crypt_ask_for_key(const char *tag, const char *whatfor,
                                              KeyFlags abilities,
                                              unsigned int app, bool *forced_valid)
{
  struct CryptKeyInfo *key = NULL;
  struct CryptCache *l = NULL;
  struct Buffer *resp = buf_pool_get();

  mutt_clear_error();

  if (whatfor)
  {
    for (l = IdDefaults; l; l = l->next)
    {
      if (mutt_istr_equal(whatfor, l->what))
      {
        buf_strcpy(resp, l->dflt);
        break;
      }
    }
  }

  while (true)
  {
    buf_reset(resp);
    if (mw_get_field(tag, resp, MUTT_COMP_NO_FLAGS, HC_OTHER, NULL, NULL) != 0)
    {
      goto done;
    }

    if (whatfor)
    {
      if (l)
      {
        mutt_str_replace(&l->dflt, buf_string(resp));
      }
      else
      {
        l = mutt_mem_malloc(sizeof(struct CryptCache));
        l->next = IdDefaults;
        IdDefaults = l;
        l->what = mutt_str_dup(whatfor);
        l->dflt = buf_strdup(resp);
      }
    }

    key = crypt_getkeybystr(buf_string(resp), abilities, app, forced_valid);
    if (key)
      goto done;

    mutt_error(_("No matching keys found for \"%s\""), buf_string(resp));
  }

done:
  buf_pool_release(&resp);
  return key;
}

/**
 * find_keys - Find keys of the recipients of the message
 * @param addrlist    Address List
 * @param app         Application type, e.g. #APPLICATION_PGP
 * @param oppenc_mode If true, use opportunistic encryption
 * @retval ptr  Space-separated string of keys
 * @retval NULL At least one of the keys can't be found
 *
 * If oppenc_mode is true, only keys that can be determined without prompting
 * will be used.
 */
static char *find_keys(const struct AddressList *addrlist, unsigned int app, bool oppenc_mode)
{
  struct ListHead crypt_hook_list = STAILQ_HEAD_INITIALIZER(crypt_hook_list);
  struct ListNode *crypt_hook = NULL;
  const char *keyid = NULL;
  char *keylist = NULL;
  size_t keylist_size = 0;
  size_t keylist_used = 0;
  struct Address *p = NULL;
  struct CryptKeyInfo *k_info = NULL;
  const char *fqdn = mutt_fqdn(true, NeoMutt->sub);
  char buf[1024] = { 0 };
  bool forced_valid = false;
  bool key_selected;
  struct AddressList hookal = TAILQ_HEAD_INITIALIZER(hookal);

  struct Address *a = NULL;
  const bool c_crypt_confirm_hook = cs_subset_bool(NeoMutt->sub, "crypt_confirm_hook");
  TAILQ_FOREACH(a, addrlist, entries)
  {
    key_selected = false;
    mutt_crypt_hook(&crypt_hook_list, a);
    crypt_hook = STAILQ_FIRST(&crypt_hook_list);
    do
    {
      p = a;
      forced_valid = false;
      k_info = NULL;

      if (crypt_hook)
      {
        keyid = crypt_hook->data;
        enum QuadOption ans = MUTT_YES;
        if (!oppenc_mode && c_crypt_confirm_hook)
        {
          snprintf(buf, sizeof(buf), _("Use keyID = \"%s\" for %s?"), keyid,
                   buf_string(p->mailbox));
          ans = query_yesorno_help(buf, MUTT_YES, NeoMutt->sub, "crypt_confirm_hook");
        }
        if (ans == MUTT_YES)
        {
          if (crypt_is_numerical_keyid(keyid))
          {
            if (mutt_strn_equal(keyid, "0x", 2))
              keyid += 2;
            goto bypass_selection; /* you don't see this. */
          }

          /* check for e-mail address */
          mutt_addrlist_clear(&hookal);
          if (strchr(keyid, '@') && (mutt_addrlist_parse(&hookal, keyid) != 0))
          {
            mutt_addrlist_qualify(&hookal, fqdn);
            p = TAILQ_FIRST(&hookal);
          }
          else if (!oppenc_mode)
          {
            k_info = crypt_getkeybystr(keyid, KEYFLAG_CANENCRYPT, app, &forced_valid);
          }
        }
        else if (ans == MUTT_NO)
        {
          if (key_selected || STAILQ_NEXT(crypt_hook, entries))
          {
            crypt_hook = STAILQ_NEXT(crypt_hook, entries);
            continue;
          }
        }
        else if (ans == MUTT_ABORT)
        {
          FREE(&keylist);
          mutt_addrlist_clear(&hookal);
          mutt_list_free(&crypt_hook_list);
          return NULL;
        }
      }

      if (!k_info)
      {
        k_info = crypt_getkeybyaddr(p, KEYFLAG_CANENCRYPT, app, &forced_valid, oppenc_mode);
      }

      if (!k_info && !oppenc_mode)
      {
        snprintf(buf, sizeof(buf), _("Enter keyID for %s: "), buf_string(p->mailbox));

        k_info = crypt_ask_for_key(buf, buf_string(p->mailbox),
                                   KEYFLAG_CANENCRYPT, app, &forced_valid);
      }

      if (!k_info)
      {
        FREE(&keylist);
        mutt_addrlist_clear(&hookal);
        mutt_list_free(&crypt_hook_list);
        return NULL;
      }

      keyid = crypt_fpr_or_lkeyid(k_info);

    bypass_selection:
      keylist_size += mutt_str_len(keyid) + 4 + 1;
      mutt_mem_realloc(&keylist, keylist_size);
      sprintf(keylist + keylist_used, "%s0x%s%s", keylist_used ? " " : "",
              keyid, forced_valid ? "!" : "");
      keylist_used = mutt_str_len(keylist);

      key_selected = true;

      crypt_key_free(&k_info);
      mutt_addrlist_clear(&hookal);

      if (crypt_hook)
        crypt_hook = STAILQ_NEXT(crypt_hook, entries);

    } while (crypt_hook);

    mutt_list_free(&crypt_hook_list);
  }
  return keylist;
}

/**
 * pgp_gpgme_find_keys - Find the keyids of the recipients of a message - Implements CryptModuleSpecs::find_keys() - @ingroup crypto_find_keys
 */
char *pgp_gpgme_find_keys(const struct AddressList *addrlist, bool oppenc_mode)
{
  return find_keys(addrlist, APPLICATION_PGP, oppenc_mode);
}

/**
 * smime_gpgme_find_keys - Find the keyids of the recipients of a message - Implements CryptModuleSpecs::find_keys() - @ingroup crypto_find_keys
 */
char *smime_gpgme_find_keys(const struct AddressList *addrlist, bool oppenc_mode)
{
  return find_keys(addrlist, APPLICATION_SMIME, oppenc_mode);
}

#ifdef USE_AUTOCRYPT
/**
 * mutt_gpgme_select_secret_key - Select a private Autocrypt key for a new account
 * @param keyid Autocrypt Key id
 * @retval  0 Success
 * @retval -1 Error
 *
 * Unfortunately, the internal ncrypt/crypt_gpgme.c functions use CryptKeyInfo,
 * and so aren't exportable.
 *
 * This function queries all private keys, provides the crypt_select_keys()
 * menu, and returns the selected key fingerprint in keyid.
 */
int mutt_gpgme_select_secret_key(struct Buffer *keyid)
{
  int rc = -1;
  gpgme_error_t err;
  gpgme_key_t key = NULL;
  gpgme_user_id_t uid = NULL;
  struct CryptKeyInfo *results = NULL, *k = NULL;
  struct CryptKeyInfo **kend = NULL;
  struct CryptKeyInfo *choice = NULL;

  gpgme_ctx_t ctx = create_gpgme_context(false);

  /* list all secret keys */
  if (gpgme_op_keylist_start(ctx, NULL, 1))
    goto cleanup;

  kend = &results;

  while (!(err = gpgme_op_keylist_next(ctx, &key)))
  {
    KeyFlags flags = KEYFLAG_NO_FLAGS;

    if (key_check_cap(key, KEY_CAP_CAN_ENCRYPT))
      flags |= KEYFLAG_CANENCRYPT;
    if (key_check_cap(key, KEY_CAP_CAN_SIGN))
      flags |= KEYFLAG_CANSIGN;

    if (key->revoked)
      flags |= KEYFLAG_REVOKED;
    if (key->expired)
      flags |= KEYFLAG_EXPIRED;
    if (key->disabled)
      flags |= KEYFLAG_DISABLED;

    int idx;
    for (idx = 0, uid = key->uids; uid; idx++, uid = uid->next)
    {
      k = mutt_mem_calloc(1, sizeof(*k));
      k->kobj = key;
      gpgme_key_ref(k->kobj);
      k->idx = idx;
      k->uid = uid->uid;
      k->flags = flags;
      if (uid->revoked)
        k->flags |= KEYFLAG_REVOKED;
      k->validity = uid->validity;
      *kend = k;
      kend = &k->next;
    }
    gpgme_key_unref(key);
  }
  if (gpg_err_code(err) != GPG_ERR_EOF)
    mutt_error(_("gpgme_op_keylist_next failed: %s"), gpgme_strerror(err));
  gpgme_op_keylist_end(ctx);

  if (!results)
  {
    /* L10N: mutt_gpgme_select_secret_key() tries to list all secret keys to choose
       from.  This error is displayed if no results were found.  */
    mutt_error(_("No secret keys found"));
    goto cleanup;
  }

  choice = dlg_gpgme(results, NULL, "*", APPLICATION_PGP, NULL);
  if (!(choice && choice->kobj && choice->kobj->subkeys && choice->kobj->subkeys->fpr))
    goto cleanup;
  buf_strcpy(keyid, choice->kobj->subkeys->fpr);

  rc = 0;

cleanup:
  crypt_key_free(&choice);
  crypt_key_free(&results);
  gpgme_release(ctx);
  return rc;
}
#endif

/**
 * pgp_gpgme_make_key_attachment - Generate a public key attachment - Implements CryptModuleSpecs::pgp_make_key_attachment() - @ingroup crypto_pgp_make_key_attachment
 */
struct Body *pgp_gpgme_make_key_attachment(void)
{
  gpgme_ctx_t context = NULL;
  gpgme_key_t export_keys[2] = { 0 };
  gpgme_data_t keydata = NULL;
  gpgme_error_t err;
  struct Body *att = NULL;
  char buf[1024] = { 0 };

  OptPgpCheckTrust = false;

  struct CryptKeyInfo *key = crypt_ask_for_key(_("Please enter the key ID: "), NULL,
                                               KEYFLAG_NO_FLAGS, APPLICATION_PGP, NULL);
  if (!key)
    goto bail;
  export_keys[0] = key->kobj;
  export_keys[1] = NULL;

  context = create_gpgme_context(false);
  gpgme_set_armor(context, 1);
  keydata = create_gpgme_data();
  err = gpgme_op_export_keys(context, export_keys, 0, keydata);
  if (err != GPG_ERR_NO_ERROR)
  {
    mutt_error(_("Error exporting key: %s"), gpgme_strerror(err));
    goto bail;
  }

  char *tempf = data_object_to_tempfile(keydata, NULL);
  if (!tempf)
    goto bail;

  att = mutt_body_new();
  /* tempf is a newly allocated string, so this is correct: */
  att->filename = tempf;
  att->unlink = true;
  att->use_disp = false;
  att->type = TYPE_APPLICATION;
  att->subtype = mutt_str_dup("pgp-keys");
  /* L10N: MIME description for exported (attached) keys.
     You can translate this entry to a non-ASCII string (it will be encoded),
     but it may be safer to keep it untranslated. */
  snprintf(buf, sizeof(buf), _("PGP Key 0x%s"), crypt_keyid(key));
  att->description = mutt_str_dup(buf);
  mutt_update_encoding(att, NeoMutt->sub);

  att->length = mutt_file_get_size(tempf);

bail:
  crypt_key_free(&key);
  gpgme_data_release(keydata);
  gpgme_release(context);

  return att;
}

/**
 * init_common - Initialise code common to PGP and SMIME parts of GPGME
 */
static void init_common(void)
{
  /* this initialization should only run one time, but it may be called by
   * either pgp_gpgme_init or smime_gpgme_init */
  static bool has_run = false;
  if (has_run)
    return;

  gpgme_check_version(NULL);
  gpgme_set_locale(NULL, LC_CTYPE, setlocale(LC_CTYPE, NULL));
#ifdef ENABLE_NLS
  gpgme_set_locale(NULL, LC_MESSAGES, setlocale(LC_MESSAGES, NULL));
#endif
  has_run = true;
}

/**
 * init_pgp - Initialise the PGP crypto backend
 */
static void init_pgp(void)
{
  if (gpgme_engine_check_version(GPGME_PROTOCOL_OpenPGP) != GPG_ERR_NO_ERROR)
  {
    mutt_error(_("GPGME: OpenPGP protocol not available"));
  }
}

/**
 * init_smime - Initialise the SMIME crypto backend
 */
static void init_smime(void)
{
  if (gpgme_engine_check_version(GPGME_PROTOCOL_CMS) != GPG_ERR_NO_ERROR)
  {
    mutt_error(_("GPGME: CMS protocol not available"));
  }
}

/**
 * pgp_gpgme_init - Initialise the crypto module - Implements CryptModuleSpecs::init() - @ingroup crypto_init
 */
void pgp_gpgme_init(void)
{
  init_common();
  init_pgp();
}

/**
 * smime_gpgme_init - Initialise the crypto module - Implements CryptModuleSpecs::init() - @ingroup crypto_init
 */
void smime_gpgme_init(void)
{
  init_common();
  init_smime();
}

/**
 * gpgme_send_menu - Show the user the encryption/signing menu
 * @param e        Email
 * @param is_smime True if an SMIME message
 * @retval num Flags, e.g. #APPLICATION_SMIME | #SEC_ENCRYPT
 */
static SecurityFlags gpgme_send_menu(struct Email *e, bool is_smime)
{
  struct CryptKeyInfo *p = NULL;
  const char *prompt = NULL;
  const char *letters = NULL;
  const char *choices = NULL;
  int choice;

  if (is_smime)
    e->security |= APPLICATION_SMIME;
  else
    e->security |= APPLICATION_PGP;

  /* Opportunistic encrypt is controlling encryption.
   * NOTE: "Signing" and "Clearing" only adjust the sign bit, so we have different
   *       letter choices for those.
   */
  const bool c_crypt_opportunistic_encrypt = cs_subset_bool(NeoMutt->sub, "crypt_opportunistic_encrypt");
  if (c_crypt_opportunistic_encrypt && (e->security & SEC_OPPENCRYPT))
  {
    if (is_smime)
    {
      /* L10N: S/MIME options (opportunistic encryption is on) */
      prompt = _("S/MIME (s)ign, sign (a)s, (p)gp, (c)lear, or (o)ppenc mode off?");
      /* L10N: S/MIME options (opportunistic encryption is on) */
      letters = _("sapco");
      choices = "SapCo";
    }
    else
    {
      /* L10N: PGP options (opportunistic encryption is on) */
      prompt = _("PGP (s)ign, sign (a)s, s/(m)ime, (c)lear, or (o)ppenc mode off?");
      /* L10N: PGP options (opportunistic encryption is on) */
      letters = _("samco");
      choices = "SamCo";
    }
  }
  else if (c_crypt_opportunistic_encrypt)
  {
    /* Opportunistic encryption option is set, but is toggled off for this message.  */
    if (is_smime)
    {
      /* L10N: S/MIME options (opportunistic encryption is off) */
      prompt = _("S/MIME (e)ncrypt, (s)ign, sign (a)s, (b)oth, (p)gp, (c)lear, or (o)ppenc mode?");
      /* L10N: S/MIME options (opportunistic encryption is off) */
      letters = _("esabpco");
      choices = "esabpcO";
    }
    else
    {
      /* L10N: PGP options (opportunistic encryption is off) */
      prompt = _("PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, s/(m)ime, (c)lear, or (o)ppenc mode?");
      /* L10N: PGP options (opportunistic encryption is off) */
      letters = _("esabmco");
      choices = "esabmcO";
    }
  }
  else
  {
    /* Opportunistic encryption is unset */
    if (is_smime)
    {
      /* L10N: S/MIME options */
      prompt = _("S/MIME (e)ncrypt, (s)ign, sign (a)s, (b)oth, (p)gp or (c)lear?");
      /* L10N: S/MIME options */
      letters = _("esabpc");
      choices = "esabpc";
    }
    else
    {
      /* L10N: PGP options */
      prompt = _("PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, s/(m)ime or (c)lear?");
      /* L10N: PGP options */
      letters = _("esabmc");
      choices = "esabmc";
    }
  }

  choice = mw_multi_choice(prompt, letters);
  if (choice > 0)
  {
    switch (choices[choice - 1])
    {
      case 'a': /* sign (a)s */
        p = crypt_ask_for_key(_("Sign as: "), NULL, KEYFLAG_CANSIGN,
                              is_smime ? APPLICATION_SMIME : APPLICATION_PGP, NULL);
        if (p)
        {
          char input_signas[128] = { 0 };
          snprintf(input_signas, sizeof(input_signas), "0x%s", crypt_fpr_or_lkeyid(p));

          if (is_smime)
            cs_subset_str_string_set(NeoMutt->sub, "smime_sign_as", input_signas, NULL);
          else
            cs_subset_str_string_set(NeoMutt->sub, "pgp_sign_as", input_signas, NULL);

          crypt_key_free(&p);

          e->security |= SEC_SIGN;
        }
        break;

      case 'b': /* (b)oth */
        e->security |= (SEC_ENCRYPT | SEC_SIGN);
        break;

      case 'C':
        e->security &= ~SEC_SIGN;
        break;

      case 'c': /* (c)lear */
        e->security &= ~(SEC_ENCRYPT | SEC_SIGN);
        break;

      case 'e': /* (e)ncrypt */
        e->security |= SEC_ENCRYPT;
        e->security &= ~SEC_SIGN;
        break;

      case 'm': /* (p)gp or s/(m)ime */
      case 'p':
        is_smime = !is_smime;
        if (is_smime)
        {
          e->security &= ~APPLICATION_PGP;
          e->security |= APPLICATION_SMIME;
        }
        else
        {
          e->security &= ~APPLICATION_SMIME;
          e->security |= APPLICATION_PGP;
        }
        crypt_opportunistic_encrypt(e);
        break;

      case 'O': /* oppenc mode on */
        e->security |= SEC_OPPENCRYPT;
        crypt_opportunistic_encrypt(e);
        break;

      case 'o': /* oppenc mode off */
        e->security &= ~SEC_OPPENCRYPT;
        break;

      case 'S': /* (s)ign in oppenc mode */
        e->security |= SEC_SIGN;
        break;

      case 's': /* (s)ign */
        e->security &= ~SEC_ENCRYPT;
        e->security |= SEC_SIGN;
        break;
    }
  }

  return e->security;
}

/**
 * pgp_gpgme_send_menu - Ask the user whether to sign and/or encrypt the email - Implements CryptModuleSpecs::send_menu() - @ingroup crypto_send_menu
 */
SecurityFlags pgp_gpgme_send_menu(struct Email *e)
{
  return gpgme_send_menu(e, false);
}

/**
 * smime_gpgme_send_menu - Ask the user whether to sign and/or encrypt the email - Implements CryptModuleSpecs::send_menu() - @ingroup crypto_send_menu
 */
SecurityFlags smime_gpgme_send_menu(struct Email *e)
{
  return gpgme_send_menu(e, true);
}

/**
 * verify_sender - Verify the sender of a message
 * @param e Email
 * @retval true Sender is verified
 */
static bool verify_sender(struct Email *e)
{
  struct Address *sender = NULL;
  bool rc = true;

  if (!TAILQ_EMPTY(&e->env->from))
  {
    mutt_expand_aliases(&e->env->from);
    sender = TAILQ_FIRST(&e->env->from);
  }
  else if (!TAILQ_EMPTY(&e->env->sender))
  {
    mutt_expand_aliases(&e->env->sender);
    sender = TAILQ_FIRST(&e->env->sender);
  }

  if (sender)
  {
    if (SignatureKey)
    {
      gpgme_key_t key = SignatureKey;
      gpgme_user_id_t uid = NULL;
      int sender_length = buf_len(sender->mailbox);
      for (uid = key->uids; uid && rc; uid = uid->next)
      {
        int uid_length = strlen(uid->email);
        if ((uid->email[0] == '<') && (uid->email[uid_length - 1] == '>') &&
            (uid_length == (sender_length + 2)))
        {
          const char *at_sign = strchr(uid->email + 1, '@');
          if (at_sign)
          {
            /* Assume address is 'mailbox@domainname'.
             * The mailbox part is case-sensitive,
             * the domainname is not. (RFC2821) */
            const char *tmp_email = uid->email + 1;
            const char *tmp_sender = buf_string(sender->mailbox);
            /* length of mailbox part including '@' */
            int mailbox_length = at_sign - tmp_email + 1;
            int domainname_length = sender_length - mailbox_length;
            int mailbox_match, domainname_match;

            mailbox_match = mutt_strn_equal(tmp_email, tmp_sender, mailbox_length);
            tmp_email += mailbox_length;
            tmp_sender += mailbox_length;
            domainname_match = (mutt_istrn_cmp(tmp_email, tmp_sender, domainname_length) == 0);
            if (mailbox_match && domainname_match)
              rc = false;
          }
          else
          {
            if (mutt_strn_equal(uid->email + 1, buf_string(sender->mailbox), sender_length))
              rc = false;
          }
        }
      }
    }
    else
    {
      mutt_any_key_to_continue(_("Failed to verify sender"));
    }
  }
  else
  {
    mutt_any_key_to_continue(_("Failed to figure out sender"));
  }

  if (SignatureKey)
  {
    gpgme_key_unref(SignatureKey);
    SignatureKey = NULL;
  }

  return rc;
}

/**
 * smime_gpgme_verify_sender - Does the sender match the certificate? - Implements CryptModuleSpecs::smime_verify_sender() - @ingroup crypto_smime_verify_sender
 */
int smime_gpgme_verify_sender(struct Email *e, struct Message *msg)
{
  return verify_sender(e);
}

/**
 * pgp_gpgme_set_sender - Set the sender of the email - Implements CryptModuleSpecs::set_sender() - @ingroup crypto_set_sender
 */
void pgp_gpgme_set_sender(const char *sender)
{
  mutt_debug(LL_DEBUG2, "setting to: %s\n", sender);
  FREE(&CurrentSender);
  CurrentSender = mutt_str_dup(sender);
}

/**
 * mutt_gpgme_print_version - Get version of GPGME
 * @retval ptr GPGME version string
 */
const char *mutt_gpgme_print_version(void)
{
  return GPGME_VERSION;
}
