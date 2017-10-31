/**
 * @file
 * Wrapper for PGP/SMIME calls to GPGME
 *
 * @authors
 * Copyright (C) 1996-1997,2007 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (C) 1998-2000 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2001 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2001 Oliver Ehli <elmy@acm.org>
 * Copyright (C) 2002-2004 g10 Code GmbH
 * Copyright (C) 2010,2012-2013 Michael R. Elkins <me@sigpipe.org>
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
 *
 * crypt_gpgme.c - GPGME based crypto operations
 */

#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <gpg-error.h>
#include <gpgme.h>
#include <langinfo.h>
#ifdef ENABLE_NLS
#include <libintl.h>
#endif
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "lib/lib.h"
#include "mutt.h"
#include "address.h"
#include "alias.h"
#include "body.h"
#include "charset.h"
#include "crypt.h"
#include "envelope.h"
#include "format_flags.h"
#include "globals.h"
#include "header.h"
#include "keymap.h"
#include "mime.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "ncrypt.h"
#include "opcodes.h"
#include "options.h"
#include "pager.h"
#include "parameter.h"
#include "protos.h"
#include "rfc822.h"
#include "sort.h"
#include "state.h"

/* Values used for comparing addresses. */
#define CRYPT_KV_VALID 1
#define CRYPT_KV_ADDR 2
#define CRYPT_KV_STRING 4
#define CRYPT_KV_STRONGID 8
#define CRYPT_KV_MATCH (CRYPT_KV_ADDR | CRYPT_KV_STRING)

/**
 * struct CryptCache - Internal cache for GPGME
 */
struct CryptCache
{
  char *what;
  char *dflt;
  struct CryptCache *next;
};

/**
 * struct DnArray - An X500 Distinguished Name
 */
struct DnArray
{
  char *key;
  char *value;
};

/* We work based on user IDs, getting from a user ID to the key is
   check and does not need any memory (gpgme uses reference counting). */
/**
 * struct CryptKeyInfo - A stored PGP key
 */
struct CryptKeyInfo
{
  struct CryptKeyInfo *next;
  gpgme_key_t kobj;
  int idx;                   /**< and the user ID at this index */
  const char *uid;           /**< and for convenience point to this user ID */
  unsigned int flags;        /**< global and per uid flags (for convenience) */
  gpgme_validity_t validity; /**< uid validity (cached for convenience) */
};

/**
 * struct CryptEntry - An entry in the Select-Key menu
 */
struct CryptEntry
{
  size_t num;
  struct CryptKeyInfo *key;
};

static struct CryptCache *id_defaults = NULL;
static gpgme_key_t signature_key = NULL;
static char *current_sender = NULL;

/*
 * General helper functions.
 */

#define PKA_NOTATION_NAME "pka-address@gnupg.org"

static bool is_pka_notation(gpgme_sig_notation_t notation)
{
  return (mutt_strcmp(notation->name, PKA_NOTATION_NAME) == 0);
}

/**
 * redraw_if_needed - accommodate for a redraw if needed
 */
static void redraw_if_needed(gpgme_ctx_t ctx)
{
#if (GPGME_VERSION_NUMBER < 0x010800)
  /* gpgme_get_ctx_flag is not available in gpgme < 1.8.0. In this case, stay
   * on the safe side and always redraw. */
  (void) ctx;
  mutt_need_hard_redraw();
#else
  const char *s = gpgme_get_ctx_flag(ctx, "redraw");
  if (!s /* flag not known */ || *s /* flag true */)
  {
    mutt_need_hard_redraw();
  }
#endif
}

/**
 * digit_or_letter - Is the character a number or letter
 * @param s Only the first character of this string is tested
 * @retval true when s points to a digit or letter
 */
static int digit_or_letter(const unsigned char *s)
{
  return ((*s >= '0' && *s < '9') || (*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z'));
}

/**
 * print_utf8 - Write a UTF-8 string to a file
 *
 * Print the utf-8 encoded string BUF of length LEN bytes to stream FP. Convert
 * the character set.
 */
static void print_utf8(FILE *fp, const char *buf, size_t len)
{
  char *tstr = NULL;

  tstr = safe_malloc(len + 1);
  memcpy(tstr, buf, len);
  tstr[len] = 0;

  /* fromcode "utf-8" is sure, so we don't want
   * charset-hook corrections: flags must be 0.
   */
  mutt_convert_string(&tstr, "utf-8", Charset, 0);
  fputs(tstr, fp);
  FREE(&tstr);
}

/*
 * Key management.
 */

/**
 * crypt_keyid - Find the ID for the key
 *
 * Return the keyID for the key K.
 * Note that this string is valid as long as K is valid
 */
static const char *crypt_keyid(struct CryptKeyInfo *k)
{
  const char *s = "????????";

  if (k->kobj && k->kobj->subkeys)
  {
    s = k->kobj->subkeys->keyid;
    if ((!option(OPT_PGP_LONG_IDS)) && (strlen(s) == 16))
      /* Return only the short keyID.  */
      s += 8;
  }

  return s;
}

/**
 * crypt_long_keyid - Find the Long ID for the key
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
 * crypt_short_keyid - Get the short keyID for the key K
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
 * crypt_fpr - Get the hexstring fingerprint from the key K
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
 * @retval string fingerprint if available
 * @retval string otherwise the long keyid
 */
static const char *crypt_fpr_or_lkeyid(struct CryptKeyInfo *k)
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
 * crypt_key_abilities - Parse key flags into a string
 *
 * Note: The string is statically allocated.
 */
static char *crypt_key_abilities(int flags)
{
  static char buff[3];

  if (!(flags & KEYFLAG_CANENCRYPT))
    buff[0] = '-';
  else if (flags & KEYFLAG_PREFER_SIGNING)
    buff[0] = '.';
  else
    buff[0] = 'e';

  if (!(flags & KEYFLAG_CANSIGN))
    buff[1] = '-';
  else if (flags & KEYFLAG_PREFER_ENCRYPTION)
    buff[1] = '.';
  else
    buff[1] = 's';

  buff[2] = '\0';

  return buff;
}

/**
 * crypt_flags - Parse the key flags into a single character
 *
 * The returned character describes the most important flag.
 */
static char crypt_flags(int flags)
{
  if (flags & KEYFLAG_REVOKED)
    return 'R';
  else if (flags & KEYFLAG_EXPIRED)
    return 'X';
  else if (flags & KEYFLAG_DISABLED)
    return 'd';
  else if (flags & KEYFLAG_CRITICAL)
    return 'c';
  else
    return ' ';
}

/**
 * crypt_copy_key - Return a copy of KEY
 */
static struct CryptKeyInfo *crypt_copy_key(struct CryptKeyInfo *key)
{
  struct CryptKeyInfo *k = NULL;

  k = safe_calloc(1, sizeof(*k));
  k->kobj = key->kobj;
  gpgme_key_ref(key->kobj);
  k->idx = key->idx;
  k->uid = key->uid;
  k->flags = key->flags;
  k->validity = key->validity;

  return k;
}

/**
 * crypt_free_key - Release all the keys in a list
 * @param keylist List of keys
 */
static void crypt_free_key(struct CryptKeyInfo **keylist)
{
  struct CryptKeyInfo *k = NULL;

  if (!keylist)
    return;

  while (*keylist)
  {
    k = *keylist;
    *keylist = (*keylist)->next;

    gpgme_key_unref(k->kobj);
    FREE(&k);
  }
}

/**
 * crypt_key_is_valid - Is the key valid
 * @retval true when key K is valid
 */
static bool crypt_key_is_valid(struct CryptKeyInfo *k)
{
  if (k->flags & KEYFLAG_CANTUSE)
    return false;
  return true;
}

/**
 * crypt_id_is_strong - Is the key strong
 * @retval true when validity of KEY is sufficient
 */
static int crypt_id_is_strong(struct CryptKeyInfo *key)
{
  unsigned int is_strong = 0;

  if ((key->flags & KEYFLAG_ISX509))
    return 1;

  switch (key->validity)
  {
    case GPGME_VALIDITY_UNKNOWN:
    case GPGME_VALIDITY_UNDEFINED:
    case GPGME_VALIDITY_NEVER:
    case GPGME_VALIDITY_MARGINAL:
      is_strong = 0;
      break;

    case GPGME_VALIDITY_FULL:
    case GPGME_VALIDITY_ULTIMATE:
      is_strong = 1;
      break;
  }

  return is_strong;
}

/**
 * crypt_id_is_valid - Is key ID valid
 * ~return true when the KEY is valid, i.e. not marked as unusable
 */
static int crypt_id_is_valid(struct CryptKeyInfo *key)
{
  return !(key->flags & KEYFLAG_CANTUSE);
}

/**
 * crypt_id_matches_addr - Does key ID match the address
 *
 * Return a bit vector describing how well the addresses ADDR and U_ADDR match
 * and whether KEY is valid.
 */
static int crypt_id_matches_addr(struct Address *addr, struct Address *u_addr,
                                 struct CryptKeyInfo *key)
{
  int rv = 0;

  if (crypt_id_is_valid(key))
    rv |= CRYPT_KV_VALID;

  if (crypt_id_is_strong(key))
    rv |= CRYPT_KV_STRONGID;

  if (addr->mailbox && u_addr->mailbox &&
      (mutt_strcasecmp(addr->mailbox, u_addr->mailbox) == 0))
    rv |= CRYPT_KV_ADDR;

  if (addr->personal && u_addr->personal &&
      (mutt_strcasecmp(addr->personal, u_addr->personal) == 0))
    rv |= CRYPT_KV_STRING;

  return rv;
}

/*
 * GPGME convenient functions.
 */

/**
 * create_gpgme_context - Create a new GPGME context
 * @param for_smime If set, protocol of the context is set to CMS
 */
static gpgme_ctx_t create_gpgme_context(int for_smime)
{
  gpgme_error_t err;
  gpgme_ctx_t ctx;

  err = gpgme_new(&ctx);
  if (err)
  {
    mutt_error(_("error creating gpgme context: %s\n"), gpgme_strerror(err));
    sleep(2);
    mutt_exit(1);
  }

  if (for_smime)
  {
    err = gpgme_set_protocol(ctx, GPGME_PROTOCOL_CMS);
    if (err)
    {
      mutt_error(_("error enabling CMS protocol: %s\n"), gpgme_strerror(err));
      sleep(2);
      mutt_exit(1);
    }
  }

  return ctx;
}

/**
 * create_gpgme_data - Create a new GPGME data object
 *
 * This is a wrapper to die on error.
 */
static gpgme_data_t create_gpgme_data(void)
{
  gpgme_error_t err;
  gpgme_data_t data;

  err = gpgme_data_new(&data);
  if (err)
  {
    mutt_error(_("error creating gpgme data object: %s\n"), gpgme_strerror(err));
    sleep(2);
    mutt_exit(1);
  }
  return data;
}

/**
 * body_to_data_object - Create GPGME object from the mail body
 *
 * Create a new GPGME Data object from the mail body A.  With CONVERT passed as
 * true, the lines are converted to CR,LF if required.  Return NULL on error or
 * the gpgme_data_t object on success.
 */
static gpgme_data_t body_to_data_object(struct Body *a, int convert)
{
  char tempfile[_POSIX_PATH_MAX];
  FILE *fptmp = NULL;
  int err = 0;
  gpgme_data_t data;

  mutt_mktemp(tempfile, sizeof(tempfile));
  fptmp = safe_fopen(tempfile, "w+");
  if (!fptmp)
  {
    mutt_perror(tempfile);
    return NULL;
  }

  mutt_write_mime_header(a, fptmp);
  fputc('\n', fptmp);
  mutt_write_mime_body(a, fptmp);

  if (convert)
  {
    int c, hadcr = 0;
    unsigned char buf[1];

    data = create_gpgme_data();
    rewind(fptmp);
    while ((c = fgetc(fptmp)) != EOF)
    {
      if (c == '\r')
        hadcr = 1;
      else
      {
        if (c == '\n' && !hadcr)
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
    safe_fclose(&fptmp);
    gpgme_data_seek(data, 0, SEEK_SET);
  }
  else
  {
    safe_fclose(&fptmp);
    err = gpgme_data_new_from_file(&data, tempfile, 1);
  }
  unlink(tempfile);
  if (err)
  {
    mutt_error(_("error allocating data object: %s\n"), gpgme_strerror(err));
    return NULL;
  }

  return data;
}

/**
 * file_to_data_object - Create GPGME data object from file
 *
 * Create a GPGME data object from the stream FP but limit the object
 * to LENGTH bytes starting at OFFSET bytes from the beginning of the file.
 */
static gpgme_data_t file_to_data_object(FILE *fp, long offset, long length)
{
  int err = 0;
  gpgme_data_t data;

  err = gpgme_data_new_from_filepart(&data, NULL, fp, offset, length);
  if (err)
  {
    mutt_error(_("error allocating data object: %s\n"), gpgme_strerror(err));
    return NULL;
  }

  return data;
}

/**
 * data_object_to_stream - Write a GPGME data object to a file
 */
static int data_object_to_stream(gpgme_data_t data, FILE *fp)
{
  int err;
  char buf[4096];
  ssize_t nread;

  err = ((gpgme_data_seek(data, 0, SEEK_SET) == -1) ? gpgme_error_from_errno(errno) : 0);
  if (err)
  {
    mutt_error(_("error rewinding data object: %s\n"), gpgme_strerror(err));
    return -1;
  }

  while ((nread = gpgme_data_read(data, buf, sizeof(buf))) > 0)
  {
    /* fixme: we are not really converting CRLF to LF but just
         skipping CR. Doing it correctly needs a more complex logic */
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
    mutt_error(_("error reading data object: %s\n"), strerror(errno));
    return -1;
  }
  return 0;
}

/**
 * data_object_to_tempfile - Copy a data object to a temporary file
 *
 * The tempfile name may be optionally passed in.
 * If ret_fp is passed in, the file will be rewound, left open, and returned
 * via that parameter.
 * The tempfile name is returned, and must be freed.
 */
static char *data_object_to_tempfile(gpgme_data_t data, char *tempf, FILE **ret_fp)
{
  int err;
  char tempfb[_POSIX_PATH_MAX];
  FILE *fp = NULL;
  ssize_t nread = 0;

  if (!tempf)
  {
    mutt_mktemp(tempfb, sizeof(tempfb));
    tempf = tempfb;
  }
  fp = safe_fopen(tempf, tempf == tempfb ? "w+" : "a+");
  if (!fp)
  {
    mutt_perror(_("Can't create temporary file"));
    return NULL;
  }

  err = ((gpgme_data_seek(data, 0, SEEK_SET) == -1) ? gpgme_error_from_errno(errno) : 0);
  if (!err)
  {
    char buf[4096];

    while ((nread = gpgme_data_read(data, buf, sizeof(buf))) > 0)
    {
      if (fwrite(buf, nread, 1, fp) != 1)
      {
        mutt_perror(tempf);
        safe_fclose(&fp);
        unlink(tempf);
        return NULL;
      }
    }
  }
  if (ret_fp)
    rewind(fp);
  else
    safe_fclose(&fp);
  if (nread == -1)
  {
    mutt_error(_("error reading data object: %s\n"), gpgme_strerror(err));
    unlink(tempf);
    safe_fclose(&fp);
    return NULL;
  }
  if (ret_fp)
    *ret_fp = fp;
  return safe_strdup(tempf);
}

static void free_recipient_set(gpgme_key_t **p_rset)
{
  gpgme_key_t *rset, k;

  if (!p_rset)
    return;

  rset = *p_rset;
  if (!rset)
    return;

  while (*rset)
  {
    k = *rset;
    gpgme_key_unref(k);
    rset++;
  }

  FREE(p_rset);
}

/**
 * create_recipient_set - Create a GpgmeRecipientSet from a string of keys
 *
 * The keys must be space delimited.
 */
static gpgme_key_t *create_recipient_set(const char *keylist, gpgme_protocol_t protocol)
{
  int err;
  const char *s = NULL;
  char buf[100];
  int i;
  gpgme_key_t *rset = NULL;
  unsigned int rset_n = 0;
  gpgme_key_t key = NULL;
  gpgme_ctx_t context = NULL;

  err = gpgme_new(&context);
  if (!err)
    err = gpgme_set_protocol(context, protocol);

  if (!err)
  {
    s = keylist;
    do
    {
      while (*s == ' ')
        s++;
      for (i = 0; *s && *s != ' ' && i < sizeof(buf) - 1;)
        buf[i++] = *s++;
      buf[i] = 0;
      if (*buf)
      {
        if (i > 1 && buf[i - 1] == '!')
        {
          /* The user selected to override the validity of that
                   key. */
          buf[i - 1] = 0;

          err = gpgme_get_key(context, buf, &key, 0);
          if (!err)
            key->uids->validity = GPGME_VALIDITY_FULL;
          buf[i - 1] = '!';
        }
        else
          err = gpgme_get_key(context, buf, &key, 0);

        safe_realloc(&rset, sizeof(*rset) * (rset_n + 1));
        if (!err)
          rset[rset_n++] = key;
        else
        {
          mutt_error(_("error adding recipient `%s': %s\n"), buf, gpgme_strerror(err));
          rset[rset_n] = NULL;
          free_recipient_set(&rset);
          gpgme_release(context);
          return NULL;
        }
      }
    } while (*s);
  }

  /* NULL terminate.  */
  safe_realloc(&rset, sizeof(*rset) * (rset_n + 1));
  rset[rset_n++] = NULL;

  if (context)
    gpgme_release(context);

  return rset;
}

/**
 * set_signer - Make sure that the correct signer is set
 * @retval 0 on success
 */
static int set_signer(gpgme_ctx_t ctx, int for_smime)
{
  char *signid = for_smime ? SmimeDefaultKey : PgpSignAs;
  gpgme_error_t err;
  gpgme_ctx_t listctx;
  gpgme_key_t key, key2;

  if (!signid || !*signid)
    return 0;

  listctx = create_gpgme_context(for_smime);
  err = gpgme_op_keylist_start(listctx, signid, 1);
  if (!err)
    err = gpgme_op_keylist_next(listctx, &key);
  if (err)
  {
    gpgme_release(listctx);
    mutt_error(_("secret key `%s' not found: %s\n"), signid, gpgme_strerror(err));
    return -1;
  }
  err = gpgme_op_keylist_next(listctx, &key2);
  if (!err)
  {
    gpgme_key_unref(key);
    gpgme_key_unref(key2);
    gpgme_release(listctx);
    mutt_error(_("ambiguous specification of secret key `%s'\n"), signid);
    return -1;
  }
  gpgme_op_keylist_end(listctx);
  gpgme_release(listctx);

  gpgme_signers_clear(ctx);
  err = gpgme_signers_add(ctx, key);
  gpgme_key_unref(key);
  if (err)
  {
    mutt_error(_("error setting secret key `%s': %s\n"), signid, gpgme_strerror(err));
    return -1;
  }
  return 0;
}

static gpgme_error_t set_pka_sig_notation(gpgme_ctx_t ctx)
{
  gpgme_error_t err;

  err = gpgme_sig_notation_add(ctx, PKA_NOTATION_NAME, current_sender, 0);

  if (err)
  {
    mutt_error(_("error setting PKA signature notation: %s\n"), gpgme_strerror(err));
    mutt_sleep(2);
  }

  return err;
}

/**
 * encrypt_gpgme_object - Encrypt the GPGPME data object
 *
 * Encrypt the gpgme data object PLAINTEXT to the recipients in RSET and return
 * an allocated filename to a temporary file containing the enciphered text.
 * With USE_SMIME set to true, the smime backend is used.  With COMBINED_SIGNED
 * a PGP message is signed and encrypted.  Returns NULL in case of error
 */
static char *encrypt_gpgme_object(gpgme_data_t plaintext, gpgme_key_t *rset,
                                  int use_smime, int combined_signed)
{
  gpgme_error_t err;
  gpgme_ctx_t ctx;
  gpgme_data_t ciphertext;
  char *outfile = NULL;

  ctx = create_gpgme_context(use_smime);
  if (!use_smime)
    gpgme_set_armor(ctx, 1);

  ciphertext = create_gpgme_data();

  if (combined_signed)
  {
    if (set_signer(ctx, use_smime))
    {
      gpgme_data_release(ciphertext);
      gpgme_release(ctx);
      return NULL;
    }

    if (option(OPT_CRYPT_USE_PKA))
    {
      err = set_pka_sig_notation(ctx);
      if (err)
      {
        gpgme_data_release(ciphertext);
        gpgme_release(ctx);
        return NULL;
      }
    }

    err = gpgme_op_encrypt_sign(ctx, rset, GPGME_ENCRYPT_ALWAYS_TRUST, plaintext, ciphertext);
  }
  else
    err = gpgme_op_encrypt(ctx, rset, GPGME_ENCRYPT_ALWAYS_TRUST, plaintext, ciphertext);
  redraw_if_needed(ctx);
  if (err)
  {
    mutt_error(_("error encrypting data: %s\n"), gpgme_strerror(err));
    gpgme_data_release(ciphertext);
    gpgme_release(ctx);
    return NULL;
  }

  gpgme_release(ctx);

  outfile = data_object_to_tempfile(ciphertext, NULL, NULL);
  gpgme_data_release(ciphertext);
  return outfile;
}

static void strlower(char *s)
{
  for (; *s; ++s)
    *s = tolower(*s);
}

/**
 * get_micalg - Find the "micalg" parameter from the last GPGME operation
 *
 * Find the "micalg" parameter from the last Gpgme operation on context CTX.
 * It is expected that this operation was a sign operation.  Return the
 * algorithm name as a C string in buffer BUF which must have been allocated by
 * the caller with size BUFLEN.  Returns 0 on success or -1 in case of an
 * error.  The return string is truncated to BUFLEN - 1.
 */
static int get_micalg(gpgme_ctx_t ctx, int use_smime, char *buf, size_t buflen)
{
  gpgme_sign_result_t result = NULL;
  const char *algorithm_name = NULL;

  if (buflen < 5)
    return -1;

  *buf = 0;
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
        strlower(buf);
      }
      else
      {
        /* convert GPGME raw hash name to RFC3156 format */
        snprintf(buf, buflen, "pgp-%s", algorithm_name);
        strlower(buf + 4);
      }
    }
  }

  return *buf ? 0 : -1;
}

static void print_time(time_t t, struct State *s)
{
  char p[STRING];

  strftime(p, sizeof(p), nl_langinfo(D_T_FMT), localtime(&t));
  state_puts(p, s);
}

/*
 * Implementation of `sign_message'.
 */

/**
 * sign_message - Sign a message
 * @param a         Message to sign
 * @param use_smime If set, use SMIME instead of PGP
 * @retval ptr  new Body
 * @retval NULL error
 */
static struct Body *sign_message(struct Body *a, int use_smime)
{
  struct Body *t = NULL;
  char *sigfile = NULL;
  int err = 0;
  char buf[100];
  gpgme_ctx_t ctx;
  gpgme_data_t message, signature;
  gpgme_sign_result_t sigres;

  convert_to_7bit(a); /* Signed data _must_ be in 7-bit format. */

  message = body_to_data_object(a, 1);
  if (!message)
    return NULL;
  signature = create_gpgme_data();

  ctx = create_gpgme_context(use_smime);
  if (!use_smime)
    gpgme_set_armor(ctx, 1);

  if (set_signer(ctx, use_smime))
  {
    gpgme_data_release(signature);
    gpgme_data_release(message);
    gpgme_release(ctx);
    return NULL;
  }

  if (option(OPT_CRYPT_USE_PKA))
  {
    err = set_pka_sig_notation(ctx);
    if (err)
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
  if (err)
  {
    gpgme_data_release(signature);
    gpgme_release(ctx);
    mutt_error(_("error signing data: %s\n"), gpgme_strerror(err));
    return NULL;
  }
  /* Check for zero signatures generated.  This can occur when $pgp_sign_as is
   * unset and there is no default key specified in ~/.gnupg/gpg.conf
   */
  sigres = gpgme_op_sign_result(ctx);
  if (!sigres->signatures)
  {
    gpgme_data_release(signature);
    gpgme_release(ctx);
    mutt_error(_("$pgp_sign_as unset and no default key specified in "
                 "~/.gnupg/gpg.conf"));
    return NULL;
  }

  sigfile = data_object_to_tempfile(signature, NULL, NULL);
  gpgme_data_release(signature);
  if (!sigfile)
  {
    gpgme_release(ctx);
    return NULL;
  }

  t = mutt_new_body();
  t->type = TYPEMULTIPART;
  t->subtype = safe_strdup("signed");
  t->encoding = ENC7BIT;
  t->use_disp = false;
  t->disposition = DISPINLINE;

  mutt_generate_boundary(&t->parameter);
  mutt_set_parameter("protocol",
                     use_smime ? "application/pkcs7-signature" :
                                 "application/pgp-signature",
                     &t->parameter);
  /* Get the micalg from gpgme.  Old gpgme versions don't support this
     for S/MIME so we assume sha-1 in this case. */
  if (!get_micalg(ctx, use_smime, buf, sizeof(buf)))
    mutt_set_parameter("micalg", buf, &t->parameter);
  else if (use_smime)
    mutt_set_parameter("micalg", "sha1", &t->parameter);
  gpgme_release(ctx);

  t->parts = a;
  a = t;

  t->parts->next = mutt_new_body();
  t = t->parts->next;
  t->type = TYPEAPPLICATION;
  if (use_smime)
  {
    t->subtype = safe_strdup("pkcs7-signature");
    mutt_set_parameter("name", "smime.p7s", &t->parameter);
    t->encoding = ENCBASE64;
    t->use_disp = true;
    t->disposition = DISPATTACH;
    t->d_filename = safe_strdup("smime.p7s");
  }
  else
  {
    t->subtype = safe_strdup("pgp-signature");
    mutt_set_parameter("name", "signature.asc", &t->parameter);
    t->use_disp = false;
    t->disposition = DISPNONE;
    t->encoding = ENC7BIT;
  }
  t->filename = sigfile;
  t->unlink = true; /* ok to remove this file after sending. */

  return a;
}

struct Body *pgp_gpgme_sign_message(struct Body *a)
{
  return sign_message(a, 0);
}

struct Body *smime_gpgme_sign_message(struct Body *a)
{
  return sign_message(a, 1);
}

/*
 * Implementation of `encrypt_message'.
 */

/**
 * pgp_gpgme_encrypt_message - Encrypt a message
 *
 * Encrypt the mail body A to all keys given as space separated keyids
 * or fingerprints in KEYLIST and return the encrypted body.
 */
struct Body *pgp_gpgme_encrypt_message(struct Body *a, char *keylist, int sign)
{
  char *outfile = NULL;
  struct Body *t = NULL;
  gpgme_key_t *rset = NULL;
  gpgme_data_t plaintext;

  rset = create_recipient_set(keylist, GPGME_PROTOCOL_OpenPGP);
  if (!rset)
    return NULL;

  if (sign)
    convert_to_7bit(a);
  plaintext = body_to_data_object(a, 0);
  if (!plaintext)
  {
    free_recipient_set(&rset);
    return NULL;
  }

  outfile = encrypt_gpgme_object(plaintext, rset, 0, sign);
  gpgme_data_release(plaintext);
  free_recipient_set(&rset);
  if (!outfile)
    return NULL;

  t = mutt_new_body();
  t->type = TYPEMULTIPART;
  t->subtype = safe_strdup("encrypted");
  t->encoding = ENC7BIT;
  t->use_disp = false;
  t->disposition = DISPINLINE;

  mutt_generate_boundary(&t->parameter);
  mutt_set_parameter("protocol", "application/pgp-encrypted", &t->parameter);

  t->parts = mutt_new_body();
  t->parts->type = TYPEAPPLICATION;
  t->parts->subtype = safe_strdup("pgp-encrypted");
  t->parts->encoding = ENC7BIT;

  t->parts->next = mutt_new_body();
  t->parts->next->type = TYPEAPPLICATION;
  t->parts->next->subtype = safe_strdup("octet-stream");
  t->parts->next->encoding = ENC7BIT;
  t->parts->next->filename = outfile;
  t->parts->next->use_disp = true;
  t->parts->next->disposition = DISPATTACH;
  t->parts->next->unlink = true; /* delete after sending the message */
  t->parts->next->d_filename = safe_strdup("msg.asc"); /* non pgp/mime
                                                           can save */

  return t;
}

/*
 * Implementation of `smime_build_smime_entity'.
 */

/**
 * smime_gpgme_build_smime_entity - Encrypt the email body to all recipients
 *
 * Encrypt the mail body A to all keys given as space separated fingerprints in
 * KEYLIST and return the S/MIME encrypted body.
 */
struct Body *smime_gpgme_build_smime_entity(struct Body *a, char *keylist)
{
  char *outfile = NULL;
  struct Body *t = NULL;
  gpgme_key_t *rset = NULL;
  gpgme_data_t plaintext;

  rset = create_recipient_set(keylist, GPGME_PROTOCOL_CMS);
  if (!rset)
    return NULL;

  /* OpenSSL converts line endings to crlf when encrypting.  Some
   * clients depend on this for signed+encrypted messages: they do not
   * convert line endings between decrypting and checking the
   * signature.  See #3904. */
  plaintext = body_to_data_object(a, 1);
  if (!plaintext)
  {
    free_recipient_set(&rset);
    return NULL;
  }

  outfile = encrypt_gpgme_object(plaintext, rset, 1, 0);
  gpgme_data_release(plaintext);
  free_recipient_set(&rset);
  if (!outfile)
    return NULL;

  t = mutt_new_body();
  t->type = TYPEAPPLICATION;
  t->subtype = safe_strdup("pkcs7-mime");
  mutt_set_parameter("name", "smime.p7m", &t->parameter);
  mutt_set_parameter("smime-type", "enveloped-data", &t->parameter);
  t->encoding = ENCBASE64; /* The output of OpenSSL SHOULD be binary */
  t->use_disp = true;
  t->disposition = DISPATTACH;
  t->d_filename = safe_strdup("smime.p7m");
  t->filename = outfile;
  t->unlink = true; /* delete after sending the message */
  t->parts = 0;
  t->next = 0;

  return t;
}

/*
 * Implementation of `verify_one'.
 */

/**
 * show_sig_summary - Show a signature summary
 * @retval 1 if there is is a severe warning
 *
 * Display the common attributes of the signature summary SUM.
 */
static int show_sig_summary(unsigned long sum, gpgme_ctx_t ctx, gpgme_key_t key,
                            int idx, struct State *s, gpgme_signature_t sig)
{
  if (!key)
    return 1;

  int severe = 0;

  if ((sum & GPGME_SIGSUM_KEY_REVOKED))
  {
    state_puts(_("Warning: One of the keys has been revoked\n"), s);
    severe = 1;
  }

  if ((sum & GPGME_SIGSUM_KEY_EXPIRED))
  {
    time_t at = key->subkeys->expires ? key->subkeys->expires : 0;
    if (at)
    {
      state_puts(_("Warning: The key used to create the "
                   "signature expired at: "),
                 s);
      print_time(at, s);
      state_puts("\n", s);
    }
    else
      state_puts(_("Warning: At least one certification key "
                   "has expired\n"),
                 s);
  }

  if ((sum & GPGME_SIGSUM_SIG_EXPIRED))
  {
    gpgme_verify_result_t result;
    gpgme_signature_t sig2;
    unsigned int i;

    result = gpgme_op_verify_result(ctx);

    for (sig2 = result->signatures, i = 0; sig2 && (i < idx); sig2 = sig2->next, i++)
      ;

    state_puts(_("Warning: The signature expired at: "), s);
    print_time(sig2 ? sig2->exp_timestamp : 0, s);
    state_puts("\n", s);
  }

  if ((sum & GPGME_SIGSUM_KEY_MISSING))
    state_puts(_("Can't verify due to a missing "
                 "key or certificate\n"),
               s);

  if ((sum & GPGME_SIGSUM_CRL_MISSING))
  {
    state_puts(_("The CRL is not available\n"), s);
    severe = 1;
  }

  if ((sum & GPGME_SIGSUM_CRL_TOO_OLD))
  {
    state_puts(_("Available CRL is too old\n"), s);
    severe = 1;
  }

  if ((sum & GPGME_SIGSUM_BAD_POLICY))
    state_puts(_("A policy requirement was not met\n"), s);

  if ((sum & GPGME_SIGSUM_SYS_ERROR))
  {
    const char *t0 = NULL, *t1 = NULL;
    gpgme_verify_result_t result;
    gpgme_signature_t sig2;
    unsigned int i;

    state_puts(_("A system error occurred"), s);

    /* Try to figure out some more detailed system error information. */
    result = gpgme_op_verify_result(ctx);
    for (sig2 = result->signatures, i = 0; sig2 && (i < idx); sig2 = sig2->next, i++)
      ;
    if (sig2)
    {
      t0 = "";
      t1 = sig2->wrong_key_usage ? "Wrong_Key_Usage" : "";
    }

    if (t0 || t1)
    {
      state_puts(": ", s);
      if (t0)
        state_puts(t0, s);
      if (t1 && !(t0 && (strcmp(t0, t1) == 0)))
      {
        if (t0)
          state_puts(",", s);
        state_puts(t1, s);
      }
    }
    state_puts("\n", s);
  }

  if (option(OPT_CRYPT_USE_PKA))
  {
    if (sig->pka_trust == 1 && sig->pka_address)
    {
      state_puts(_("WARNING: PKA entry does not match "
                   "signer's address: "),
                 s);
      state_puts(sig->pka_address, s);
      state_puts("\n", s);
    }
    else if (sig->pka_trust == 2 && sig->pka_address)
    {
      state_puts(_("PKA verified signer's address is: "), s);
      state_puts(sig->pka_address, s);
      state_puts("\n", s);
    }
  }

  return severe;
}

static void show_fingerprint(gpgme_key_t key, struct State *state)
{
  const char *s = NULL;
  int is_pgp;
  char *buf = NULL, *p = NULL;
  const char *prefix = _("Fingerprint: ");

  if (!key)
    return;
  s = key->subkeys ? key->subkeys->fpr : NULL;
  if (!s)
    return;
  is_pgp = (key->protocol == GPGME_PROTOCOL_OpenPGP);

  buf = safe_malloc(strlen(prefix) + strlen(s) * 4 + 2);
  strcpy(buf, prefix);
  p = buf + strlen(buf);
  if (is_pgp && strlen(s) == 40)
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
      if (is_pgp && i == 7)
        *p++ = ' ';
    }
  }

  /* just in case print remaining odd digits */
  for (; *s; s++)
    *p++ = *s;
  *p++ = '\n';
  *p = 0;
  state_puts(buf, state);
  FREE(&buf);
}

/**
 * show_one_sig_validity - Show the validity of a key used for one signature
 */
static void show_one_sig_validity(gpgme_ctx_t ctx, int idx, struct State *s)
{
  gpgme_verify_result_t result = NULL;
  gpgme_signature_t sig = NULL;
  const char *txt = NULL;

  result = gpgme_op_verify_result(ctx);
  if (result)
    for (sig = result->signatures; sig && (idx > 0); sig = sig->next, idx--)
      ;

  switch (sig ? sig->validity : 0)
  {
    case GPGME_VALIDITY_UNKNOWN:
      txt = _("WARNING: We have NO indication whether "
              "the key belongs to the person named "
              "as shown above\n");
      break;
    case GPGME_VALIDITY_UNDEFINED:
      break;
    case GPGME_VALIDITY_NEVER:
      txt = _("WARNING: The key does NOT BELONG to "
              "the person named as shown above\n");
      break;
    case GPGME_VALIDITY_MARGINAL:
      txt = _("WARNING: It is NOT certain that the key "
              "belongs to the person named as shown above\n");
      break;
    case GPGME_VALIDITY_FULL:
    case GPGME_VALIDITY_ULTIMATE:
      txt = NULL;
      break;
  }
  if (txt)
    state_puts(txt, s);
}

static void print_smime_keyinfo(const char *msg, gpgme_signature_t sig,
                                gpgme_key_t key, struct State *s)
{
  int msgwid;
  gpgme_user_id_t uids = NULL;
  bool aka = false;

  state_puts(msg, s);
  state_puts(" ", s);
  /* key is NULL when not present in the user's keyring */
  if (key)
  {
    for (uids = key->uids; uids; uids = uids->next)
    {
      if (uids->revoked)
        continue;
      if (aka)
      {
        msgwid = mutt_strwidth(msg) - mutt_strwidth(_("aka: ")) + 1;
        if (msgwid < 0)
          msgwid = 0;
        for (int i = 0; i < msgwid; i++)
          state_puts(" ", s);
        state_puts(_("aka: "), s);
      }
      state_puts(uids->uid, s);
      state_puts("\n", s);

      aka = true;
    }
  }
  else
  {
    state_puts(_("KeyID "), s);
    state_puts(sig->fpr, s);
    state_puts("\n", s);
  }

  /* timestamp is 0 when verification failed.
     "Jan 1 1970" is not the created date. */
  if (sig->timestamp)
  {
    msgwid = mutt_strwidth(msg) - mutt_strwidth(_("created: ")) + 1;
    if (msgwid < 0)
      msgwid = 0;
    for (int i = 0; i < msgwid; i++)
      state_puts(" ", s);
    state_puts(_("created: "), s);
    print_time(sig->timestamp, s);
    state_puts("\n", s);
  }
}

/**
 * show_one_sig_status - Show information about one signature
 * @retval  0 Normal procession
 * @retval  1 A bad signature
 * @retval  2 A signature with a warning
 * @retval -1 No more signature
 *
 * This function is called with the context CTX of a successful verification
 * operation and the enumerator IDX which should start at 0 and increment for
 * each call/signature.
 */
static int show_one_sig_status(gpgme_ctx_t ctx, int idx, struct State *s)
{
  const char *fpr = NULL;
  gpgme_key_t key = NULL;
  int i;
  bool anybad = false, anywarn = false;
  unsigned int sum;
  gpgme_verify_result_t result;
  gpgme_signature_t sig;
  gpgme_error_t err = GPG_ERR_NO_ERROR;
  char buf[LONG_STRING];

  result = gpgme_op_verify_result(ctx);
  if (result)
  {
    /* FIXME: this code should use a static variable and remember
         the current position in the list of signatures, IMHO.
         -moritz.  */

    for (i = 0, sig = result->signatures; sig && (i < idx); i++, sig = sig->next)
      ;
    if (!sig)
      return -1; /* Signature not found.  */

    if (signature_key)
    {
      gpgme_key_unref(signature_key);
      signature_key = NULL;
    }

    fpr = sig->fpr;
    sum = sig->summary;

    if (gpg_err_code(sig->status) != GPG_ERR_NO_ERROR)
      anybad = true;

    if (gpg_err_code(sig->status) != GPG_ERR_NO_PUBKEY)
    {
      err = gpgme_get_key(ctx, fpr, &key, 0); /* secret key?  */
      if (!err)
      {
        if (!signature_key)
          signature_key = key;
      }
      else
      {
        key = NULL; /* Old gpgme versions did not set KEY to NULL on
                         error.   Do it here to avoid a double free. */
      }
    }
    else
    {
      /* pubkey not present */
    }

    if (!s || !s->fpout || !(s->flags & MUTT_DISPLAY))
      ; /* No state information so no way to print anything. */
    else if (err)
    {
      snprintf(buf, sizeof(buf),
               _("Error getting key information for KeyID %s: %s\n"), fpr,
               gpgme_strerror(err));
      state_puts(buf, s);
      anybad = true;
    }
    else if ((sum & GPGME_SIGSUM_GREEN))
    {
      print_smime_keyinfo(_("Good signature from:"), sig, key, s);
      if (show_sig_summary(sum, ctx, key, idx, s, sig))
        anywarn = true;
      show_one_sig_validity(ctx, idx, s);
    }
    else if ((sum & GPGME_SIGSUM_RED))
    {
      print_smime_keyinfo(_("*BAD* signature from:"), sig, key, s);
      show_sig_summary(sum, ctx, key, idx, s, sig);
    }
    else if (!anybad && key && (key->protocol == GPGME_PROTOCOL_OpenPGP))
    { /* We can't decide (yellow) but this is a PGP key with a good
           signature, so we display what a PGP user expects: The name,
           fingerprint and the key validity (which is neither fully or
           ultimate). */
      print_smime_keyinfo(_("Good signature from:"), sig, key, s);
      show_one_sig_validity(ctx, idx, s);
      show_fingerprint(key, s);
      if (show_sig_summary(sum, ctx, key, idx, s, sig))
        anywarn = true;
    }
    else /* can't decide (yellow) */
    {
      print_smime_keyinfo(_("Problem signature from:"), sig, key, s);
      /* 0 indicates no expiration */
      if (sig->exp_timestamp)
      {
        /* L10N:
             This is trying to match the width of the
             "Problem signature from:" translation just above. */
        state_puts(_("               expires: "), s);
        print_time(sig->exp_timestamp, s);
        state_puts("\n", s);
      }
      show_sig_summary(sum, ctx, key, idx, s, sig);
      anywarn = true;
    }

    if (key != signature_key)
      gpgme_key_unref(key);
  }

  return anybad ? 1 : anywarn ? 2 : 0;
}

/**
 * verify_one - Do the actual verification step
 *
 * With IS_SMIME set to true we assume S/MIME (surprise!)
 */
static int verify_one(struct Body *sigbdy, struct State *s, const char *tempfile, int is_smime)
{
  int badsig = -1;
  int anywarn = 0;
  int err;
  gpgme_ctx_t ctx;
  gpgme_data_t signature, message;

  signature = file_to_data_object(s->fpin, sigbdy->offset, sigbdy->length);
  if (!signature)
    return -1;

  /* We need to tell gpgme about the encoding because the backend can't
     auto-detect plain base-64 encoding which is used by S/MIME. */
  if (is_smime)
    gpgme_data_set_encoding(signature, GPGME_DATA_ENCODING_BASE64);

  err = gpgme_data_new_from_file(&message, tempfile, 1);
  if (err)
  {
    gpgme_data_release(signature);
    mutt_error(_("error allocating data object: %s\n"), gpgme_strerror(err));
    return -1;
  }
  ctx = create_gpgme_context(is_smime);

  /* Note: We don't need a current time output because GPGME avoids
     such an attack by separating the meta information from the
     data. */
  state_attach_puts(_("[-- Begin signature information --]\n"), s);

  err = gpgme_op_verify(ctx, signature, message, NULL);
  gpgme_data_release(message);
  gpgme_data_release(signature);

  redraw_if_needed(ctx);
  if (err)
  {
    char buf[200];

    snprintf(buf, sizeof(buf) - 1, _("Error: verification failed: %s\n"),
             gpgme_strerror(err));
    state_puts(buf, s);
  }
  else
  { /* Verification succeeded, see what the result is. */
    int res, idx;
    int anybad = 0;
    gpgme_verify_result_t verify_result;

    if (signature_key)
    {
      gpgme_key_unref(signature_key);
      signature_key = NULL;
    }

    verify_result = gpgme_op_verify_result(ctx);
    if (verify_result && verify_result->signatures)
    {
      for (idx = 0; (res = show_one_sig_status(ctx, idx, s)) != -1; idx++)
      {
        if (res == 1)
          anybad = 1;
        else if (res == 2)
          anywarn = 2;
      }
      if (!anybad)
        badsig = 0;
    }
  }

  if (!badsig)
  {
    gpgme_verify_result_t result;
    gpgme_sig_notation_t notation;
    gpgme_signature_t sig;
    int non_pka_notations;

    result = gpgme_op_verify_result(ctx);
    if (result)
    {
      for (sig = result->signatures; sig; sig = sig->next)
      {
        non_pka_notations = 0;
        for (notation = sig->notations; notation; notation = notation->next)
          if (!is_pka_notation(notation))
            non_pka_notations++;

        if (non_pka_notations)
        {
          char buf[SHORT_STRING];
          snprintf(buf, sizeof(buf),
                   _("*** Begin Notation (signature by: %s) ***\n"), sig->fpr);
          state_puts(buf, s);
          for (notation = sig->notations; notation; notation = notation->next)
          {
            if (is_pka_notation(notation))
              continue;

            if (notation->name)
            {
              state_puts(notation->name, s);
              state_puts("=", s);
            }
            if (notation->value)
            {
              state_puts(notation->value, s);
              if (!(*notation->value && (notation->value[strlen(notation->value) - 1] == '\n')))
                state_puts("\n", s);
            }
          }
          state_puts(_("*** End Notation ***\n"), s);
        }
      }
    }
  }

  gpgme_release(ctx);

  state_attach_puts(_("[-- End signature information --]\n\n"), s);
  mutt_debug(1, "verify_one: returning %d.\n", badsig);

  return badsig ? 1 : anywarn ? 2 : 0;
}

int pgp_gpgme_verify_one(struct Body *sigbdy, struct State *s, const char *tempfile)
{
  return verify_one(sigbdy, s, tempfile, 0);
}

int smime_gpgme_verify_one(struct Body *sigbdy, struct State *s, const char *tempfile)
{
  return verify_one(sigbdy, s, tempfile, 1);
}

/*
 * Implementation of `decrypt_part'.
 */

/**
 * decrypt_part - Decrypt a PGP or SMIME message
 *
 * (depending on the boolean flag IS_SMIME) with body A described further by
 * state S.  Write plaintext out to file FPOUT and return a new body.  For PGP
 * returns a flag in R_IS_SIGNED to indicate whether this is a combined
 * encrypted and signed message, for S/MIME it returns true when it is not a
 * encrypted but a signed message.
 */
static struct Body *decrypt_part(struct Body *a, struct State *s, FILE *fpout,
                                 int is_smime, int *r_is_signed)
{
  if (!a || !s || !fpout)
    return NULL;

  struct stat info;
  struct Body *tattach = NULL;
  int err = 0;
  gpgme_ctx_t ctx;
  gpgme_data_t ciphertext, plaintext;
  bool maybe_signed = false;
  bool anywarn = false;
  int sig_stat = 0;

  if (r_is_signed)
    *r_is_signed = 0;

  ctx = create_gpgme_context(is_smime);

restart:
  /* Make a data object from the body, create context etc. */
  ciphertext = file_to_data_object(s->fpin, a->offset, a->length);
  if (!ciphertext)
    return NULL;
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
    err = gpgme_op_decrypt(ctx, ciphertext, plaintext);
  gpgme_data_release(ciphertext);
  if (err)
  {
    if (is_smime && !maybe_signed && gpg_err_code(err) == GPG_ERR_NO_DATA)
    {
      /* Check whether this might be a signed message despite what
             the mime header told us.  Retry then.  gpgsm returns the
             error information "unsupported Algorithm '?'" but gpgme
             will not store this unknown algorithm, thus we test that
             it has not been set. */
      gpgme_decrypt_result_t result;

      result = gpgme_op_decrypt_result(ctx);
      if (!result->unsupported_algorithm)
      {
        maybe_signed = true;
        gpgme_data_release(plaintext);
        /* gpgsm ends the session after an error; restart it */
        gpgme_release(ctx);
        ctx = create_gpgme_context(is_smime);
        goto restart;
      }
    }
    redraw_if_needed(ctx);
    if ((s->flags & MUTT_DISPLAY))
    {
      char buf[200];

      snprintf(buf, sizeof(buf) - 1,
               _("[-- Error: decryption failed: %s --]\n\n"), gpgme_strerror(err));
      state_attach_puts(buf, s);
    }
    gpgme_data_release(plaintext);
    gpgme_release(ctx);
    return NULL;
  }
  redraw_if_needed(ctx);

  /* Read the output from GPGME, and make sure to change CRLF to LF,
     otherwise read_mime_header has a hard time parsing the message.  */
  if (data_object_to_stream(plaintext, fpout))
  {
    gpgme_data_release(plaintext);
    gpgme_release(ctx);
    return NULL;
  }
  gpgme_data_release(plaintext);

  a->is_signed_data = false;
  if (sig_stat)
  {
    int res, idx;
    int anybad = 0;

    if (maybe_signed)
      a->is_signed_data = true;
    if (r_is_signed)
      *r_is_signed = -1; /* A signature exists. */

    if ((s->flags & MUTT_DISPLAY))
      state_attach_puts(_("[-- Begin signature "
                          "information --]\n"),
                        s);
    for (idx = 0; (res = show_one_sig_status(ctx, idx, s)) != -1; idx++)
    {
      if (res == 1)
        anybad = 1;
      else if (res == 2)
        anywarn = true;
    }
    if (!anybad && idx && r_is_signed && *r_is_signed)
      *r_is_signed = anywarn ? 2 : 1; /* Good signature. */

    if ((s->flags & MUTT_DISPLAY))
      state_attach_puts(_("[-- End signature "
                          "information --]\n\n"),
                        s);
  }
  gpgme_release(ctx);
  ctx = NULL;

  fflush(fpout);
  rewind(fpout);
  tattach = mutt_read_mime_header(fpout, 0);
  if (tattach)
  {
    /*
       * Need to set the length of this body part.
       */
    fstat(fileno(fpout), &info);
    tattach->length = info.st_size - tattach->offset;

    tattach->warnsig = anywarn;

    /* See if we need to recurse on this MIME part.  */
    mutt_parse_part(fpout, tattach);
  }

  return tattach;
}

/**
 * pgp_gpgme_decrypt_mime - Decrypt a PGP/MIME message
 * @retval 0 on success
 *
 * The message in FPIN and B and return a new body and the stream in CUR and
 * FPOUT.
 */
int pgp_gpgme_decrypt_mime(FILE *fpin, FILE **fpout, struct Body *b, struct Body **cur)
{
  char tempfile[_POSIX_PATH_MAX];
  struct State s;
  struct Body *first_part = b;
  int is_signed = 0;
  bool need_decode = false;
  int saved_type;
  LOFF_T saved_offset;
  size_t saved_length;
  FILE *decoded_fp = NULL;
  int rv = 0;

  first_part->goodsig = false;
  first_part->warnsig = false;

  if (mutt_is_valid_multipart_pgp_encrypted(b))
    b = b->parts->next;
  else if (mutt_is_malformed_multipart_pgp_encrypted(b))
  {
    b = b->parts->next->next;
    need_decode = true;
  }
  else
    return -1;

  memset(&s, 0, sizeof(s));
  s.fpin = fpin;

  if (need_decode)
  {
    saved_type = b->type;
    saved_offset = b->offset;
    saved_length = b->length;

    mutt_mktemp(tempfile, sizeof(tempfile));
    decoded_fp = safe_fopen(tempfile, "w+");
    if (!decoded_fp)
    {
      mutt_perror(tempfile);
      return -1;
    }
    unlink(tempfile);

    fseeko(s.fpin, b->offset, SEEK_SET);
    s.fpout = decoded_fp;

    mutt_decode_attachment(b, &s);

    fflush(decoded_fp);
    b->length = ftello(decoded_fp);
    b->offset = 0;
    rewind(decoded_fp);
    s.fpin = decoded_fp;
    s.fpout = 0;
  }

  mutt_mktemp(tempfile, sizeof(tempfile));
  *fpout = safe_fopen(tempfile, "w+");
  if (!*fpout)
  {
    mutt_perror(tempfile);
    rv = -1;
    goto bail;
  }
  unlink(tempfile);

  *cur = decrypt_part(b, &s, *fpout, 0, &is_signed);
  if (!*cur)
    rv = -1;
  rewind(*fpout);
  if (is_signed > 0)
    first_part->goodsig = true;

bail:
  if (need_decode)
  {
    b->type = saved_type;
    b->length = saved_length;
    b->offset = saved_offset;
    safe_fclose(&decoded_fp);
  }

  return rv;
}

/**
 * smime_gpgme_decrypt_mime - Decrypt a S/MIME message
 * @retval 0 on success
 *
 * The message in FPIN and B and return a new body and
 * the stream in CUR and FPOUT.
 */
int smime_gpgme_decrypt_mime(FILE *fpin, FILE **fpout, struct Body *b, struct Body **cur)
{
  char tempfile[_POSIX_PATH_MAX];
  struct State s;
  FILE *tmpfp = NULL;
  int is_signed;
  LOFF_T saved_b_offset;
  size_t saved_b_length;
  int saved_b_type;

  if (!mutt_is_application_smime(b))
    return -1;

  if (b->parts)
    return -1;

  /* Decode the body - we need to pass binary CMS to the
     backend.  The backend allows for Base64 encoded data but it does
     not allow for QP which I have seen in some messages.  So better
     do it here. */
  saved_b_type = b->type;
  saved_b_offset = b->offset;
  saved_b_length = b->length;
  memset(&s, 0, sizeof(s));
  s.fpin = fpin;
  fseeko(s.fpin, b->offset, SEEK_SET);
  mutt_mktemp(tempfile, sizeof(tempfile));
  tmpfp = safe_fopen(tempfile, "w+");
  if (!tmpfp)
  {
    mutt_perror(tempfile);
    return -1;
  }
  mutt_unlink(tempfile);

  s.fpout = tmpfp;
  mutt_decode_attachment(b, &s);
  fflush(tmpfp);
  b->length = ftello(s.fpout);
  b->offset = 0;
  rewind(tmpfp);

  memset(&s, 0, sizeof(s));
  s.fpin = tmpfp;
  s.fpout = 0;
  mutt_mktemp(tempfile, sizeof(tempfile));
  *fpout = safe_fopen(tempfile, "w+");
  if (!*fpout)
  {
    mutt_perror(tempfile);
    return -1;
  }
  mutt_unlink(tempfile);

  *cur = decrypt_part(b, &s, *fpout, 1, &is_signed);
  if (*cur)
    (*cur)->goodsig = is_signed > 0;
  b->type = saved_b_type;
  b->length = saved_b_length;
  b->offset = saved_b_offset;
  safe_fclose(&tmpfp);
  rewind(*fpout);
  if (*cur && !is_signed && !(*cur)->parts && mutt_is_application_smime(*cur))
  {
    /* Assume that this is a opaque signed s/mime message.  This is
         an ugly way of doing it but we have anyway a problem with
         arbitrary encoded S/MIME messages: Only the outer part may be
         encrypted.  The entire mime parsing should be revamped,
         probably by keeping the temporary files so that we don't
         need to decrypt them all the time.  Inner parts of an
         encrypted part can then point into this file and there won't
         ever be a need to decrypt again.  This needs a partial
         rewrite of the MIME engine. */
    struct Body *bb = *cur;
    struct Body *tmp_b = NULL;

    saved_b_type = bb->type;
    saved_b_offset = bb->offset;
    saved_b_length = bb->length;
    memset(&s, 0, sizeof(s));
    s.fpin = *fpout;
    fseeko(s.fpin, bb->offset, SEEK_SET);
    mutt_mktemp(tempfile, sizeof(tempfile));
    tmpfp = safe_fopen(tempfile, "w+");
    if (!tmpfp)
    {
      mutt_perror(tempfile);
      return -1;
    }
    mutt_unlink(tempfile);

    s.fpout = tmpfp;
    mutt_decode_attachment(bb, &s);
    fflush(tmpfp);
    bb->length = ftello(s.fpout);
    bb->offset = 0;
    rewind(tmpfp);
    safe_fclose(fpout);

    memset(&s, 0, sizeof(s));
    s.fpin = tmpfp;
    s.fpout = 0;
    mutt_mktemp(tempfile, sizeof(tempfile));
    *fpout = safe_fopen(tempfile, "w+");
    if (!*fpout)
    {
      mutt_perror(tempfile);
      return -1;
    }
    mutt_unlink(tempfile);

    tmp_b = decrypt_part(bb, &s, *fpout, 1, &is_signed);
    if (tmp_b)
      tmp_b->goodsig = is_signed > 0;
    bb->type = saved_b_type;
    bb->length = saved_b_length;
    bb->offset = saved_b_offset;
    safe_fclose(&tmpfp);
    rewind(*fpout);
    mutt_free_body(cur);
    *cur = tmp_b;
  }
  return *cur ? 0 : -1;
}

static int pgp_gpgme_extract_keys(gpgme_data_t keydata, FILE **fp, int dryrun)
{
  /* there's no side-effect free way to view key data in GPGME,
   * so we import the key into a temporary keyring */
  char tmpdir[_POSIX_PATH_MAX];
  char tmpfile[_POSIX_PATH_MAX];
  gpgme_ctx_t tmpctx;
  gpgme_error_t err;
  gpgme_engine_info_t engineinfo;
  gpgme_key_t key;
  gpgme_user_id_t uid;
  gpgme_subkey_t subkey;
  const char *shortid = NULL;
  int len;
  char date[STRING];
  int more;
  int rc = -1;
  time_t tt;

  err = gpgme_new(&tmpctx);
  if (err != GPG_ERR_NO_ERROR)
  {
    mutt_debug(1, "Error creating GPGME context\n");
    return rc;
  }

  if (dryrun)
  {
    snprintf(tmpdir, sizeof(tmpdir), "%s/neomutt-gpgme-XXXXXX", Tmpdir);
    if (!mkdtemp(tmpdir))
    {
      mutt_debug(1, "Error creating temporary GPGME home\n");
      goto err_ctx;
    }

    engineinfo = gpgme_ctx_get_engine_info(tmpctx);
    while (engineinfo && engineinfo->protocol != GPGME_PROTOCOL_OpenPGP)
      engineinfo = engineinfo->next;
    if (!engineinfo)
    {
      mutt_debug(1, "Error finding GPGME PGP engine\n");
      goto err_tmpdir;
    }

    err = gpgme_ctx_set_engine_info(tmpctx, GPGME_PROTOCOL_OpenPGP,
                                    engineinfo->file_name, tmpdir);
    if (err != GPG_ERR_NO_ERROR)
    {
      mutt_debug(1, "Error setting GPGME context home\n");
      goto err_tmpdir;
    }
  }

  err = gpgme_op_import(tmpctx, keydata);
  if (err != GPG_ERR_NO_ERROR)
  {
    mutt_debug(1, "Error importing key\n");
    goto err_tmpdir;
  }

  mutt_mktemp(tmpfile, sizeof(tmpfile));
  *fp = safe_fopen(tmpfile, "w+");
  if (!*fp)
  {
    mutt_perror(tmpfile);
    goto err_tmpdir;
  }
  unlink(tmpfile);

  err = gpgme_op_keylist_start(tmpctx, NULL, 0);
  while (!err)
  {
    if ((err = gpgme_op_keylist_next(tmpctx, &key)))
      break;
    uid = key->uids;
    subkey = key->subkeys;
    more = 0;
    while (subkey)
    {
      shortid = subkey->keyid;
      len = mutt_strlen(subkey->keyid);
      if (len > 8)
        shortid += len - 8;
      tt = subkey->timestamp;
      strftime(date, sizeof(date), "%Y-%m-%d", localtime(&tt));

      if (!more)
        fprintf(*fp, "pub %5.5s %d/%8s %s %s\n", gpgme_pubkey_algo_name(subkey->pubkey_algo),
                subkey->length, shortid, date, uid->uid);
      else
        fprintf(*fp, "sub %5.5s %d/%8s %s\n", gpgme_pubkey_algo_name(subkey->pubkey_algo),
                subkey->length, shortid, date);
      subkey = subkey->next;
      more = 1;
    }
    gpgme_key_unref(key);
  }
  if (gpg_err_code(err) != GPG_ERR_EOF)
  {
    mutt_debug(1, "Error listing keys\n");
    goto err_fp;
  }

  rc = 0;

err_fp:
  if (rc)
    safe_fclose(fp);
err_tmpdir:
  if (dryrun)
    mutt_rmtree(tmpdir);
err_ctx:
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
  if (mutt_strncmp(a, b, n) == 0)
  {
    /* at this point we know that 'b' is at least 'n' chars long */
    if (b[n] == '\n' || (b[n] == '\r' && b[n + 1] == '\n'))
      return true;
  }
  return false;
}

#define _LINE_COMPARE(_x, _y) line_compare(_x, sizeof(_x) - 1, _y)
#define MESSAGE(_y) _LINE_COMPARE("MESSAGE-----", _y)
#define SIGNED_MESSAGE(_y) _LINE_COMPARE("SIGNED MESSAGE-----", _y)
#define PUBLIC_KEY_BLOCK(_y) _LINE_COMPARE("PUBLIC KEY BLOCK-----", _y)
#define BEGIN_PGP_SIGNATURE(_y)                                                \
  _LINE_COMPARE("-----BEGIN PGP SIGNATURE-----", _y)

/*
 * Implementation of `pgp_check_traditional'.
 */
static int pgp_check_traditional_one_body(FILE *fp, struct Body *b)
{
  char tempfile[_POSIX_PATH_MAX];
  char buf[HUGE_STRING];
  FILE *tfp = NULL;

  short sgn = 0;
  short enc = 0;

  if (b->type != TYPETEXT)
    return 0;

  mutt_mktemp(tempfile, sizeof(tempfile));
  if (mutt_decode_save_attachment(fp, b, tempfile, 0, 0) != 0)
  {
    unlink(tempfile);
    return 0;
  }

  tfp = fopen(tempfile, "r");
  if (!tfp)
  {
    unlink(tempfile);
    return 0;
  }

  while (fgets(buf, sizeof(buf), tfp))
  {
    if (mutt_strncmp("-----BEGIN PGP ", buf, 15) == 0)
    {
      if (MESSAGE(buf + 15))
      {
        enc = 1;
        break;
      }
      else if (SIGNED_MESSAGE(buf + 15))
      {
        sgn = 1;
        break;
      }
    }
  }
  safe_fclose(&tfp);
  unlink(tempfile);

  if (!enc && !sgn)
    return 0;

  /* fix the content type */

  mutt_set_parameter("format", "fixed", &b->parameter);
  mutt_set_parameter("x-action", enc ? "pgp-encrypted" : "pgp-signed", &b->parameter);

  return 1;
}

int pgp_gpgme_check_traditional(FILE *fp, struct Body *b, int just_one)
{
  int rv = 0;
  int r;
  for (; b; b = b->next)
  {
    if (!just_one && is_multipart(b))
      rv = (pgp_gpgme_check_traditional(fp, b->parts, 0) || rv);
    else if (b->type == TYPETEXT)
    {
      if ((r = mutt_is_application_pgp(b)))
        rv = (rv || r);
      else
        rv = (pgp_check_traditional_one_body(fp, b) || rv);
    }

    if (just_one)
      break;
  }
  return rv;
}

void pgp_gpgme_invoke_import(const char *fname)
{
  gpgme_data_t keydata;
  gpgme_error_t err;
  FILE *in = NULL;
  FILE *out = NULL;

  in = safe_fopen(fname, "r");
  if (!in)
    return;
  /* Note that the stream, "in", needs to be kept open while the keydata
   * is used.
   */
  err = gpgme_data_new_from_stream(&keydata, in);
  if (err != GPG_ERR_NO_ERROR)
  {
    safe_fclose(&in);
    mutt_error(_("error allocating data object: %s\n"), gpgme_strerror(err));
    mutt_sleep(1);
    return;
  }

  if (pgp_gpgme_extract_keys(keydata, &out, 0))
  {
    mutt_error(_("Error extracting key data!\n"));
    mutt_sleep(1);
  }
  gpgme_data_release(keydata);
  safe_fclose(&in);
  safe_fclose(&out);
}

/*
 * Implementation of `application_handler'.
 */

/**
 * copy_clearsigned - Copy a clearsigned message
 *
 * strip the signature and PGP's dash-escaping.
 *
 * XXX - charset handling: We assume that it is safe to do character set
 * decoding first, dash decoding second here, while we do it the other way
 * around in the main handler.
 *
 * (Note that we aren't worse than Outlook & Cie in this, and also note that we
 * can successfully handle anything produced by any existing versions of neomutt.)
 */
static void copy_clearsigned(gpgme_data_t data, struct State *s, char *charset)
{
  char buf[HUGE_STRING];
  bool complete, armor_header;
  FGETCONV *fc = NULL;
  char *fname = NULL;
  FILE *fp = NULL;

  fname = data_object_to_tempfile(data, NULL, &fp);
  if (!fname)
  {
    safe_fclose(&fp);
    return;
  }
  unlink(fname);
  FREE(&fname);

  /* fromcode comes from the MIME Content-Type charset label. It might
   * be a wrong label, so we want the ability to do corrections via
   * charset-hooks. Therefore we set flags to MUTT_ICONV_HOOK_FROM.
   */
  fc = fgetconv_open(fp, charset, Charset, MUTT_ICONV_HOOK_FROM);

  for (complete = true, armor_header = true; fgetconvs(buf, sizeof(buf), fc) != NULL;
       complete = (strchr(buf, '\n') != NULL))
  {
    if (!complete)
    {
      if (!armor_header)
        state_puts(buf, s);
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

    if (s->prefix)
      state_puts(s->prefix, s);

    if (buf[0] == '-' && buf[1] == ' ')
      state_puts(buf + 2, s);
    else
      state_puts(buf, s);
  }

  fgetconv_close(&fc);
  safe_fclose(&fp);
}

/**
 * pgp_gpgme_application_handler - Support for classic_application/pgp
 */
int pgp_gpgme_application_handler(struct Body *m, struct State *s)
{
  int needpass = -1;
  bool pgp_keyblock = false;
  bool clearsign = false;
  long bytes;
  LOFF_T last_pos, offset;
  char buf[HUGE_STRING];
  FILE *pgpout = NULL;

  gpgme_error_t err = 0;
  gpgme_data_t armored_data = NULL;

  bool maybe_goodsig = true;
  bool have_any_sigs = false;

  char body_charset[STRING]; /* Only used for clearsigned messages. */

  mutt_debug(2, "Entering pgp_application_pgp handler\n");

  /* For clearsigned messages we won't be able to get a character set
     but we know that this may only be text thus we assume Latin-1
     here. */
  if (!mutt_get_body_charset(body_charset, sizeof(body_charset), m))
    strfcpy(body_charset, "iso-8859-1", sizeof(body_charset));

  fseeko(s->fpin, m->offset, SEEK_SET);
  last_pos = m->offset;

  for (bytes = m->length; bytes > 0;)
  {
    if (fgets(buf, sizeof(buf), s->fpin) == NULL)
      break;

    offset = ftello(s->fpin);
    bytes -= (offset - last_pos); /* don't rely on mutt_strlen(buf) */
    last_pos = offset;

    if (mutt_strncmp("-----BEGIN PGP ", buf, 15) == 0)
    {
      clearsign = false;

      if (MESSAGE(buf + 15))
        needpass = 1;
      else if (SIGNED_MESSAGE(buf + 15))
      {
        clearsign = true;
        needpass = 0;
      }
      else if (PUBLIC_KEY_BLOCK(buf + 15))
      {
        needpass = 0;
        pgp_keyblock = true;
      }
      else
      {
        /* XXX - we may wish to recode here */
        if (s->prefix)
          state_puts(s->prefix, s);
        state_puts(buf, s);
        continue;
      }

      have_any_sigs = (have_any_sigs || (clearsign && (s->flags & MUTT_VERIFY)));

      /* Copy PGP material to an data container */
      armored_data = file_to_data_object(s->fpin, m->offset, m->length);
      /* Invoke PGP if needed */
      if (pgp_keyblock)
      {
        pgp_gpgme_extract_keys(armored_data, &pgpout, 1);
      }
      else if (!clearsign || (s->flags & MUTT_VERIFY))
      {
        bool sig_stat = false;
        gpgme_data_t plaintext;
        gpgme_ctx_t ctx;

        plaintext = create_gpgme_data();
        ctx = create_gpgme_context(0);

        if (clearsign)
          err = gpgme_op_verify(ctx, armored_data, NULL, plaintext);
        else
        {
          err = gpgme_op_decrypt_verify(ctx, armored_data, plaintext);
          if (gpg_err_code(err) == GPG_ERR_NO_DATA)
          {
            /* Decrypt verify can't handle signed only messages. */
            err = (gpgme_data_seek(armored_data, 0, SEEK_SET) == -1) ?
                      gpgme_error_from_errno(errno) :
                      0;
            /* Must release plaintext so that we supply an
                         uninitialized object. */
            gpgme_data_release(plaintext);
            plaintext = create_gpgme_data();
            err = gpgme_op_verify(ctx, armored_data, NULL, plaintext);
          }
        }
        redraw_if_needed(ctx);

        if (err)
        {
          char errbuf[200];

          snprintf(errbuf, sizeof(errbuf) - 1,
                   _("Error: decryption/verification failed: %s\n"), gpgme_strerror(err));
          state_puts(errbuf, s);
        }
        else
        { /* Decryption/Verification succeeded */
          char *tmpfname = NULL;

          {
            /* Check whether signatures have been verified.  */
            gpgme_verify_result_t verify_result;

            verify_result = gpgme_op_verify_result(ctx);
            if (verify_result->signatures)
              sig_stat = true;
          }

          have_any_sigs = false;
          maybe_goodsig = false;
          if ((s->flags & MUTT_DISPLAY) && sig_stat)
          {
            int res, idx;
            bool anybad = false;

            state_attach_puts(_("[-- Begin signature "
                                "information --]\n"),
                              s);
            have_any_sigs = true;
            for (idx = 0; (res = show_one_sig_status(ctx, idx, s)) != -1; idx++)
            {
              if (res == 1)
                anybad = true;
            }
            if (!anybad && idx)
              maybe_goodsig = true;

            state_attach_puts(_("[-- End signature "
                                "information --]\n\n"),
                              s);
          }

          tmpfname = data_object_to_tempfile(plaintext, NULL, &pgpout);
          if (!tmpfname)
          {
            safe_fclose(&pgpout);
            state_puts(_("Error: copy data failed\n"), s);
          }
          else
          {
            unlink(tmpfname);
            FREE(&tmpfname);
          }
        }
        gpgme_data_release(plaintext);
        gpgme_release(ctx);
      }

      /*
           * Now, copy cleartext to the screen.  NOTE - we expect that PGP
           * outputs utf-8 cleartext.  This may not always be true, but it
           * seems to be a reasonable guess.
           */

      if (s->flags & MUTT_DISPLAY)
      {
        if (needpass)
          state_attach_puts(_("[-- BEGIN PGP MESSAGE --]\n\n"), s);
        else if (pgp_keyblock)
          state_attach_puts(_("[-- BEGIN PGP PUBLIC KEY BLOCK --]\n"), s);
        else
          state_attach_puts(_("[-- BEGIN PGP SIGNED MESSAGE --]\n\n"), s);
      }

      if (clearsign)
      {
        copy_clearsigned(armored_data, s, body_charset);
      }
      else if (pgpout)
      {
        FGETCONV *fc = NULL;
        int c;
        rewind(pgpout);
        fc = fgetconv_open(pgpout, "utf-8", Charset, 0);
        while ((c = fgetconv(fc)) != EOF)
        {
          state_putc(c, s);
          if (c == '\n' && s->prefix)
            state_puts(s->prefix, s);
        }
        fgetconv_close(&fc);
      }

      if (s->flags & MUTT_DISPLAY)
      {
        state_putc('\n', s);
        if (needpass)
          state_attach_puts(_("[-- END PGP MESSAGE --]\n"), s);
        else if (pgp_keyblock)
          state_attach_puts(_("[-- END PGP PUBLIC KEY BLOCK --]\n"), s);
        else
          state_attach_puts(_("[-- END PGP SIGNED MESSAGE --]\n"), s);
      }

      gpgme_data_release(armored_data);
      if (pgpout)
      {
        safe_fclose(&pgpout);
      }
    }
    else
    {
      /* A traditional PGP part may mix signed and unsigned content */
      /* XXX - we may wish to recode here */
      if (s->prefix)
        state_puts(s->prefix, s);
      state_puts(buf, s);
    }
  }

  m->goodsig = (maybe_goodsig && have_any_sigs);

  if (needpass == -1)
  {
    state_attach_puts(_("[-- Error: could not find beginning"
                        " of PGP message! --]\n\n"),
                      s);
    return 1;
  }
  mutt_debug(2, "Leaving pgp_application_pgp handler\n");

  return err;
}

/*
 * Implementation of `encrypted_handler'.
 */

/**
 * pgp_gpgme_encrypted_handler - MIME handler for pgp/mime encrypted messages
 *
 * This handler is passed the application/octet-stream directly.
 * The caller must propagate a->goodsig to its parent.
 */
int pgp_gpgme_encrypted_handler(struct Body *a, struct State *s)
{
  char tempfile[_POSIX_PATH_MAX];
  FILE *fpout = NULL;
  struct Body *tattach = NULL;
  int is_signed;
  int rc = 0;

  mutt_debug(2, "Entering pgp_encrypted handler\n");

  mutt_mktemp(tempfile, sizeof(tempfile));
  fpout = safe_fopen(tempfile, "w+");
  if (!fpout)
  {
    if (s->flags & MUTT_DISPLAY)
      state_attach_puts(_("[-- Error: could not create temporary file! "
                          "--]\n"),
                        s);
    return -1;
  }

  tattach = decrypt_part(a, s, fpout, 0, &is_signed);
  if (tattach)
  {
    tattach->goodsig = is_signed > 0;

    if (s->flags & MUTT_DISPLAY)
      state_attach_puts(
          is_signed ? _("[-- The following data is PGP/MIME signed and "
                        "encrypted --]\n\n") :
                      _("[-- The following data is PGP/MIME encrypted --]\n\n"),
          s);

    {
      FILE *savefp = s->fpin;
      s->fpin = fpout;
      rc = mutt_body_handler(tattach, s);
      s->fpin = savefp;
    }

    /*
       * if a multipart/signed is the _only_ sub-part of a
       * multipart/encrypted, cache signature verification
       * status.
       */
    if (mutt_is_multipart_signed(tattach) && !tattach->next)
      a->goodsig |= tattach->goodsig;

    if (s->flags & MUTT_DISPLAY)
    {
      state_puts("\n", s);
      state_attach_puts(
          is_signed ? _("[-- End of PGP/MIME signed and encrypted data --]\n") :
                      _("[-- End of PGP/MIME encrypted data --]\n"),
          s);
    }

    mutt_free_body(&tattach);
    mutt_message(_("PGP message successfully decrypted."));
  }
  else
  {
    mutt_error(_("Could not decrypt PGP message"));
    mutt_sleep(2);
    rc = -1;
  }

  safe_fclose(&fpout);
  mutt_unlink(tempfile);
  mutt_debug(2, "Leaving pgp_encrypted handler\n");

  return rc;
}

/**
 * smime_gpgme_application_handler - Support for application/smime
 */
int smime_gpgme_application_handler(struct Body *a, struct State *s)
{
  char tempfile[_POSIX_PATH_MAX];
  FILE *fpout = NULL;
  struct Body *tattach = NULL;
  int is_signed;
  int rc = 0;

  mutt_debug(2, "Entering smime_encrypted handler\n");

  a->warnsig = false;
  mutt_mktemp(tempfile, sizeof(tempfile));
  fpout = safe_fopen(tempfile, "w+");
  if (!fpout)
  {
    if (s->flags & MUTT_DISPLAY)
      state_attach_puts(_("[-- Error: could not create temporary file! "
                          "--]\n"),
                        s);
    return -1;
  }

  tattach = decrypt_part(a, s, fpout, 1, &is_signed);
  if (tattach)
  {
    tattach->goodsig = is_signed > 0;

    if (s->flags & MUTT_DISPLAY)
      state_attach_puts(
          is_signed ? _("[-- The following data is S/MIME signed --]\n\n") :
                      _("[-- The following data is S/MIME encrypted --]\n\n"),
          s);

    {
      FILE *savefp = s->fpin;
      s->fpin = fpout;
      rc = mutt_body_handler(tattach, s);
      s->fpin = savefp;
    }

    /*
       * if a multipart/signed is the _only_ sub-part of a
       * multipart/encrypted, cache signature verification
       * status.
       */
    if (mutt_is_multipart_signed(tattach) && !tattach->next)
    {
      a->goodsig = tattach->goodsig;
      if (!a->goodsig)
        a->warnsig = tattach->warnsig;
    }
    else if (tattach->goodsig)
    {
      a->goodsig = true;
      a->warnsig = tattach->warnsig;
    }

    if (s->flags & MUTT_DISPLAY)
    {
      state_puts("\n", s);
      state_attach_puts(is_signed ? _("[-- End of S/MIME signed data --]\n") :
                                    _("[-- End of S/MIME encrypted data --]\n"),
                        s);
    }

    mutt_free_body(&tattach);
  }

  safe_fclose(&fpout);
  mutt_unlink(tempfile);
  mutt_debug(2, "Leaving smime_encrypted handler\n");

  return rc;
}

/**
 * crypt_entry_fmt - Format an entry on the CRYPT key selection menu
 *
 * * \%u user id
 * * \%n number
 * * \%t trust/validity of the key-uid association
 * * \%p         protocol
 * * \%[...] date of key using strftime(3)
 *
 * * \%k key id
 * * \%a algorithm
 * * \%l length
 * * \%f flags
 * * \%c capabilities
 *
 * * \%K key id of the principal key
 * * \%A algorithm of the principal key
 * * \%L length of the principal key
 * * \%F flags of the principal key
 * * \%C capabilities of the principal key
 */
static const char *crypt_entry_fmt(char *dest, size_t destlen, size_t col, int cols,
                                   char op, const char *src, const char *prefix,
                                   const char *ifstring, const char *elsestring,
                                   unsigned long data, enum FormatFlag flags)
{
  char fmt[16];
  struct CryptEntry *entry = NULL;
  struct CryptKeyInfo *key = NULL;
  int kflags = 0;
  int optional = (flags & MUTT_FORMAT_OPTIONAL);
  const char *s = NULL;
  unsigned long val;

  entry = (struct CryptEntry *) data;
  key = entry->key;

  /*    if (isupper ((unsigned char) op)) */
  /*      key = pkey; */

  kflags = (key->flags /* | (pkey->flags & KEYFLAG_RESTRICTIONS)
                          | uid->flags */);

  switch (tolower(op))
  {
    case '[':
    {
      const char *cp = NULL;
      char buf2[SHORT_STRING], *p = NULL;
      int do_locales;
      struct tm *tm = NULL;
      size_t len;

      p = dest;

      cp = src;
      if (*cp == '!')
      {
        do_locales = 0;
        cp++;
      }
      else
        do_locales = 1;

      len = destlen - 1;
      while (len > 0 && *cp != ']')
      {
        if (*cp == '%')
        {
          cp++;
          if (len >= 2)
          {
            *p++ = '%';
            *p++ = *cp;
            len -= 2;
          }
          else
            break; /* not enough space */
          cp++;
        }
        else
        {
          *p++ = *cp++;
          len--;
        }
      }
      *p = 0;

      {
        time_t tt = 0;

        if (key->kobj->subkeys && (key->kobj->subkeys->timestamp > 0))
          tt = key->kobj->subkeys->timestamp;

        tm = localtime(&tt);
      }

      if (!do_locales)
        setlocale(LC_TIME, "C");
      strftime(buf2, sizeof(buf2), dest, tm);
      if (!do_locales)
        setlocale(LC_TIME, "");

      snprintf(fmt, sizeof(fmt), "%%%ss", prefix);
      snprintf(dest, destlen, fmt, buf2);
      if (len > 0)
        src = cp + 1;
    }
    break;
    case 'n':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prefix);
        snprintf(dest, destlen, fmt, entry->num);
      }
      break;
    case 'k':
      if (!optional)
      {
        /* fixme: we need a way to distinguish between main and subkeys.
           Store the idx in entry? */
        snprintf(fmt, sizeof(fmt), "%%%ss", prefix);
        snprintf(dest, destlen, fmt, crypt_keyid(key));
      }
      break;
    case 'u':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prefix);
        snprintf(dest, destlen, fmt, key->uid);
      }
      break;
    case 'a':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%s.3s", prefix);
        if (key->kobj->subkeys)
          s = gpgme_pubkey_algo_name(key->kobj->subkeys->pubkey_algo);
        else
          s = "?";
        snprintf(dest, destlen, fmt, s);
      }
      break;
    case 'l':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%slu", prefix);
        if (key->kobj->subkeys)
          val = key->kobj->subkeys->length;
        else
          val = 0;
        snprintf(dest, destlen, fmt, val);
      }
      break;
    case 'f':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sc", prefix);
        snprintf(dest, destlen, fmt, crypt_flags(kflags));
      }
      else if (!(kflags & (KEYFLAG_RESTRICTIONS)))
        optional = 0;
      break;
    case 'c':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prefix);
        snprintf(dest, destlen, fmt, crypt_key_abilities(kflags));
      }
      else if (!(kflags & (KEYFLAG_ABILITIES)))
        optional = 0;
      break;
    case 't':
      if ((kflags & KEYFLAG_ISX509))
        s = "x";
      else
      {
        switch (key->validity)
        {
          case GPGME_VALIDITY_UNDEFINED:
            s = "q";
            break;
          case GPGME_VALIDITY_NEVER:
            s = "n";
            break;
          case GPGME_VALIDITY_MARGINAL:
            s = "m";
            break;
          case GPGME_VALIDITY_FULL:
            s = "f";
            break;
          case GPGME_VALIDITY_ULTIMATE:
            s = "u";
            break;
          case GPGME_VALIDITY_UNKNOWN:
          default:
            s = "?";
            break;
        }
      }
      snprintf(fmt, sizeof(fmt), "%%%sc", prefix);
      snprintf(dest, destlen, fmt, *s);
      break;
    case 'p':
      snprintf(fmt, sizeof(fmt), "%%%ss", prefix);
      snprintf(dest, destlen, fmt, gpgme_get_protocol_name(key->kobj->protocol));
      break;

    default:
      *dest = '\0';
  }

  if (optional)
    mutt_expando_format(dest, destlen, col, cols, ifstring, mutt_attach_fmt, data, 0);
  else if (flags & MUTT_FORMAT_OPTIONAL)
    mutt_expando_format(dest, destlen, col, cols, elsestring, mutt_attach_fmt, data, 0);
  return src;
}

/**
 * crypt_entry - Used by the display function to format a line
 */
static void crypt_entry(char *s, size_t l, struct Menu *menu, int num)
{
  struct CryptKeyInfo **key_table = (struct CryptKeyInfo **) menu->data;
  struct CryptEntry entry;

  entry.key = key_table[num];
  entry.num = num + 1;

  mutt_expando_format(s, l, 0, MuttIndexWindow->cols, NONULL(PgpEntryFormat),
                      crypt_entry_fmt, (unsigned long) &entry, MUTT_FORMAT_ARROWCURSOR);
}

/**
 * _crypt_compare_address - Compare Key addresses and IDs for sorting
 */
static int _crypt_compare_address(const void *a, const void *b)
{
  struct CryptKeyInfo **s = (struct CryptKeyInfo **) a;
  struct CryptKeyInfo **t = (struct CryptKeyInfo **) b;
  int r;

  if ((r = mutt_strcasecmp((*s)->uid, (*t)->uid)))
    return r > 0;
  else
    return (mutt_strcasecmp(crypt_fpr_or_lkeyid(*s), crypt_fpr_or_lkeyid(*t)) > 0);
}

static int crypt_compare_address(const void *a, const void *b)
{
  return ((PgpSortKeys & SORT_REVERSE) ? !_crypt_compare_address(a, b) :
                                         _crypt_compare_address(a, b));
}

/**
 * _crypt_compare_keyid - Compare Key IDs and addresses for sorting
 */
static int _crypt_compare_keyid(const void *a, const void *b)
{
  struct CryptKeyInfo **s = (struct CryptKeyInfo **) a;
  struct CryptKeyInfo **t = (struct CryptKeyInfo **) b;
  int r;

  if ((r = mutt_strcasecmp(crypt_fpr_or_lkeyid(*s), crypt_fpr_or_lkeyid(*t))))
    return r > 0;
  else
    return (mutt_strcasecmp((*s)->uid, (*t)->uid) > 0);
}

static int crypt_compare_keyid(const void *a, const void *b)
{
  return ((PgpSortKeys & SORT_REVERSE) ? !_crypt_compare_keyid(a, b) :
                                         _crypt_compare_keyid(a, b));
}

/**
 * _crypt_compare_date - Compare Key creation dates and addresses for sorting
 */
static int _crypt_compare_date(const void *a, const void *b)
{
  struct CryptKeyInfo **s = (struct CryptKeyInfo **) a;
  struct CryptKeyInfo **t = (struct CryptKeyInfo **) b;
  unsigned long ts = 0, tt = 0;

  if ((*s)->kobj->subkeys && ((*s)->kobj->subkeys->timestamp > 0))
    ts = (*s)->kobj->subkeys->timestamp;
  if ((*t)->kobj->subkeys && ((*t)->kobj->subkeys->timestamp > 0))
    tt = (*t)->kobj->subkeys->timestamp;

  if (ts > tt)
    return 1;
  if (ts < tt)
    return 0;

  return (mutt_strcasecmp((*s)->uid, (*t)->uid) > 0);
}

static int crypt_compare_date(const void *a, const void *b)
{
  return ((PgpSortKeys & SORT_REVERSE) ? !_crypt_compare_date(a, b) :
                                         _crypt_compare_date(a, b));
}

/**
 * _crypt_compare_trust - Compare the trust of keys for sorting
 *
 * Compare two trust values, the key length, the creation dates. the addresses
 * and the key IDs.
 */
static int _crypt_compare_trust(const void *a, const void *b)
{
  struct CryptKeyInfo **s = (struct CryptKeyInfo **) a;
  struct CryptKeyInfo **t = (struct CryptKeyInfo **) b;
  unsigned long ts = 0, tt = 0;
  int r;

  if ((r = (((*s)->flags & (KEYFLAG_RESTRICTIONS)) - ((*t)->flags & (KEYFLAG_RESTRICTIONS)))))
    return r > 0;

  ts = (*s)->validity;
  tt = (*t)->validity;
  if ((r = (tt - ts)))
    return r < 0;

  if ((*s)->kobj->subkeys)
    ts = (*s)->kobj->subkeys->length;
  if ((*t)->kobj->subkeys)
    tt = (*t)->kobj->subkeys->length;
  if (ts != tt)
    return ts > tt;

  if ((*s)->kobj->subkeys && ((*s)->kobj->subkeys->timestamp > 0))
    ts = (*s)->kobj->subkeys->timestamp;
  if ((*t)->kobj->subkeys && ((*t)->kobj->subkeys->timestamp > 0))
    tt = (*t)->kobj->subkeys->timestamp;
  if (ts > tt)
    return 1;
  if (ts < tt)
    return 0;

  if ((r = mutt_strcasecmp((*s)->uid, (*t)->uid)))
    return r > 0;
  return (mutt_strcasecmp(crypt_fpr_or_lkeyid((*s)), crypt_fpr_or_lkeyid((*t))) > 0);
}

static int crypt_compare_trust(const void *a, const void *b)
{
  return ((PgpSortKeys & SORT_REVERSE) ? !_crypt_compare_trust(a, b) :
                                         _crypt_compare_trust(a, b));
}

/**
 * print_dn_part - Print the X.500 Distinguished Name
 *
 * Print the X.500 Distinguished Name part KEY from the array of parts DN to FP.
 */
static int print_dn_part(FILE *fp, struct DnArray *dn, const char *key)
{
  int any = 0;

  for (; dn->key; dn++)
  {
    if (strcmp(dn->key, key) == 0)
    {
      if (any)
        fputs(" + ", fp);
      print_utf8(fp, dn->value, strlen(dn->value));
      any = 1;
    }
  }
  return any;
}

/**
 * print_dn_parts - Print all parts of a DN in a standard sequence
 */
static void print_dn_parts(FILE *fp, struct DnArray *dn)
{
  static const char *const stdpart[] = {
    "CN", "OU", "O", "STREET", "L", "ST", "C", NULL,
  };
  int any = 0, any2 = 0, i;

  for (i = 0; stdpart[i]; i++)
  {
    if (any)
      fputs(", ", fp);
    any = print_dn_part(fp, dn, stdpart[i]);
  }
  /* now print the rest without any specific ordering */
  for (; dn->key; dn++)
  {
    for (i = 0; stdpart[i]; i++)
    {
      if (strcmp(dn->key, stdpart[i]) == 0)
        break;
    }
    if (!stdpart[i])
    {
      if (any)
        fputs(", ", fp);
      if (!any2)
        fputs("(", fp);
      any = print_dn_part(fp, dn, dn->key);
      any2 = 1;
    }
  }
  if (any2)
    fputs(")", fp);
}

/**
 * parse_dn_part - Parse an RDN; this is a helper to parse_dn()
 */
static const char *parse_dn_part(struct DnArray *array, const char *string)
{
  const char *s = NULL, *s1 = NULL;
  size_t n;
  char *p = NULL;

  /* parse attributeType */
  for (s = string + 1; *s && *s != '='; s++)
    ;
  if (!*s)
    return NULL; /* error */
  n = s - string;
  if (!n)
    return NULL; /* empty key */
  array->key = safe_malloc(n + 1);
  p = array->key;
  memcpy(p, string, n); /* fixme: trim trailing spaces */
  p[n] = 0;
  string = s + 1;

  if (*string == '#')
  { /* hexstring */
    string++;
    for (s = string; isxdigit(*s); s++)
      s++;
    n = s - string;
    if (!n || (n & 1))
      return NULL; /* empty or odd number of digits */
    n /= 2;
    p = safe_malloc(n + 1);
    array->value = (char *) p;
    for (s1 = string; n; s1 += 2, n--)
      sscanf(s1, "%2hhx", (unsigned char *) p++);
    *p = 0;
  }
  else
  { /* regular v3 quoted string */
    for (n = 0, s = string; *s; s++)
    {
      if (*s == '\\')
      { /* pair */
        s++;
        if (*s == ',' || *s == '=' || *s == '+' || *s == '<' || *s == '>' ||
            *s == '#' || *s == ';' || *s == '\\' || *s == '\"' || *s == ' ')
          n++;
        else if (isxdigit(*s) && isxdigit(*(s + 1)))
        {
          s++;
          n++;
        }
        else
          return NULL; /* invalid escape sequence */
      }
      else if (*s == '\"')
        return NULL; /* invalid encoding */
      else if (*s == ',' || *s == '=' || *s == '+' || *s == '<' || *s == '>' ||
               *s == '#' || *s == ';')
        break;
      else
        n++;
    }

    p = safe_malloc(n + 1);
    array->value = (char *) p;
    for (s = string; n; s++, n--)
    {
      if (*s == '\\')
      {
        s++;
        if (isxdigit(*s))
        {
          sscanf(s, "%2hhx", (unsigned char *) p++);
          s++;
        }
        else
          *p++ = *s;
      }
      else
        *p++ = *s;
    }
    *p = 0;
  }
  return s;
}

/**
 * parse_dn - Parse a DN and return an array-ized one
 *
 * This is not a validating parser and it does not support any old-stylish
 * syntax; gpgme is expected to return only rfc2253 compatible strings.
 */
static struct DnArray *parse_dn(const char *string)
{
  struct DnArray *array = NULL;
  size_t arrayidx, arraysize;

  arraysize = 7; /* C,ST,L,O,OU,CN,email */
  array = safe_malloc((arraysize + 1) * sizeof(*array));
  arrayidx = 0;
  while (*string)
  {
    while (*string == ' ')
      string++;
    if (!*string)
      break; /* ready */
    if (arrayidx >= arraysize)
    {
      /* neomutt lacks a real safe_realloc - so we need to copy */
      struct DnArray *a2 = NULL;

      arraysize += 5;
      a2 = safe_malloc((arraysize + 1) * sizeof(*array));
      for (int i = 0; i < arrayidx; i++)
      {
        a2[i].key = array[i].key;
        a2[i].value = array[i].value;
      }
      FREE(&array);
      array = a2;
    }
    array[arrayidx].key = NULL;
    array[arrayidx].value = NULL;
    string = parse_dn_part(array + arrayidx, string);
    arrayidx++;
    if (!string)
      goto failure;
    while (*string == ' ')
      string++;
    if (*string && *string != ',' && *string != ';' && *string != '+')
      goto failure; /* invalid delimiter */
    if (*string)
      string++;
  }
  array[arrayidx].key = NULL;
  array[arrayidx].value = NULL;
  return array;

failure:
  for (int i = 0; i < arrayidx; i++)
  {
    FREE(&array[i].key);
    FREE(&array[i].value);
  }
  FREE(&array);
  return NULL;
}

/**
 * parse_and_print_user_id - Print a nice representation of the userid
 *
 * Make sure it is displayed in a proper way, which does mean to reorder some
 * parts for S/MIME's DNs.  USERID is a string as returned by the gpgme key
 * functions.  It is utf-8 encoded.
 */
static void parse_and_print_user_id(FILE *fp, const char *userid)
{
  const char *s = NULL;

  if (*userid == '<')
  {
    s = strchr(userid + 1, '>');
    if (s)
      print_utf8(fp, userid + 1, s - userid - 1);
  }
  else if (*userid == '(')
    fputs(_("[Can't display this user ID (unknown encoding)]"), fp);
  else if (!digit_or_letter((const unsigned char *) userid))
    fputs(_("[Can't display this user ID (invalid encoding)]"), fp);
  else
  {
    struct DnArray *dn = parse_dn(userid);
    if (!dn)
      fputs(_("[Can't display this user ID (invalid DN)]"), fp);
    else
    {
      print_dn_parts(fp, dn);
      for (int i = 0; dn[i].key; i++)
      {
        FREE(&dn[i].key);
        FREE(&dn[i].value);
      }
      FREE(&dn);
    }
  }
}

/**
 * enum KeyCap - PGP/SMIME Key Capabilities
 */
enum KeyCap
{
  KEY_CAP_CAN_ENCRYPT,
  KEY_CAP_CAN_SIGN,
  KEY_CAP_CAN_CERTIFY
};

static unsigned int key_check_cap(gpgme_key_t key, enum KeyCap cap)
{
  gpgme_subkey_t subkey = NULL;
  unsigned int ret = 0;

  switch (cap)
  {
    case KEY_CAP_CAN_ENCRYPT:
      ret = key->can_encrypt;
      if (ret == 0)
        for (subkey = key->subkeys; subkey; subkey = subkey->next)
          if ((ret = subkey->can_encrypt))
            break;
      break;
    case KEY_CAP_CAN_SIGN:
      ret = key->can_sign;
      if (ret == 0)
        for (subkey = key->subkeys; subkey; subkey = subkey->next)
          if ((ret = subkey->can_sign))
            break;
      break;
    case KEY_CAP_CAN_CERTIFY:
      ret = key->can_certify;
      if (ret == 0)
        for (subkey = key->subkeys; subkey; subkey = subkey->next)
          if ((ret = subkey->can_certify))
            break;
      break;
  }

  return ret;
}

/**
 * enum KeyInfo - PGP Key info
 */
enum KeyInfo
{
  KIP_NAME = 0,
  KIP_AKA,
  KIP_VALID_FROM,
  KIP_VALID_TO,
  KIP_KEY_TYPE,
  KIP_KEY_USAGE,
  KIP_FINGERPRINT,
  KIP_SERIAL_NO,
  KIP_ISSUED_BY,
  KIP_SUBKEY,
  KIP_END
};

static const char *const KeyInfoPrompts[] = {
  /* L10N:
   * The following are the headers for the "verify key" output from the
   * GPGME key selection menu (bound to "c" in the key selection menu).
   * They will be automatically aligned. */
  N_("Name: "),      N_("aka: "),       N_("Valid From: "),  N_("Valid To: "),
  N_("Key Type: "),  N_("Key Usage: "), N_("Fingerprint: "), N_("Serial-No: "),
  N_("Issued By: "), N_("Subkey: ")
};

int KeyInfoPadding[KIP_END] = { 0 };

/**
 * print_key_info - Verbose information about a key or certificate to a file
 */
static void print_key_info(gpgme_key_t key, FILE *fp)
{
  int idx;
  const char *s = NULL, *s2 = NULL;
  time_t tt = 0;
  struct tm *tm = NULL;
  char shortbuf[SHORT_STRING];
  unsigned long aval = 0;
  const char *delim = NULL;
  int is_pgp = 0;
  gpgme_user_id_t uid = NULL;
  static int max_header_width = 0;
  int width;

  if (!max_header_width)
  {
    for (int i = 0; i < KIP_END; i++)
    {
      KeyInfoPadding[i] = mutt_strlen(_(KeyInfoPrompts[i]));
      width = mutt_strwidth(_(KeyInfoPrompts[i]));
      if (max_header_width < width)
        max_header_width = width;
      KeyInfoPadding[i] -= width;
    }
    for (int i = 0; i < KIP_END; i++)
      KeyInfoPadding[i] += max_header_width;
  }

  is_pgp = key->protocol == GPGME_PROTOCOL_OpenPGP;

  for (idx = 0, uid = key->uids; uid; idx++, uid = uid->next)
  {
    if (uid->revoked)
      continue;

    s = uid->uid;
    /* L10N: DOTFILL */

    if (!idx)
      fprintf(fp, "%*s", KeyInfoPadding[KIP_NAME], _(KeyInfoPrompts[KIP_NAME]));
    else
      fprintf(fp, "%*s", KeyInfoPadding[KIP_AKA], _(KeyInfoPrompts[KIP_AKA]));
    if (uid->invalid)
    {
      /* L10N: comes after the Name or aka if the key is invalid */
      fputs(_("[Invalid]"), fp);
      putc(' ', fp);
    }
    if (is_pgp)
      print_utf8(fp, s, strlen(s));
    else
      parse_and_print_user_id(fp, s);
    putc('\n', fp);
  }

  if (key->subkeys && (key->subkeys->timestamp > 0))
  {
    tt = key->subkeys->timestamp;

    tm = localtime(&tt);
    strftime(shortbuf, sizeof(shortbuf), nl_langinfo(D_T_FMT), tm);
    fprintf(fp, "%*s%s\n", KeyInfoPadding[KIP_VALID_FROM],
            _(KeyInfoPrompts[KIP_VALID_FROM]), shortbuf);
  }

  if (key->subkeys && (key->subkeys->expires > 0))
  {
    tt = key->subkeys->expires;

    tm = localtime(&tt);
    strftime(shortbuf, sizeof(shortbuf), nl_langinfo(D_T_FMT), tm);
    fprintf(fp, "%*s%s\n", KeyInfoPadding[KIP_VALID_TO],
            _(KeyInfoPrompts[KIP_VALID_TO]), shortbuf);
  }

  if (key->subkeys)
    s = gpgme_pubkey_algo_name(key->subkeys->pubkey_algo);
  else
    s = "?";

  s2 = is_pgp ? "PGP" : "X.509";

  if (key->subkeys)
    aval = key->subkeys->length;

  fprintf(fp, "%*s", KeyInfoPadding[KIP_KEY_TYPE], _(KeyInfoPrompts[KIP_KEY_TYPE]));
  /* L10N: This is printed after "Key Type: " and looks like this:
   *       PGP, 2048 bit RSA */
  fprintf(fp, _("%s, %lu bit %s\n"), s2, aval, s);

  fprintf(fp, "%*s", KeyInfoPadding[KIP_KEY_USAGE], _(KeyInfoPrompts[KIP_KEY_USAGE]));
  delim = "";

  if (key_check_cap(key, KEY_CAP_CAN_ENCRYPT))
  {
    /* L10N: value in Key Usage: field */
    fprintf(fp, "%s%s", delim, _("encryption"));
    delim = _(", ");
  }
  if (key_check_cap(key, KEY_CAP_CAN_SIGN))
  {
    /* L10N: value in Key Usage: field */
    fprintf(fp, "%s%s", delim, _("signing"));
    delim = _(", ");
  }
  if (key_check_cap(key, KEY_CAP_CAN_CERTIFY))
  {
    /* L10N: value in Key Usage: field */
    fprintf(fp, "%s%s", delim, _("certification"));
    delim = _(", ");
  }
  putc('\n', fp);

  if (key->subkeys)
  {
    s = key->subkeys->fpr;
    fprintf(fp, "%*s", KeyInfoPadding[KIP_FINGERPRINT], _(KeyInfoPrompts[KIP_FINGERPRINT]));
    if (is_pgp && strlen(s) == 40)
    {
      for (int i = 0; *s && s[1] && s[2] && s[3] && s[4]; s += 4, i++)
      {
        putc(*s, fp);
        putc(s[1], fp);
        putc(s[2], fp);
        putc(s[3], fp);
        putc(is_pgp ? ' ' : ':', fp);
        if (is_pgp && i == 4)
          putc(' ', fp);
      }
    }
    else
    {
      for (int i = 0; *s && s[1] && s[2]; s += 2, i++)
      {
        putc(*s, fp);
        putc(s[1], fp);
        putc(is_pgp ? ' ' : ':', fp);
        if (is_pgp && i == 7)
          putc(' ', fp);
      }
    }
    fprintf(fp, "%s\n", s);
  }

  if (key->issuer_serial)
  {
    s = key->issuer_serial;
    if (s)
      fprintf(fp, "%*s0x%s\n", KeyInfoPadding[KIP_SERIAL_NO],
              _(KeyInfoPrompts[KIP_SERIAL_NO]), s);
  }

  if (key->issuer_name)
  {
    s = key->issuer_name;
    if (s)
    {
      fprintf(fp, "%*s", KeyInfoPadding[KIP_ISSUED_BY], _(KeyInfoPrompts[KIP_ISSUED_BY]));
      parse_and_print_user_id(fp, s);
      putc('\n', fp);
    }
  }

  /* For PGP we list all subkeys. */
  if (is_pgp)
  {
    gpgme_subkey_t subkey = NULL;

    for (idx = 1, subkey = key->subkeys; subkey; idx++, subkey = subkey->next)
    {
      s = subkey->keyid;

      putc('\n', fp);
      if (strlen(s) == 16)
        s += 8; /* display only the short keyID */
      fprintf(fp, "%*s0x%s", KeyInfoPadding[KIP_SUBKEY], _(KeyInfoPrompts[KIP_SUBKEY]), s);
      if (subkey->revoked)
      {
        putc(' ', fp);
        /* L10N: describes a subkey */
        fputs(_("[Revoked]"), fp);
      }
      if (subkey->invalid)
      {
        putc(' ', fp);
        /* L10N: describes a subkey */
        fputs(_("[Invalid]"), fp);
      }
      if (subkey->expired)
      {
        putc(' ', fp);
        /* L10N: describes a subkey */
        fputs(_("[Expired]"), fp);
      }
      if (subkey->disabled)
      {
        putc(' ', fp);
        /* L10N: describes a subkey */
        fputs(_("[Disabled]"), fp);
      }
      putc('\n', fp);

      if (subkey->timestamp > 0)
      {
        tt = subkey->timestamp;

        tm = localtime(&tt);
        strftime(shortbuf, sizeof(shortbuf), nl_langinfo(D_T_FMT), tm);
        fprintf(fp, "%*s%s\n", KeyInfoPadding[KIP_VALID_FROM],
                _(KeyInfoPrompts[KIP_VALID_FROM]), shortbuf);
      }

      if (subkey->expires > 0)
      {
        tt = subkey->expires;

        tm = localtime(&tt);
        strftime(shortbuf, sizeof(shortbuf), nl_langinfo(D_T_FMT), tm);
        fprintf(fp, "%*s%s\n", KeyInfoPadding[KIP_VALID_TO],
                _(KeyInfoPrompts[KIP_VALID_TO]), shortbuf);
      }

      s = gpgme_pubkey_algo_name(subkey->pubkey_algo);

      aval = subkey->length;

      fprintf(fp, "%*s", KeyInfoPadding[KIP_KEY_TYPE], _(KeyInfoPrompts[KIP_KEY_TYPE]));
      fprintf(fp, _("%s, %lu bit %s\n"), "PGP", aval, s);

      fprintf(fp, "%*s", KeyInfoPadding[KIP_KEY_USAGE], _(KeyInfoPrompts[KIP_KEY_USAGE]));
      delim = "";

      if (subkey->can_encrypt)
      {
        fprintf(fp, "%s%s", delim, _("encryption"));
        delim = _(", ");
      }
      if (subkey->can_sign)
      {
        fprintf(fp, "%s%s", delim, _("signing"));
        delim = _(", ");
      }
      if (subkey->can_certify)
      {
        fprintf(fp, "%s%s", delim, _("certification"));
        delim = _(", ");
      }
      putc('\n', fp);
    }
  }
}

/**
 * verify_key - Show detailed information about the selected key
 */
static void verify_key(struct CryptKeyInfo *key)
{
  FILE *fp = NULL;
  char cmd[LONG_STRING], tempfile[_POSIX_PATH_MAX];
  const char *s = NULL;
  gpgme_ctx_t listctx = NULL;
  gpgme_error_t err;
  gpgme_key_t k = NULL;
  int maxdepth = 100;

  mutt_mktemp(tempfile, sizeof(tempfile));
  fp = safe_fopen(tempfile, "w");
  if (!fp)
  {
    mutt_perror(_("Can't create temporary file"));
    return;
  }
  mutt_message(_("Collecting data..."));

  print_key_info(key->kobj, fp);

  err = gpgme_new(&listctx);
  if (err)
  {
    fprintf(fp, "Internal error: can't create gpgme context: %s\n", gpgme_strerror(err));
    goto leave;
  }
  if ((key->flags & KEYFLAG_ISX509))
    gpgme_set_protocol(listctx, GPGME_PROTOCOL_CMS);

  k = key->kobj;
  gpgme_key_ref(k);
  while ((s = k->chain_id) && k->subkeys && (strcmp(s, k->subkeys->fpr) != 0))
  {
    putc('\n', fp);
    err = gpgme_op_keylist_start(listctx, s, 0);
    gpgme_key_unref(k);
    k = NULL;
    if (!err)
      err = gpgme_op_keylist_next(listctx, &k);
    if (err)
    {
      fprintf(fp, _("Error finding issuer key: %s\n"), gpgme_strerror(err));
      goto leave;
    }
    gpgme_op_keylist_end(listctx);

    print_key_info(k, fp);
    if (!--maxdepth)
    {
      putc('\n', fp);
      fputs(_("Error: certification chain too long - stopping here\n"), fp);
      break;
    }
  }

leave:
  gpgme_key_unref(k);
  gpgme_release(listctx);
  safe_fclose(&fp);
  mutt_clear_error();
  snprintf(cmd, sizeof(cmd), _("Key ID: 0x%s"), crypt_keyid(key));
  mutt_do_pager(cmd, tempfile, 0, NULL);
}

/*
 * Implementation of `findkeys'.
 */

/**
 * list_to_pattern - Convert STailQ to GPGME-compatible pattern
 *
 * We need to convert spaces in an item into a '+' and '%' into "%25".
 */
static char *list_to_pattern(struct ListHead *list)
{
  char *pattern = NULL, *p = NULL;
  const char *s = NULL;
  size_t n;

  n = 0;
  struct ListNode *np;
  STAILQ_FOREACH(np, list, entries)
  {
    for (s = np->data; *s; s++)
    {
      if (*s == '%' || *s == '+')
        n += 2;
      n++;
    }
    n++; /* delimiter or end of string */
  }
  n++; /* make sure to allocate at least one byte */
  pattern = p = safe_calloc(1, n);
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
          *p++ = '+';
        else
          *p++ = *s;
      }
    }
  }
  *p = 0;
  return pattern;
}

/**
 * get_candidates - Get a list of keys which are candidates for the selection
 *
 * Select by looking at the HINTS list.
 */
static struct CryptKeyInfo *get_candidates(struct ListHead *hints, unsigned int app, int secret)
{
  struct CryptKeyInfo *db = NULL, *k = NULL, **kend = NULL;
  char *pattern = NULL;
  gpgme_error_t err;
  gpgme_ctx_t ctx;
  gpgme_key_t key;
  int idx;
  gpgme_user_id_t uid = NULL;

  pattern = list_to_pattern(hints);
  if (!pattern)
    return NULL;

  err = gpgme_new(&ctx);
  if (err)
  {
    mutt_error(_("gpgme_new failed: %s"), gpgme_strerror(err));
    FREE(&pattern);
    return NULL;
  }

  db = NULL;
  kend = &db;

  if ((app & APPLICATION_PGP))
  {
    /* It's all a mess.  That old GPGME expects different things
         depending on the protocol.  For gpg we don't need percent
         escaped pappert but simple strings passed in an array to the
         keylist_ext_start function. */
    size_t n = 0;
    struct ListNode *np;
    STAILQ_FOREACH(np, hints, entries)
    {
      if (np->data && *np->data)
        n++;
    }
    if (!n)
      goto no_pgphints;

    char **patarr = safe_calloc(n + 1, sizeof(*patarr));
    n = 0;
    STAILQ_FOREACH(np, hints, entries)
    {
      if (np->data && *np->data)
        patarr[n++] = safe_strdup(np->data);
    }
    patarr[n] = NULL;
    err = gpgme_op_keylist_ext_start(ctx, (const char **) patarr, secret, 0);
    for (n = 0; patarr[n]; n++)
      FREE(&patarr[n]);
    FREE(&patarr);
    if (err)
    {
      mutt_error(_("gpgme_op_keylist_start failed: %s"), gpgme_strerror(err));
      gpgme_release(ctx);
      FREE(&pattern);
      return NULL;
    }

    while (!(err = gpgme_op_keylist_next(ctx, &key)))
    {
      unsigned int flags = 0;

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
        k = safe_calloc(1, sizeof(*k));
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
    if (err)
    {
      mutt_error(_("gpgme_op_keylist_start failed: %s"), gpgme_strerror(err));
      gpgme_release(ctx);
      FREE(&pattern);
      return NULL;
    }

    while (!(err = gpgme_op_keylist_next(ctx, &key)))
    {
      unsigned int flags = KEYFLAG_ISX509;

      if (key_check_cap(key, KEY_CAP_CAN_ENCRYPT))
        flags |= KEYFLAG_CANENCRYPT;
      if (key_check_cap(key, KEY_CAP_CAN_SIGN))
        flags |= KEYFLAG_CANSIGN;

      for (idx = 0, uid = key->uids; uid; idx++, uid = uid->next)
      {
        k = safe_calloc(1, sizeof(*k));
        k->kobj = key;
        gpgme_key_ref(k->kobj);
        k->idx = idx;
        k->uid = uid->uid;
        k->flags = flags;
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
 * crypt_add_string_to_hints - Add the string STR to the list HINTS
 *
 * This list is later used to match addresses.
 */
static void crypt_add_string_to_hints(struct ListHead *hints, const char *str)
{
  char *scratch = NULL;
  char *t = NULL;

  scratch = safe_strdup(str);
  if (!scratch)
    return;

  for (t = strtok(scratch, " ,.:\"()<>\n"); t; t = strtok(NULL, " ,.:\"()<>\n"))
  {
    if (strlen(t) > 3)
      mutt_list_insert_tail(hints, safe_strdup(t));
  }

  FREE(&scratch);
}

/**
 * crypt_select_key - Get the user to select a key
 *
 * Display a menu to select a key from the array KEYS. FORCED_VALID will be set
 * to true on return if the user did override the key's validity.
 */
static struct CryptKeyInfo *crypt_select_key(struct CryptKeyInfo *keys,
                                             struct Address *p, const char *s,
                                             unsigned int app, int *forced_valid)
{
  int keymax;
  struct CryptKeyInfo **key_table = NULL;
  struct Menu *menu = NULL;
  int i;
  bool done = false;
  char helpstr[LONG_STRING], buf[LONG_STRING];
  struct CryptKeyInfo *k = NULL;
  int (*f)(const void *, const void *);
  int menu_to_use = 0;
  bool unusable = false;

  *forced_valid = 0;

  /* build the key table */
  keymax = i = 0;
  key_table = NULL;
  for (k = keys; k; k = k->next)
  {
    if (!option(OPT_PGP_SHOW_UNUSABLE) && (k->flags & KEYFLAG_CANTUSE))
    {
      unusable = true;
      continue;
    }

    if (i == keymax)
    {
      keymax += 20;
      safe_realloc(&key_table, sizeof(struct CryptKeyInfo *) * keymax);
    }

    key_table[i++] = k;
  }

  if (!i && unusable)
  {
    mutt_error(_("All matching keys are marked expired/revoked."));
    mutt_sleep(1);
    return NULL;
  }

  switch (PgpSortKeys & SORT_MASK)
  {
    case SORT_DATE:
      f = crypt_compare_date;
      break;
    case SORT_KEYID:
      f = crypt_compare_keyid;
      break;
    case SORT_ADDRESS:
      f = crypt_compare_address;
      break;
    case SORT_TRUST:
    default:
      f = crypt_compare_trust;
      break;
  }
  qsort(key_table, i, sizeof(struct CryptKeyInfo *), f);

  if (app & APPLICATION_PGP)
    menu_to_use = MENU_KEY_SELECT_PGP;
  else if (app & APPLICATION_SMIME)
    menu_to_use = MENU_KEY_SELECT_SMIME;

  helpstr[0] = 0;
  mutt_make_help(buf, sizeof(buf), _("Exit  "), menu_to_use, OP_EXIT);
  strcat(helpstr, buf);
  mutt_make_help(buf, sizeof(buf), _("Select  "), menu_to_use, OP_GENERIC_SELECT_ENTRY);
  strcat(helpstr, buf);
  mutt_make_help(buf, sizeof(buf), _("Check key  "), menu_to_use, OP_VERIFY_KEY);
  strcat(helpstr, buf);
  mutt_make_help(buf, sizeof(buf), _("Help"), menu_to_use, OP_HELP);
  strcat(helpstr, buf);

  menu = mutt_new_menu(menu_to_use);
  menu->max = i;
  menu->make_entry = crypt_entry;
  menu->help = helpstr;
  menu->data = key_table;
  mutt_push_current_menu(menu);

  {
    const char *ts = NULL;

    if ((app & APPLICATION_PGP) && (app & APPLICATION_SMIME))
      ts = _("PGP and S/MIME keys matching");
    else if ((app & APPLICATION_PGP))
      ts = _("PGP keys matching");
    else if ((app & APPLICATION_SMIME))
      ts = _("S/MIME keys matching");
    else
      ts = _("keys matching");

    if (p)
      /* L10N:
         %1$s is one of the previous four entries.
         %2$s is an address.
         e.g. "S/MIME keys matching <me@mutt.org>." */
      snprintf(buf, sizeof(buf), _("%s <%s>."), ts, p->mailbox);
    else
      /* L10N:
         e.g. 'S/MIME keys matching "Michael Elkins".' */
      snprintf(buf, sizeof(buf), _("%s \"%s\"."), ts, s);
    menu->title = buf;
  }

  mutt_clear_error();
  k = NULL;
  while (!done)
  {
    *forced_valid = 0;
    switch (mutt_menu_loop(menu))
    {
      case OP_VERIFY_KEY:
        verify_key(key_table[menu->current]);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_VIEW_ID:
        mutt_message("%s", key_table[menu->current]->uid);
        break;

      case OP_GENERIC_SELECT_ENTRY:
        /* FIXME make error reporting more verbose - this should be
             easy because gpgme provides more information */
        if (option(OPT_PGP_CHECK_TRUST))
        {
          if (!crypt_key_is_valid(key_table[menu->current]))
          {
            mutt_error(_("This key can't be used: "
                         "expired/disabled/revoked."));
            break;
          }
        }

        if (option(OPT_PGP_CHECK_TRUST) &&
            (!crypt_id_is_valid(key_table[menu->current]) ||
             !crypt_id_is_strong(key_table[menu->current])))
        {
          const char *warn_s = NULL;
          char buff[LONG_STRING];

          if (key_table[menu->current]->flags & KEYFLAG_CANTUSE)
            warn_s = N_("ID is expired/disabled/revoked.");
          else
          {
            warn_s = "??";
            switch (key_table[menu->current]->validity)
            {
              case GPGME_VALIDITY_UNKNOWN:
              case GPGME_VALIDITY_UNDEFINED:
                warn_s = N_("ID has undefined validity.");
                break;
              case GPGME_VALIDITY_NEVER:
                warn_s = N_("ID is not valid.");
                break;
              case GPGME_VALIDITY_MARGINAL:
                warn_s = N_("ID is only marginally valid.");
                break;
              case GPGME_VALIDITY_FULL:
              case GPGME_VALIDITY_ULTIMATE:
                break;
            }
          }

          snprintf(buff, sizeof(buff),
                   _("%s Do you really want to use the key?"), _(warn_s));

          if (mutt_yesorno(buff, 0) != MUTT_YES)
          {
            mutt_clear_error();
            break;
          }
          *forced_valid = 1;
        }

        k = crypt_copy_key(key_table[menu->current]);
        done = true;
        break;

      case OP_EXIT:
        k = NULL;
        done = true;
        break;
    }
  }

  mutt_pop_current_menu(menu);
  mutt_menu_destroy(&menu);
  FREE(&key_table);

  return k;
}

static struct CryptKeyInfo *crypt_getkeybyaddr(struct Address *a,
                                               short abilities, unsigned int app,
                                               int *forced_valid, bool oppenc_mode)
{
  struct Address *r = NULL, *p = NULL;
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

  *forced_valid = 0;

  if (a && a->mailbox)
    crypt_add_string_to_hints(&hints, a->mailbox);
  if (a && a->personal)
    crypt_add_string_to_hints(&hints, a->personal);

  if (!oppenc_mode)
    mutt_message(_("Looking for keys matching \"%s\"..."), a ? a->mailbox : "");
  keys = get_candidates(&hints, app, (abilities & KEYFLAG_CANSIGN));

  mutt_list_free(&hints);

  if (!keys)
    return NULL;

  mutt_debug(5, "crypt_getkeybyaddr: looking for %s <%s>.\n",
             a ? a->personal : "", a ? a->mailbox : "");

  for (k = keys; k; k = k->next)
  {
    mutt_debug(5, "  looking at key: %s `%.15s'\n", crypt_keyid(k), k->uid);

    if (abilities && !(k->flags & abilities))
    {
      mutt_debug(5, "  insufficient abilities: Has %x, want %x\n", k->flags, abilities);
      continue;
    }

    this_key_has_strong = false; /* strong and valid match */
    this_key_has_addr_match = false;
    match = false; /* any match */

    r = rfc822_parse_adrlist(NULL, k->uid);
    for (p = r; p; p = p->next)
    {
      int validity = crypt_id_matches_addr(a, p, k);

      if (validity & CRYPT_KV_MATCH) /* something matches */
      {
        match = true;

        if ((validity & CRYPT_KV_VALID) && (validity & CRYPT_KV_ADDR))
        {
          if (validity & CRYPT_KV_STRONGID)
          {
            if (the_strong_valid_key && the_strong_valid_key->kobj != k->kobj)
              multi = true;
            this_key_has_strong = true;
          }
          else
            this_key_has_addr_match = true;
        }
      }
    }
    rfc822_free_address(&r);

    if (match)
    {
      struct CryptKeyInfo *tmp = NULL;

      *matches_endp = tmp = crypt_copy_key(k);
      matches_endp = &tmp->next;

      if (this_key_has_strong)
        the_strong_valid_key = tmp;
      else if (this_key_has_addr_match)
        a_valid_addrmatch_key = tmp;
    }
  }

  crypt_free_key(&keys);

  if (matches)
  {
    if (oppenc_mode)
    {
      if (the_strong_valid_key)
        k = crypt_copy_key(the_strong_valid_key);
      else if (a_valid_addrmatch_key)
        k = crypt_copy_key(a_valid_addrmatch_key);
      else
        k = NULL;
    }
    else if (the_strong_valid_key && !multi)
    {
      /*
           * There was precisely one strong match on a valid ID.
           *
           * Proceed without asking the user.
           */
      k = crypt_copy_key(the_strong_valid_key);
    }
    else
    {
      /*
           * Else: Ask the user.
           */
      k = crypt_select_key(matches, a, NULL, app, forced_valid);
    }

    crypt_free_key(&matches);
  }
  else
    k = NULL;

  return k;
}

static struct CryptKeyInfo *crypt_getkeybystr(char *p, short abilities,
                                              unsigned int app, int *forced_valid)
{
  struct ListHead hints = STAILQ_HEAD_INITIALIZER(hints);
  struct CryptKeyInfo *keys = NULL;
  struct CryptKeyInfo *matches = NULL;
  struct CryptKeyInfo **matches_endp = &matches;
  struct CryptKeyInfo *k = NULL;
  const char *ps = NULL, *pl = NULL, *pfcopy = NULL, *phint = NULL;

  mutt_message(_("Looking for keys matching \"%s\"..."), p);

  *forced_valid = 0;

  pfcopy = crypt_get_fingerprint_or_id(p, &phint, &pl, &ps);
  crypt_add_string_to_hints(&hints, phint);
  keys = get_candidates(&hints, app, (abilities & KEYFLAG_CANSIGN));
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

    mutt_debug(5, "crypt_getkeybystr: matching \"%s\" against "
                  "key %s, \"%s\": ",
               p, crypt_long_keyid(k), k->uid);

    if (!*p || (pfcopy && (mutt_strcasecmp(pfcopy, crypt_fpr(k)) == 0)) ||
        (pl && (mutt_strcasecmp(pl, crypt_long_keyid(k)) == 0)) ||
        (ps && (mutt_strcasecmp(ps, crypt_short_keyid(k)) == 0)) ||
        mutt_stristr(k->uid, p))
    {
      struct CryptKeyInfo *tmp = NULL;

      mutt_debug(5, "match.\n");

      *matches_endp = tmp = crypt_copy_key(k);
      matches_endp = &tmp->next;
    }
    else
    {
      mutt_debug(5, "no match.\n");
    }
  }

  FREE(&pfcopy);
  crypt_free_key(&keys);

  if (matches)
  {
    k = crypt_select_key(matches, NULL, p, app, forced_valid);
    crypt_free_key(&matches);
    return k;
  }

  return NULL;
}

/**
 * crypt_ask_for_key - Ask the user for a key
 *
 * Display TAG as a prompt to ask for a key.  If WHATFOR is not null use it as
 * default and store it under that label as the next default.  ABILITIES
 * describe the required key abilities (sign, encrypt) and APP the type of the
 * requested key; ether S/MIME or PGP.  Return a copy of the key or NULL if not
 * found.
 */
static struct CryptKeyInfo *crypt_ask_for_key(char *tag, char *whatfor, short abilities,
                                              unsigned int app, int *forced_valid)
{
  struct CryptKeyInfo *key = NULL;
  char resp[SHORT_STRING];
  struct CryptCache *l = NULL;
  int dummy;

  if (!forced_valid)
    forced_valid = &dummy;

  mutt_clear_error();

  *forced_valid = 0;
  resp[0] = 0;
  if (whatfor)
  {
    for (l = id_defaults; l; l = l->next)
      if (mutt_strcasecmp(whatfor, l->what) == 0)
      {
        strfcpy(resp, NONULL(l->dflt), sizeof(resp));
        break;
      }
  }

  for (;;)
  {
    resp[0] = 0;
    if (mutt_get_field(tag, resp, sizeof(resp), MUTT_CLEAR) != 0)
      return NULL;

    if (whatfor)
    {
      if (l)
        mutt_str_replace(&l->dflt, resp);
      else
      {
        l = safe_malloc(sizeof(struct CryptCache));
        l->next = id_defaults;
        id_defaults = l;
        l->what = safe_strdup(whatfor);
        l->dflt = safe_strdup(resp);
      }
    }

    if ((key = crypt_getkeybystr(resp, abilities, app, forced_valid)))
      return key;

    mutt_error(_("No matching keys found for \"%s\""), resp);
    mutt_sleep(0);
  }
  /* not reached */
}

/**
 * find_keys - Find keys of the recipients of the message
 * @retval NULL if any of the keys can not be found
 *
 * If oppenc_mode is true, only keys that can be determined without prompting
 * will be used.
 */
static char *find_keys(struct Address *adrlist, unsigned int app, int oppenc_mode)
{
  struct ListHead crypt_hook_list = STAILQ_HEAD_INITIALIZER(crypt_hook_list);
  struct ListNode *crypt_hook = NULL;
  char *crypt_hook_val = NULL;
  const char *keyID = NULL;
  char *keylist = NULL, *t = NULL;
  size_t keylist_size = 0;
  size_t keylist_used = 0;
  struct Address *addr = NULL;
  struct Address *p = NULL, *q = NULL;
  struct CryptKeyInfo *k_info = NULL;
  const char *fqdn = mutt_fqdn(1);
  char buf[LONG_STRING];
  int forced_valid;
  int r;
  bool key_selected;

  for (p = adrlist; p; p = p->next)
  {
    key_selected = false;
    mutt_crypt_hook(&crypt_hook_list, p);
    crypt_hook = STAILQ_FIRST(&crypt_hook_list);
    do
    {
      q = p;
      forced_valid = 0;
      k_info = NULL;

      if (crypt_hook)
      {
        crypt_hook_val = crypt_hook->data;
        r = MUTT_YES;
        if (!oppenc_mode && option(OPT_CRYPT_CONFIRMHOOK))
        {
          snprintf(buf, sizeof(buf), _("Use keyID = \"%s\" for %s?"),
                   crypt_hook_val, p->mailbox);
          r = mutt_yesorno(buf, MUTT_YES);
        }
        if (r == MUTT_YES)
        {
          if (crypt_is_numerical_keyid(crypt_hook_val))
          {
            keyID = crypt_hook_val;
            if (strncmp(keyID, "0x", 2) == 0)
              keyID += 2;
            goto bypass_selection; /* you don't see this. */
          }

          /* check for e-mail address */
          if ((t = strchr(crypt_hook_val, '@')) &&
              (addr = rfc822_parse_adrlist(NULL, crypt_hook_val)))
          {
            if (fqdn)
              rfc822_qualify(addr, fqdn);
            q = addr;
          }
          else if (!oppenc_mode)
          {
            k_info = crypt_getkeybystr(crypt_hook_val, KEYFLAG_CANENCRYPT, app, &forced_valid);
          }
        }
        else if (r == MUTT_NO)
        {
          if (key_selected || STAILQ_NEXT(crypt_hook, entries))
          {
            crypt_hook = STAILQ_NEXT(crypt_hook, entries);
            continue;
          }
        }
        else if (r == MUTT_ABORT)
        {
          FREE(&keylist);
          rfc822_free_address(&addr);
          mutt_list_free(&crypt_hook_list);
          return NULL;
        }
      }

      if (!k_info)
      {
        k_info = crypt_getkeybyaddr(q, KEYFLAG_CANENCRYPT, app, &forced_valid, oppenc_mode);
      }

      if (!k_info && !oppenc_mode)
      {
        snprintf(buf, sizeof(buf), _("Enter keyID for %s: "), q->mailbox);

        k_info = crypt_ask_for_key(buf, q->mailbox, KEYFLAG_CANENCRYPT, app, &forced_valid);
      }

      if (!k_info)
      {
        FREE(&keylist);
        rfc822_free_address(&addr);
        mutt_list_free(&crypt_hook_list);
        return NULL;
      }

      keyID = crypt_fpr_or_lkeyid(k_info);

    bypass_selection:
      keylist_size += mutt_strlen(keyID) + 4 + 1;
      safe_realloc(&keylist, keylist_size);
      sprintf(keylist + keylist_used, "%s0x%s%s", keylist_used ? " " : "",
              keyID, forced_valid ? "!" : "");
      keylist_used = mutt_strlen(keylist);

      key_selected = true;

      crypt_free_key(&k_info);
      rfc822_free_address(&addr);

      if (crypt_hook)
        crypt_hook = STAILQ_NEXT(crypt_hook, entries);

    } while (crypt_hook);

    mutt_list_free(&crypt_hook_list);
  }
  return keylist;
}

char *pgp_gpgme_findkeys(struct Address *adrlist, int oppenc_mode)
{
  return find_keys(adrlist, APPLICATION_PGP, oppenc_mode);
}

char *smime_gpgme_findkeys(struct Address *adrlist, int oppenc_mode)
{
  return find_keys(adrlist, APPLICATION_SMIME, oppenc_mode);
}

#ifdef HAVE_GPGME_OP_EXPORT_KEYS
struct Body *pgp_gpgme_make_key_attachment(char *tempf)
{
  struct CryptKeyInfo *key = NULL;
  gpgme_ctx_t context = NULL;
  gpgme_key_t export_keys[2];
  gpgme_data_t keydata = NULL;
  gpgme_error_t err;
  struct Body *att = NULL;
  char buff[LONG_STRING];
  struct stat sb;

  unset_option(OPT_PGP_CHECK_TRUST);

  key = crypt_ask_for_key(_("Please enter the key ID: "), NULL, 0, APPLICATION_PGP, NULL);
  if (!key)
    goto bail;
  export_keys[0] = key->kobj;
  export_keys[1] = NULL;

  context = create_gpgme_context(0);
  gpgme_set_armor(context, 1);
  keydata = create_gpgme_data();
  err = gpgme_op_export_keys(context, export_keys, 0, keydata);
  if (err != GPG_ERR_NO_ERROR)
  {
    mutt_error(_("Error exporting key: %s\n"), gpgme_strerror(err));
    mutt_sleep(1);
    goto bail;
  }

  tempf = data_object_to_tempfile(keydata, tempf, NULL);
  if (!tempf)
    goto bail;

  att = mutt_new_body();
  /* tempf is a newly allocated string, so this is correct: */
  att->filename = tempf;
  att->unlink = true;
  att->use_disp = false;
  att->type = TYPEAPPLICATION;
  att->subtype = safe_strdup("pgp-keys");
  /* L10N:
     MIME description for exported (attached) keys.
     You can translate this entry to a non-ASCII string (it will be encoded),
     but it may be safer to keep it untranslated. */
  snprintf(buff, sizeof(buff), _("PGP Key 0x%s."), crypt_keyid(key));
  att->description = safe_strdup(buff);
  mutt_update_encoding(att);

  stat(tempf, &sb);
  att->length = sb.st_size;

bail:
  crypt_free_key(&key);
  gpgme_data_release(keydata);
  gpgme_release(context);

  return att;
}
#endif

/*
 * Implementation of `init'.
 */

/**
 * init_common - Initialise code common to PGP and SMIME parts of GPGME
 */
static void init_common(void)
{
  /* this initialization should only run one time, but it may be called by
   * either pgp_gpgme_init or smime_gpgme_init */
  static bool has_run = false;
  if (!has_run)
  {
    gpgme_check_version(NULL);
    gpgme_set_locale(NULL, LC_CTYPE, setlocale(LC_CTYPE, NULL));
#ifdef ENABLE_NLS
    gpgme_set_locale(NULL, LC_MESSAGES, setlocale(LC_MESSAGES, NULL));
#endif
    has_run = true;
  }
}

static void init_pgp(void)
{
  if (gpgme_engine_check_version(GPGME_PROTOCOL_OpenPGP) != GPG_ERR_NO_ERROR)
  {
    mutt_error(_("GPGME: OpenPGP protocol not available"));
  }
}

static void init_smime(void)
{
  if (gpgme_engine_check_version(GPGME_PROTOCOL_CMS) != GPG_ERR_NO_ERROR)
  {
    mutt_error(_("GPGME: CMS protocol not available"));
  }
}

void pgp_gpgme_init(void)
{
  init_common();
  init_pgp();
}

void smime_gpgme_init(void)
{
  init_common();
  init_smime();
}

static int gpgme_send_menu(struct Header *msg, int is_smime)
{
  struct CryptKeyInfo *p = NULL;
  char input_signas[SHORT_STRING];
  char *prompt = NULL, *letters = NULL, *choices = NULL;
  int choice;

  if (is_smime)
    msg->security |= APPLICATION_SMIME;
  else
    msg->security |= APPLICATION_PGP;

  /*
   * Opportunistic encrypt is controlling encryption.
   * NOTE: "Signing" and "Clearing" only adjust the sign bit, so we have different
   *       letter choices for those.
   */
  if (option(OPT_CRYPT_OPPORTUNISTIC_ENCRYPT) && (msg->security & OPPENCRYPT))
  {
    if (is_smime)
    {
      /* L10N: S/MIME options (opportunistic encryption is on) */
      prompt =
          _("S/MIME (s)ign, sign (a)s, (p)gp, (c)lear, or (o)ppenc mode off? ");
      /* L10N: S/MIME options (opportunistic encryption is on) */
      letters = _("sapco");
      choices = "SapCo";
    }
    else
    {
      /* L10N: PGP options (opportunistic encryption is on) */
      prompt =
          _("PGP (s)ign, sign (a)s, s/(m)ime, (c)lear, or (o)ppenc mode off? ");
      /* L10N: PGP options (opportunistic encryption is on) */
      letters = _("samco");
      choices = "SamCo";
    }
  }
  /*
   * Opportunistic encryption option is set, but is toggled off
   * for this message.
   */
  else if (option(OPT_CRYPT_OPPORTUNISTIC_ENCRYPT))
  {
    if (is_smime)
    {
      /* L10N: S/MIME options (opportunistic encryption is off) */
      prompt = _("S/MIME (e)ncrypt, (s)ign, sign (a)s, (b)oth, (p)gp, (c)lear, "
                 "or (o)ppenc mode? ");
      /* L10N: S/MIME options (opportunistic encryption is off) */
      letters = _("esabpco");
      choices = "esabpcO";
    }
    else
    {
      /* L10N: PGP options (opportunistic encryption is off) */
      prompt = _("PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, s/(m)ime, (c)lear, "
                 "or (o)ppenc mode? ");
      /* L10N: PGP options (opportunistic encryption is off) */
      letters = _("esabmco");
      choices = "esabmcO";
    }
  }
  /*
   * Opportunistic encryption is unset
   */
  else
  {
    if (is_smime)
    {
      /* L10N: S/MIME options */
      prompt =
          _("S/MIME (e)ncrypt, (s)ign, sign (a)s, (b)oth, (p)gp or (c)lear? ");
      /* L10N: S/MIME options */
      letters = _("esabpc");
      choices = "esabpc";
    }
    else
    {
      /* L10N: PGP options */
      prompt =
          _("PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, s/(m)ime or (c)lear? ");
      /* L10N: PGP options */
      letters = _("esabmc");
      choices = "esabmc";
    }
  }

  choice = mutt_multi_choice(prompt, letters);
  if (choice > 0)
  {
    switch (choices[choice - 1])
    {
      case 'e': /* (e)ncrypt */
        msg->security |= ENCRYPT;
        msg->security &= ~SIGN;
        break;

      case 's': /* (s)ign */
        msg->security &= ~ENCRYPT;
        msg->security |= SIGN;
        break;

      case 'S': /* (s)ign in oppenc mode */
        msg->security |= SIGN;
        break;

      case 'a': /* sign (a)s */
        if ((p = crypt_ask_for_key(_("Sign as: "), NULL, KEYFLAG_CANSIGN,
                                   is_smime ? APPLICATION_SMIME : APPLICATION_PGP, NULL)))
        {
          snprintf(input_signas, sizeof(input_signas), "0x%s", crypt_fpr_or_lkeyid(p));
          mutt_str_replace(is_smime ? &SmimeDefaultKey : &PgpSignAs, input_signas);
          crypt_free_key(&p);

          msg->security |= SIGN;
        }
        break;

      case 'b': /* (b)oth */
        msg->security |= (ENCRYPT | SIGN);
        break;

      case 'p': /* (p)gp or s/(m)ime */
      case 'm':
        is_smime = !is_smime;
        if (is_smime)
        {
          msg->security &= ~APPLICATION_PGP;
          msg->security |= APPLICATION_SMIME;
        }
        else
        {
          msg->security &= ~APPLICATION_SMIME;
          msg->security |= APPLICATION_PGP;
        }
        crypt_opportunistic_encrypt(msg);
        break;

      case 'c': /* (c)lear */
        msg->security &= ~(ENCRYPT | SIGN);
        break;

      case 'C':
        msg->security &= ~SIGN;
        break;

      case 'O': /* oppenc mode on */
        msg->security |= OPPENCRYPT;
        crypt_opportunistic_encrypt(msg);
        break;

      case 'o': /* oppenc mode off */
        msg->security &= ~OPPENCRYPT;
        break;
    }
  }

  return msg->security;
}

int pgp_gpgme_send_menu(struct Header *msg)
{
  return gpgme_send_menu(msg, 0);
}

int smime_gpgme_send_menu(struct Header *msg)
{
  return gpgme_send_menu(msg, 1);
}

static int verify_sender(struct Header *h, gpgme_protocol_t protocol)
{
  struct Address *sender = NULL;
  unsigned int ret = 1;

  if (h->env->from)
  {
    h->env->from = mutt_expand_aliases(h->env->from);
    sender = h->env->from;
  }
  else if (h->env->sender)
  {
    h->env->sender = mutt_expand_aliases(h->env->sender);
    sender = h->env->sender;
  }

  if (sender)
  {
    if (signature_key)
    {
      gpgme_key_t key = signature_key;
      gpgme_user_id_t uid = NULL;
      int sender_length = 0;
      int uid_length = 0;

      sender_length = strlen(sender->mailbox);
      for (uid = key->uids; uid && ret; uid = uid->next)
      {
        uid_length = strlen(uid->email);
        if (1 && (uid->email[0] == '<') && (uid->email[uid_length - 1] == '>') &&
            (uid_length == sender_length + 2))
        {
          const char *at_sign = strchr(uid->email + 1, '@');
          if (!at_sign)
          {
            if (strncmp(uid->email + 1, sender->mailbox, sender_length) == 0)
              ret = 0;
          }
          else
          {
            /*
             * Assume address is 'mailbox@domainname'.
             * The mailbox part is case-sensitive,
             * the domainname is not. (RFC2821)
             */
            const char *tmp_email = uid->email + 1;
            const char *tmp_sender = sender->mailbox;
            /* length of mailbox part including '@' */
            int mailbox_length = at_sign - tmp_email + 1;
            int domainname_length = sender_length - mailbox_length;
            int mailbox_match, domainname_match;

            mailbox_match = (strncmp(tmp_email, tmp_sender, mailbox_length) == 0);
            tmp_email += mailbox_length;
            tmp_sender += mailbox_length;
            domainname_match =
                (strncasecmp(tmp_email, tmp_sender, domainname_length) == 0);
            if (mailbox_match && domainname_match)
              ret = 0;
          }
        }
      }
    }
    else
      mutt_any_key_to_continue(_("Failed to verify sender"));
  }
  else
    mutt_any_key_to_continue(_("Failed to figure out sender"));

  if (signature_key)
  {
    gpgme_key_unref(signature_key);
    signature_key = NULL;
  }

  return ret;
}

int smime_gpgme_verify_sender(struct Header *h)
{
  return verify_sender(h, GPGME_PROTOCOL_CMS);
}

void mutt_gpgme_set_sender(const char *sender)
{
  FREE(&current_sender);
  current_sender = safe_strdup(sender);
}
