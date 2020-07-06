/**
 * @file
 * SMIME helper routines
 *
 * @authors
 * Copyright (C) 2001-2002 Oliver Ehli <elmy@acm.org>
 * Copyright (C) 2002 Mike Schiraldi <raldi@research.netsol.com>
 * Copyright (C) 2004 g10 Code GmbH
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
 * @page crypt_smime SMIME helper routines
 *
 * SMIME helper routines
 */

#include "config.h"
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "copy.h"
#include "crypt.h"
#include "cryptglue.h"
#include "format_flags.h"
#include "handler.h"
#include "keymap.h"
#include "mutt_globals.h"
#include "mutt_logging.h"
#include "mutt_menu.h"
#include "muttlib.h"
#include "opcodes.h"
#include "protos.h"
#include "state.h"
#include "ncrypt/lib.h"
#include "send/lib.h"
#ifdef CRYPT_BACKEND_CLASSIC_SMIME
#include "smime.h"
#endif

/* These Config Variables are only used in ncrypt/smime.c */
bool C_SmimeAskCertLabel; ///< Config: Prompt the user for a label for SMIME certificates
char *C_SmimeCaLocation;   ///< Config: File containing trusted certificates
char *C_SmimeCertificates; ///< Config: File containing user's public certificates
char *C_SmimeDecryptCommand; ///< Config: (smime) External command to decrypt an SMIME message
bool C_SmimeDecryptUseDefaultKey; ///< Config: Use the default key for decryption
char *C_SmimeEncryptCommand; ///< Config: (smime) External command to encrypt a message
char *C_SmimeGetCertCommand; ///< Config: (smime) External command to extract a certificate from a message
char *C_SmimeGetCertEmailCommand; ///< Config: (smime) External command to get a certificate for an email
char *C_SmimeGetSignerCertCommand; ///< Config: (smime) External command to extract a certificate from an email
char *C_SmimeImportCertCommand; ///< Config: (smime) External command to import a certificate
char *C_SmimeKeys; ///< Config: File containing user's private certificates
char *C_SmimePk7outCommand; ///< Config: (smime) External command to extract a public certificate
char *C_SmimeSignCommand; ///< Config: (smime) External command to sign a message
char *C_SmimeSignDigestAlg; ///< Config: Digest algorithm
long C_SmimeTimeout;        ///< Config: Time in seconds to cache a passphrase
char *C_SmimeVerifyCommand; ///< Config: (smime) External command to verify a signed message
char *C_SmimeVerifyOpaqueCommand; ///< Config: (smime) External command to verify a signature

/**
 * struct SmimeCommandContext - Data for a SIME command
 */
struct SmimeCommandContext
{
  const char *key;           ///< %k
  const char *cryptalg;      ///< %a
  const char *digestalg;     ///< %d
  const char *fname;         ///< %f
  const char *sig_fname;     ///< %s
  const char *certificates;  ///< %c
  const char *intermediates; ///< %i
};

char SmimePass[256];
time_t SmimeExpTime = 0; /* when does the cached passphrase expire? */

static struct Buffer SmimeKeyToUse = { 0 };
static struct Buffer SmimeCertToUse = { 0 };
static struct Buffer SmimeIntermediateToUse = { 0 };

/**
 * smime_init - Initialise smime globals
 */
void smime_init(void)
{
  mutt_buffer_alloc(&SmimeKeyToUse, 256);
  mutt_buffer_alloc(&SmimeCertToUse, 256);
  mutt_buffer_alloc(&SmimeIntermediateToUse, 256);
}

/**
 * smime_cleanup - Clean up smime globals
 */
void smime_cleanup(void)
{
  mutt_buffer_dealloc(&SmimeKeyToUse);
  mutt_buffer_dealloc(&SmimeCertToUse);
  mutt_buffer_dealloc(&SmimeIntermediateToUse);
}

/**
 * smime_key_free - Free a list of SMIME keys
 * @param[out] keylist List of keys to free
 */
static void smime_key_free(struct SmimeKey **keylist)
{
  if (!keylist)
    return;

  struct SmimeKey *key = NULL;

  while (*keylist)
  {
    key = *keylist;
    *keylist = (*keylist)->next;

    FREE(&key->email);
    FREE(&key->hash);
    FREE(&key->label);
    FREE(&key->issuer);
    FREE(&key);
  }
}

/**
 * smime_copy_key - Copy an SMIME key
 * @param key Key to copy
 * @retval ptr Newly allocated SMIME key
 */
static struct SmimeKey *smime_copy_key(struct SmimeKey *key)
{
  if (!key)
    return NULL;

  struct SmimeKey *copy = NULL;

  copy = mutt_mem_calloc(1, sizeof(struct SmimeKey));
  copy->email = mutt_str_dup(key->email);
  copy->hash = mutt_str_dup(key->hash);
  copy->label = mutt_str_dup(key->label);
  copy->issuer = mutt_str_dup(key->issuer);
  copy->trust = key->trust;
  copy->flags = key->flags;

  return copy;
}

/*
 *     Queries and passphrase handling.
 */

/**
 * smime_class_void_passphrase - Implements CryptModuleSpecs::void_passphrase()
 */
void smime_class_void_passphrase(void)
{
  memset(SmimePass, 0, sizeof(SmimePass));
  SmimeExpTime = 0;
}

/**
 * smime_class_valid_passphrase - Implements CryptModuleSpecs::valid_passphrase()
 */
bool smime_class_valid_passphrase(void)
{
  const time_t now = mutt_date_epoch();
  if (now < SmimeExpTime)
  {
    /* Use cached copy.  */
    return true;
  }

  smime_class_void_passphrase();

  if (mutt_get_password(_("Enter S/MIME passphrase:"), SmimePass, sizeof(SmimePass)) == 0)
  {
    SmimeExpTime = mutt_date_add_timeout(now, C_SmimeTimeout);
    return true;
  }
  else
    SmimeExpTime = 0;

  return false;
}

/*
 *     The OpenSSL interface
 */

/**
 * smime_command_format_str - Format an SMIME command - Implements ::format_t
 *
 * | Expando | Description
 * |:--------|:-----------------------------------------------------------------
 * | \%a     | Algorithm used for encryption
 * | \%C     | CA location: Depending on whether `$smime_ca_location` points to a directory or file
 * | \%c     | One or more certificate IDs
 * | \%d     | Message digest algorithm specified with `$smime_sign_digest_alg`
 * | \%f     | File containing a message
 * | \%i     | Intermediate certificates
 * | \%k     | The key-pair specified with `$smime_default_key`
 * | \%s     | File containing the signature part of a multipart/signed attachment when verifying it
 */
static const char *smime_command_format_str(char *buf, size_t buflen, size_t col,
                                            int cols, char op, const char *src,
                                            const char *prec, const char *if_str,
                                            const char *else_str, intptr_t data,
                                            MuttFormatFlags flags)
{
  char fmt[128];
  struct SmimeCommandContext *cctx = (struct SmimeCommandContext *) data;
  bool optional = (flags & MUTT_FORMAT_OPTIONAL);

  switch (op)
  {
    case 'C':
    {
      if (!optional)
      {
        struct Buffer *path = mutt_buffer_pool_get();
        struct Buffer *buf1 = mutt_buffer_pool_get();
        struct Buffer *buf2 = mutt_buffer_pool_get();
        struct stat sb;

        mutt_buffer_strcpy(path, C_SmimeCaLocation);
        mutt_buffer_expand_path(path);
        mutt_buffer_quote_filename(buf1, mutt_b2s(path), true);

        if ((stat(mutt_b2s(path), &sb) != 0) || !S_ISDIR(sb.st_mode))
          mutt_buffer_printf(buf2, "-CAfile %s", mutt_b2s(buf1));
        else
          mutt_buffer_printf(buf2, "-CApath %s", mutt_b2s(buf1));

        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, mutt_b2s(buf2));

        mutt_buffer_pool_release(&path);
        mutt_buffer_pool_release(&buf1);
        mutt_buffer_pool_release(&buf2);
      }
      else if (!C_SmimeCaLocation)
        optional = false;
      break;
    }

    case 'c':
    { /* certificate (list) */
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(cctx->certificates));
      }
      else if (!cctx->certificates)
        optional = false;
      break;
    }

    case 'i':
    { /* intermediate certificates  */
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(cctx->intermediates));
      }
      else if (!cctx->intermediates)
        optional = false;
      break;
    }

    case 's':
    { /* detached signature */
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(cctx->sig_fname));
      }
      else if (!cctx->sig_fname)
        optional = false;
      break;
    }

    case 'k':
    { /* private key */
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(cctx->key));
      }
      else if (!cctx->key)
        optional = false;
      break;
    }

    case 'a':
    { /* algorithm for encryption */
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(cctx->cryptalg));
      }
      else if (!cctx->key)
        optional = false;
      break;
    }

    case 'f':
    { /* file to process */
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(cctx->fname));
      }
      else if (!cctx->fname)
        optional = false;
      break;
    }

    case 'd':
    { /* algorithm for the signature message digest */
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(cctx->digestalg));
      }
      else if (!cctx->key)
        optional = false;
      break;
    }

    default:
      *buf = '\0';
      break;
  }

  if (optional)
  {
    mutt_expando_format(buf, buflen, col, cols, if_str,
                        smime_command_format_str, data, MUTT_FORMAT_NO_FLAGS);
  }
  else if (flags & MUTT_FORMAT_OPTIONAL)
  {
    mutt_expando_format(buf, buflen, col, cols, else_str,
                        smime_command_format_str, data, MUTT_FORMAT_NO_FLAGS);
  }

  return src;
}

/**
 * smime_command - Format an SMIME command string
 * @param buf    Buffer for the result
 * @param buflen Length of buffer
 * @param cctx   Data to pass to the formatter
 * @param fmt    printf-like formatting string
 */
static void smime_command(char *buf, size_t buflen,
                          struct SmimeCommandContext *cctx, const char *fmt)
{
  mutt_expando_format(buf, buflen, 0, buflen, NONULL(fmt), smime_command_format_str,
                      (intptr_t) cctx, MUTT_FORMAT_NO_FLAGS);
  mutt_debug(LL_DEBUG2, "%s\n", buf);
}

/**
 * smime_invoke - Run an SMIME command
 * @param[out] fp_smime_in       stdin  for the command, or NULL (OPTIONAL)
 * @param[out] fp_smime_out      stdout for the command, or NULL (OPTIONAL)
 * @param[out] fp_smime_err      stderr for the command, or NULL (OPTIONAL)
 * @param[in]  fp_smime_infd     stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  fp_smime_outfd    stdout for the command, or -1 (OPTIONAL)
 * @param[in]  fp_smime_errfd    stderr for the command, or -1 (OPTIONAL)
 * @param[in]  fname         Filename to pass to the command
 * @param[in]  sig_fname     Signature filename to pass to the command
 * @param[in]  cryptalg      Encryption algorithm
 * @param[in]  digestalg     Hashing algorithm
 * @param[in]  key           SMIME key
 * @param[in]  certificates  Public certificates
 * @param[in]  intermediates Intermediate certificates
 * @param[in]  format        printf-like format string
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `fp_smime_in` has priority over `fp_smime_infd`.
 *       Likewise `fp_smime_out` and `fp_smime_err`.
 */
static pid_t smime_invoke(FILE **fp_smime_in, FILE **fp_smime_out, FILE **fp_smime_err,
                          int fp_smime_infd, int fp_smime_outfd, int fp_smime_errfd,
                          const char *fname, const char *sig_fname, const char *cryptalg,
                          const char *digestalg, const char *key, const char *certificates,
                          const char *intermediates, const char *format)
{
  struct SmimeCommandContext cctx = { 0 };
  char cmd[STR_COMMAND];

  if (!format || (*format == '\0'))
    return (pid_t) -1;

  cctx.fname = fname;
  cctx.sig_fname = sig_fname;
  cctx.key = key;
  cctx.cryptalg = cryptalg;
  cctx.digestalg = digestalg;
  cctx.certificates = certificates;
  cctx.intermediates = intermediates;

  smime_command(cmd, sizeof(cmd), &cctx, format);

  return filter_create_fd(cmd, fp_smime_in, fp_smime_out, fp_smime_err,
                          fp_smime_infd, fp_smime_outfd, fp_smime_errfd);
}

/*
 *    Key and certificate handling.
 */

/**
 * smime_key_flags - Turn SMIME key flags into a string
 * @param flags Flags, see #KeyFlags
 * @retval ptr Flag string
 *
 * @note The string is statically allocated
 */
static char *smime_key_flags(KeyFlags flags)
{
  static char buf[3];

  if (!(flags & KEYFLAG_CANENCRYPT))
    buf[0] = '-';
  else
    buf[0] = 'e';

  if (!(flags & KEYFLAG_CANSIGN))
    buf[1] = '-';
  else
    buf[1] = 's';

  buf[2] = '\0';

  return buf;
}

/**
 * smime_make_entry - Format a menu item for the smime key list - Implements Menu::make_entry()
 */
static void smime_make_entry(char *buf, size_t buflen, struct Menu *menu, int line)
{
  struct SmimeKey **table = menu->mdata;
  struct SmimeKey *key = table[line];
  char *truststate = NULL;
  switch (key->trust)
  {
    case 'e':
      /* L10N: Describes the trust state of a S/MIME key.
         This translation must be padded with spaces to the right such that it
         has the same length as the other translations.
         The translation strings which need to be padded are:
         Expired, Invalid, Revoked, Trusted, Unverified, Verified, and Unknown.  */
      truststate = _("Expired   ");
      break;
    case 'i':
      /* L10N: Describes the trust state of a S/MIME key.
         This translation must be padded with spaces to the right such that it
         has the same length as the other translations.
         The translation strings which need to be padded are:
         Expired, Invalid, Revoked, Trusted, Unverified, Verified, and Unknown.  */
      truststate = _("Invalid   ");
      break;
    case 'r':
      /* L10N: Describes the trust state of a S/MIME key.
         This translation must be padded with spaces to the right such that it
         has the same length as the other translations.
         The translation strings which need to be padded are:
         Expired, Invalid, Revoked, Trusted, Unverified, Verified, and Unknown.  */
      truststate = _("Revoked   ");
      break;
    case 't':
      /* L10N: Describes the trust state of a S/MIME key.
         This translation must be padded with spaces to the right such that it
         has the same length as the other translations.
         The translation strings which need to be padded are:
         Expired, Invalid, Revoked, Trusted, Unverified, Verified, and Unknown.  */
      truststate = _("Trusted   ");
      break;
    case 'u':
      /* L10N: Describes the trust state of a S/MIME key.
         This translation must be padded with spaces to the right such that it
         has the same length as the other translations.
         The translation strings which need to be padded are:
         Expired, Invalid, Revoked, Trusted, Unverified, Verified, and Unknown.  */
      truststate = _("Unverified");
      break;
    case 'v':
      /* L10N: Describes the trust state of a S/MIME key.
         This translation must be padded with spaces to the right such that it
         has the same length as the other translations.
         The translation strings which need to be padded are:
         Expired, Invalid, Revoked, Trusted, Unverified, Verified, and Unknown.  */
      truststate = _("Verified  ");
      break;
    default:
      /* L10N: Describes the trust state of a S/MIME key.
         This translation must be padded with spaces to the right such that it
         has the same length as the other translations.
         The translation strings which need to be padded are:
         Expired, Invalid, Revoked, Trusted, Unverified, Verified, and Unknown.  */
      truststate = _("Unknown   ");
  }
  snprintf(buf, buflen, " 0x%s %s %s %-35.35s %s", key->hash,
           smime_key_flags(key->flags), truststate, key->email, key->label);
}

/**
 * smime_select_key - Get the user to select a key
 * @param keys  List of keys to select from
 * @param query String to match
 * @retval ptr Key selected by user
 */
static struct SmimeKey *smime_select_key(struct SmimeKey *keys, char *query)
{
  struct SmimeKey **table = NULL;
  int table_size = 0;
  int table_index = 0;
  struct SmimeKey *key = NULL;
  struct SmimeKey *selected_key = NULL;
  char helpstr[1024];
  char buf[1024];
  char title[256];
  struct Menu *menu = NULL;
  const char *s = "";
  bool done = false;

  for (table_index = 0, key = keys; key; key = key->next)
  {
    if (table_index == table_size)
    {
      table_size += 5;
      mutt_mem_realloc(&table, sizeof(struct SmimeKey *) * table_size);
    }

    table[table_index++] = key;
  }

  snprintf(title, sizeof(title), _("S/MIME certificates matching \"%s\""), query);

  /* Make Helpstring */
  helpstr[0] = '\0';
  mutt_make_help(buf, sizeof(buf), _("Exit  "), MENU_SMIME, OP_EXIT);
  strcat(helpstr, buf);
  mutt_make_help(buf, sizeof(buf), _("Select  "), MENU_SMIME, OP_GENERIC_SELECT_ENTRY);
  strcat(helpstr, buf);
  mutt_make_help(buf, sizeof(buf), _("Help"), MENU_SMIME, OP_HELP);
  strcat(helpstr, buf);

  struct MuttWindow *dlg =
      mutt_window_new(WT_DLG_SMIME, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);

  struct MuttWindow *index =
      mutt_window_new(WT_INDEX, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);

  struct MuttWindow *ibar =
      mutt_window_new(WT_INDEX_BAR, MUTT_WIN_ORIENT_VERTICAL,
                      MUTT_WIN_SIZE_FIXED, MUTT_WIN_SIZE_UNLIMITED, 1);

  if (C_StatusOnTop)
  {
    mutt_window_add_child(dlg, ibar);
    mutt_window_add_child(dlg, index);
  }
  else
  {
    mutt_window_add_child(dlg, index);
    mutt_window_add_child(dlg, ibar);
  }

  dialog_push(dlg);

  /* Create the menu */
  menu = mutt_menu_new(MENU_SMIME);

  menu->pagelen = index->state.rows;
  menu->win_index = index;
  menu->win_ibar = ibar;

  menu->max = table_index;
  menu->make_entry = smime_make_entry;
  menu->help = helpstr;
  menu->mdata = table;
  menu->title = title;
  mutt_menu_push_current(menu);
  /* sorting keys might be done later - TODO */

  mutt_clear_error();

  done = false;
  while (!done)
  {
    switch (mutt_menu_loop(menu))
    {
      case OP_GENERIC_SELECT_ENTRY:
        if (table[menu->current]->trust != 't')
        {
          switch (table[menu->current]->trust)
          {
            case 'e':
            case 'i':
            case 'r':
              s = _("ID is expired/disabled/revoked. Do you really want to use "
                    "the key?");
              break;
            case 'u':
              s = _("ID has undefined validity. Do you really want to use the "
                    "key?");
              break;
            case 'v':
              s = _("ID is not trusted. Do you really want to use the key?");
              break;
          }

          snprintf(buf, sizeof(buf), "%s", s);

          if (mutt_yesorno(buf, MUTT_NO) != MUTT_YES)
          {
            mutt_clear_error();
            break;
          }
        }

        selected_key = table[menu->current];
        done = true;
        break;
      case OP_EXIT:
        done = true;
        break;
    }
  }

  mutt_menu_pop_current(menu);
  mutt_menu_free(&menu);
  FREE(&table);
  dialog_pop();
  mutt_window_free(&dlg);

  return selected_key;
}

/**
 * smime_parse_key - Parse an SMIME key block
 * @param buf String to parse
 * @retval ptr  SMIME key
 * @retval NULL Error
 */
static struct SmimeKey *smime_parse_key(char *buf)
{
  char *pend = NULL, *p = NULL;
  int field = 0;

  struct SmimeKey *key = mutt_mem_calloc(1, sizeof(struct SmimeKey));

  for (p = buf; p; p = pend)
  {
    /* Some users manually maintain their .index file, and use a tab
     * as a delimiter, which the old parsing code (using fscanf)
     * happened to allow.  smime_keys uses a space, so search for both.  */
    if ((pend = strchr(p, ' ')) || (pend = strchr(p, '\t')) || (pend = strchr(p, '\n')))
      *pend++ = 0;

    /* For backward compatibility, don't count consecutive delimiters
     * as an empty field.  */
    if (*p == '\0')
      continue;

    field++;

    switch (field)
    {
      case 1: /* mailbox */
        key->email = mutt_str_dup(p);
        break;
      case 2: /* hash */
        key->hash = mutt_str_dup(p);
        break;
      case 3: /* label */
        key->label = mutt_str_dup(p);
        break;
      case 4: /* issuer */
        key->issuer = mutt_str_dup(p);
        break;
      case 5: /* trust */
        key->trust = *p;
        break;
      case 6: /* purpose */
        while (*p)
        {
          switch (*p++)
          {
            case 'e':
              key->flags |= KEYFLAG_CANENCRYPT;
              break;

            case 's':
              key->flags |= KEYFLAG_CANSIGN;
              break;
          }
        }
        break;
    }
  }

  /* Old index files could be missing issuer, trust, and purpose,
   * but anything less than that is an error. */
  if (field < 3)
  {
    smime_key_free(&key);
    return NULL;
  }

  if (field < 4)
    key->issuer = mutt_str_dup("?");

  if (field < 5)
    key->trust = 't';

  if (field < 6)
    key->flags = (KEYFLAG_CANENCRYPT | KEYFLAG_CANSIGN);

  return key;
}

/**
 * smime_get_candidates - Find keys matching a string
 * @param search           String to match
 * @param only_public_key  If true, only get the public keys
 * @retval ptr Matching key
 */
static struct SmimeKey *smime_get_candidates(char *search, bool only_public_key)
{
  char buf[1024];
  struct SmimeKey *key = NULL, *results = NULL;
  struct SmimeKey **results_end = &results;

  struct Buffer *index_file = mutt_buffer_pool_get();
  mutt_buffer_printf(index_file, "%s/.index",
                     only_public_key ? NONULL(C_SmimeCertificates) : NONULL(C_SmimeKeys));

  FILE *fp = mutt_file_fopen(mutt_b2s(index_file), "r");
  if (!fp)
  {
    mutt_perror(mutt_b2s(index_file));
    mutt_buffer_pool_release(&index_file);
    return NULL;
  }
  mutt_buffer_pool_release(&index_file);

  while (fgets(buf, sizeof(buf), fp))
  {
    if (((*search == '\0')) || mutt_istr_find(buf, search))
    {
      key = smime_parse_key(buf);
      if (key)
      {
        *results_end = key;
        results_end = &key->next;
      }
    }
  }

  mutt_file_fclose(&fp);

  return results;
}

/**
 * smime_get_key_by_hash - Find a key by its hash
 * @param hash             Hash to find
 * @param only_public_key  If true, only get the public keys
 * @retval ptr Matching key
 *
 * Returns the first matching key record, without prompting or checking of
 * abilities or trust.
 */
static struct SmimeKey *smime_get_key_by_hash(char *hash, bool only_public_key)
{
  struct SmimeKey *match = NULL;
  struct SmimeKey *results = smime_get_candidates(hash, only_public_key);
  for (struct SmimeKey *result = results; result; result = result->next)
  {
    if (mutt_istr_equal(hash, result->hash))
    {
      match = smime_copy_key(result);
      break;
    }
  }

  smime_key_free(&results);

  return match;
}

/**
 * smime_get_key_by_addr - Find an SIME key by address
 * @param mailbox          Email address to match
 * @param abilities        Abilities to match, see #KeyFlags
 * @param only_public_key  If true, only get the public keys
 * @param oppenc_mode      If true, use opportunistic encryption
 * @retval ptr Matching key
 */
static struct SmimeKey *smime_get_key_by_addr(char *mailbox, KeyFlags abilities,
                                              bool only_public_key, bool oppenc_mode)
{
  if (!mailbox)
    return NULL;

  struct SmimeKey *results = NULL, *result = NULL;
  struct SmimeKey *matches = NULL;
  struct SmimeKey **matches_end = &matches;
  struct SmimeKey *match = NULL;
  struct SmimeKey *trusted_match = NULL;
  struct SmimeKey *valid_match = NULL;
  struct SmimeKey *return_key = NULL;
  bool multi_trusted_matches = false;

  results = smime_get_candidates(mailbox, only_public_key);
  for (result = results; result; result = result->next)
  {
    if (abilities && !(result->flags & abilities))
    {
      continue;
    }

    if (mutt_istr_equal(mailbox, result->email))
    {
      match = smime_copy_key(result);
      *matches_end = match;
      matches_end = &match->next;

      if (match->trust == 't')
      {
        if (trusted_match && !mutt_istr_equal(match->hash, trusted_match->hash))
        {
          multi_trusted_matches = true;
        }
        trusted_match = match;
      }
      else if ((match->trust == 'u') || (match->trust == 'v'))
      {
        valid_match = match;
      }
    }
  }

  smime_key_free(&results);

  if (matches)
  {
    if (oppenc_mode)
    {
      if (trusted_match)
        return_key = smime_copy_key(trusted_match);
      else if (valid_match && !C_CryptOpportunisticEncryptStrongKeys)
        return_key = smime_copy_key(valid_match);
      else
        return_key = NULL;
    }
    else if (trusted_match && !multi_trusted_matches)
    {
      return_key = smime_copy_key(trusted_match);
    }
    else
    {
      return_key = smime_copy_key(smime_select_key(matches, mailbox));
    }

    smime_key_free(&matches);
  }

  return return_key;
}

/**
 * smime_get_key_by_str - Find an SMIME key by string
 * @param str              String to match
 * @param abilities        Abilities to match, see #KeyFlags
 * @param only_public_key  If true, only get the public keys
 * @retval ptr Matching key
 */
static struct SmimeKey *smime_get_key_by_str(char *str, KeyFlags abilities, bool only_public_key)
{
  if (!str)
    return NULL;

  struct SmimeKey *results = NULL, *result = NULL;
  struct SmimeKey *matches = NULL;
  struct SmimeKey **matches_end = &matches;
  struct SmimeKey *match = NULL;
  struct SmimeKey *return_key = NULL;

  results = smime_get_candidates(str, only_public_key);
  for (result = results; result; result = result->next)
  {
    if (abilities && !(result->flags & abilities))
    {
      continue;
    }

    if (mutt_istr_equal(str, result->hash) ||
        mutt_istr_find(result->email, str) || mutt_istr_find(result->label, str))
    {
      match = smime_copy_key(result);
      *matches_end = match;
      matches_end = &match->next;
    }
  }

  smime_key_free(&results);

  if (matches)
  {
    return_key = smime_copy_key(smime_select_key(matches, str));
    smime_key_free(&matches);
  }

  return return_key;
}

/**
 * smime_ask_for_key - Ask the user to select a key
 * @param prompt           Prompt to show the user
 * @param abilities        Abilities to match, see #KeyFlags
 * @param only_public_key  If true, only get the public keys
 * @retval ptr Selected SMIME key
 */
static struct SmimeKey *smime_ask_for_key(char *prompt, KeyFlags abilities, bool only_public_key)
{
  struct SmimeKey *key = NULL;
  char resp[128];

  if (!prompt)
    prompt = _("Enter keyID: ");

  mutt_clear_error();

  while (true)
  {
    resp[0] = '\0';
    if (mutt_get_field(prompt, resp, sizeof(resp), MUTT_COMP_NO_FLAGS) != 0)
      return NULL;

    key = smime_get_key_by_str(resp, abilities, only_public_key);
    if (key)
      return key;

    mutt_error(_("No matching keys found for \"%s\""), resp);
  }
}

/**
 * getkeys - Get the keys for a mailbox
 * @param mailbox Email address
 *
 * This sets the '*ToUse' variables for an upcoming decryption, where the
 * required key is different from #C_SmimeDefaultKey.
 */
static void getkeys(char *mailbox)
{
  const char *k = NULL;

  struct SmimeKey *key = smime_get_key_by_addr(mailbox, KEYFLAG_CANENCRYPT, false, false);

  if (!key)
  {
    char buf[256];
    snprintf(buf, sizeof(buf), _("Enter keyID for %s: "), mailbox);
    key = smime_ask_for_key(buf, KEYFLAG_CANENCRYPT, false);
  }

  size_t smime_keys_len = mutt_str_len(C_SmimeKeys);

  k = key ? key->hash : NONULL(C_SmimeDefaultKey);

  /* if the key is different from last time */
  if ((mutt_buffer_len(&SmimeKeyToUse) <= smime_keys_len) ||
      !mutt_istr_equal(k, SmimeKeyToUse.data + smime_keys_len + 1))
  {
    smime_class_void_passphrase();
    mutt_buffer_printf(&SmimeKeyToUse, "%s/%s", NONULL(C_SmimeKeys), k);
    mutt_buffer_printf(&SmimeCertToUse, "%s/%s", NONULL(C_SmimeCertificates), k);
  }

  smime_key_free(&key);
}

/**
 * smime_class_getkeys - Implements CryptModuleSpecs::smime_getkeys()
 */
void smime_class_getkeys(struct Envelope *env)
{
  if (C_SmimeDecryptUseDefaultKey && C_SmimeDefaultKey)
  {
    mutt_buffer_printf(&SmimeKeyToUse, "%s/%s", NONULL(C_SmimeKeys), C_SmimeDefaultKey);
    mutt_buffer_printf(&SmimeCertToUse, "%s/%s", NONULL(C_SmimeCertificates), C_SmimeDefaultKey);
    return;
  }

  struct Address *a = NULL;
  TAILQ_FOREACH(a, &env->to, entries)
  {
    if (mutt_addr_is_user(a))
    {
      getkeys(a->mailbox);
      return;
    }
  }

  TAILQ_FOREACH(a, &env->cc, entries)
  {
    if (mutt_addr_is_user(a))
    {
      getkeys(a->mailbox);
      return;
    }
  }

  struct Address *f = mutt_default_from(NeoMutt->sub);
  getkeys(f->mailbox);
  mutt_addr_free(&f);
}

/**
 * smime_class_find_keys - Implements CryptModuleSpecs::find_keys()
 */
char *smime_class_find_keys(struct AddressList *al, bool oppenc_mode)
{
  struct SmimeKey *key = NULL;
  char *keyid = NULL, *keylist = NULL;
  size_t keylist_size = 0;
  size_t keylist_used = 0;

  struct Address *a = NULL;
  TAILQ_FOREACH(a, al, entries)
  {
    key = smime_get_key_by_addr(a->mailbox, KEYFLAG_CANENCRYPT, true, oppenc_mode);
    if (!key && !oppenc_mode)
    {
      char buf[1024];
      snprintf(buf, sizeof(buf), _("Enter keyID for %s: "), a->mailbox);
      key = smime_ask_for_key(buf, KEYFLAG_CANENCRYPT, true);
    }
    if (!key)
    {
      if (!oppenc_mode)
        mutt_message(_("No (valid) certificate found for %s"), a->mailbox);
      FREE(&keylist);
      return NULL;
    }

    keyid = key->hash;
    keylist_size += mutt_str_len(keyid) + 2;
    mutt_mem_realloc(&keylist, keylist_size);
    sprintf(keylist + keylist_used, "%s%s", keylist_used ? " " : "", keyid);
    keylist_used = mutt_str_len(keylist);

    smime_key_free(&key);
  }
  return keylist;
}

/**
 * smime_handle_cert_email - Process an email containing certificates
 * @param[in]  certificate Email with certificates
 * @param[in]  mailbox     Email address
 * @param[in]  copy        If true, save the certificates to buffer
 * @param[out] buffer      Buffer allocated to hold certificates
 * @param[out] num         Number of certificates in buffer
 * @retval  0 Success
 * @retval -1 Error
 * @retval -2 Error
 */
static int smime_handle_cert_email(char *certificate, char *mailbox, bool copy,
                                   char ***buffer, int *num)
{
  char email[256];
  int rc = -1, count = 0;
  pid_t pid;

  FILE *fp_err = mutt_file_mkstemp();
  if (!fp_err)
  {
    mutt_perror(_("Can't create temporary file"));
    return 1;
  }

  FILE *fp_out = mutt_file_mkstemp();
  if (!fp_out)
  {
    mutt_file_fclose(&fp_err);
    mutt_perror(_("Can't create temporary file"));
    return 1;
  }

  pid = smime_invoke(NULL, NULL, NULL, -1, fileno(fp_out), fileno(fp_err), certificate,
                     NULL, NULL, NULL, NULL, NULL, NULL, C_SmimeGetCertEmailCommand);
  if (pid == -1)
  {
    mutt_message(_("Error: unable to create OpenSSL subprocess"));
    mutt_file_fclose(&fp_err);
    mutt_file_fclose(&fp_out);
    return 1;
  }

  filter_wait(pid);

  fflush(fp_out);
  rewind(fp_out);
  fflush(fp_err);
  rewind(fp_err);

  while ((fgets(email, sizeof(email), fp_out)))
  {
    size_t len = mutt_str_len(email);
    if (len && (email[len - 1] == '\n'))
      email[len - 1] = '\0';
    if (mutt_istr_startswith(email, mailbox))
      rc = 1;

    rc = (rc < 0) ? 0 : rc;
    count++;
  }

  if (rc == -1)
  {
    mutt_endwin();
    mutt_file_copy_stream(fp_err, stdout);
    mutt_any_key_to_continue(_("Error: unable to create OpenSSL subprocess"));
    rc = 1;
  }
  else if (rc == 0)
    rc = 1;
  else
    rc = 0;

  if (copy && buffer && num)
  {
    (*num) = count;
    *buffer = mutt_mem_calloc(count, sizeof(char *));
    count = 0;

    rewind(fp_out);
    while ((fgets(email, sizeof(email), fp_out)))
    {
      size_t len = mutt_str_len(email);
      if (len && (email[len - 1] == '\n'))
        email[len - 1] = '\0';
      (*buffer)[count] = mutt_mem_calloc(mutt_str_len(email) + 1, sizeof(char));
      strncpy((*buffer)[count], email, mutt_str_len(email));
      count++;
    }
  }
  else if (copy)
    rc = 2;

  mutt_file_fclose(&fp_out);
  mutt_file_fclose(&fp_err);

  return rc;
}

/**
 * smime_extract_certificate - Extract an SMIME certificate from a file
 * @param infile File to read
 * @retval ptr Filename of temporary file containing certificate
 */
static char *smime_extract_certificate(const char *infile)
{
  FILE *fp_err = NULL;
  FILE *fp_out = NULL;
  FILE *fp_cert = NULL;
  char *retval = NULL;
  pid_t pid;
  int empty;

  struct Buffer *pk7out = mutt_buffer_pool_get();
  struct Buffer *certfile = mutt_buffer_pool_get();

  fp_err = mutt_file_mkstemp();
  if (!fp_err)
  {
    mutt_perror(_("Can't create temporary file"));
    goto cleanup;
  }

  mutt_buffer_mktemp(pk7out);
  fp_out = mutt_file_fopen(mutt_b2s(pk7out), "w+");
  if (!fp_out)
  {
    mutt_perror(mutt_b2s(pk7out));
    goto cleanup;
  }

  /* Step 1: Convert the signature to a PKCS#7 structure, as we can't
   * extract the full set of certificates directly. */
  pid = smime_invoke(NULL, NULL, NULL, -1, fileno(fp_out), fileno(fp_err), infile,
                     NULL, NULL, NULL, NULL, NULL, NULL, C_SmimePk7outCommand);
  if (pid == -1)
  {
    mutt_any_key_to_continue(_("Error: unable to create OpenSSL subprocess"));
    goto cleanup;
  }

  filter_wait(pid);

  fflush(fp_out);
  rewind(fp_out);
  fflush(fp_err);
  rewind(fp_err);
  empty = (fgetc(fp_out) == EOF);
  if (empty)
  {
    mutt_perror(mutt_b2s(pk7out));
    mutt_file_copy_stream(fp_err, stdout);
    goto cleanup;
  }
  mutt_file_fclose(&fp_out);

  mutt_buffer_mktemp(certfile);
  fp_cert = mutt_file_fopen(mutt_b2s(certfile), "w+");
  if (!fp_cert)
  {
    mutt_perror(mutt_b2s(certfile));
    mutt_file_unlink(mutt_b2s(pk7out));
    goto cleanup;
  }

  // Step 2: Extract the certificates from a PKCS#7 structure.
  pid = smime_invoke(NULL, NULL, NULL, -1, fileno(fp_cert), fileno(fp_err),
                     mutt_b2s(pk7out), NULL, NULL, NULL, NULL, NULL, NULL,
                     C_SmimeGetCertCommand);
  if (pid == -1)
  {
    mutt_any_key_to_continue(_("Error: unable to create OpenSSL subprocess"));
    mutt_file_unlink(mutt_b2s(pk7out));
    goto cleanup;
  }

  filter_wait(pid);

  mutt_file_unlink(mutt_b2s(pk7out));

  fflush(fp_cert);
  rewind(fp_cert);
  fflush(fp_err);
  rewind(fp_err);
  empty = (fgetc(fp_cert) == EOF);
  if (empty)
  {
    mutt_file_copy_stream(fp_err, stdout);
    goto cleanup;
  }

  mutt_file_fclose(&fp_cert);

  retval = mutt_buffer_strdup(certfile);

cleanup:
  mutt_file_fclose(&fp_err);
  if (fp_out)
  {
    mutt_file_fclose(&fp_out);
    mutt_file_unlink(mutt_b2s(pk7out));
  }
  if (fp_cert)
  {
    mutt_file_fclose(&fp_cert);
    mutt_file_unlink(mutt_b2s(certfile));
  }
  mutt_buffer_pool_release(&pk7out);
  mutt_buffer_pool_release(&certfile);
  return retval;
}

/**
 * smime_extract_signer_certificate - Extract the signer's certificate
 * @param infile File to read
 * @retval ptr Name of temporary file containing certificate
 */
static char *smime_extract_signer_certificate(const char *infile)
{
  char *cert = NULL;
  struct Buffer *certfile = NULL;
  pid_t pid;
  int empty;

  FILE *fp_err = mutt_file_mkstemp();
  if (!fp_err)
  {
    mutt_perror(_("Can't create temporary file"));
    return NULL;
  }

  certfile = mutt_buffer_pool_get();
  mutt_buffer_mktemp(certfile);
  FILE *fp_out = mutt_file_fopen(mutt_b2s(certfile), "w+");
  if (!fp_out)
  {
    mutt_file_fclose(&fp_err);
    mutt_perror(mutt_b2s(certfile));
    goto cleanup;
  }

  /* Extract signer's certificate
   */
  pid = smime_invoke(NULL, NULL, NULL, -1, -1, fileno(fp_err), infile, NULL, NULL, NULL,
                     NULL, mutt_b2s(certfile), NULL, C_SmimeGetSignerCertCommand);
  if (pid == -1)
  {
    mutt_any_key_to_continue(_("Error: unable to create OpenSSL subprocess"));
    goto cleanup;
  }

  filter_wait(pid);

  fflush(fp_out);
  rewind(fp_out);
  fflush(fp_err);
  rewind(fp_err);
  empty = (fgetc(fp_out) == EOF);
  if (empty)
  {
    mutt_endwin();
    mutt_file_copy_stream(fp_err, stdout);
    mutt_any_key_to_continue(NULL);
    goto cleanup;
  }

  mutt_file_fclose(&fp_out);
  cert = mutt_buffer_strdup(certfile);

cleanup:
  mutt_file_fclose(&fp_err);
  if (fp_out)
  {
    mutt_file_fclose(&fp_out);
    mutt_file_unlink(mutt_b2s(certfile));
  }
  mutt_buffer_pool_release(&certfile);
  return cert;
}

/**
 * smime_class_invoke_import - Implements CryptModuleSpecs::smime_invoke_import()
 */
void smime_class_invoke_import(const char *infile, const char *mailbox)
{
  char *certfile = NULL;
  char buf[256];
  FILE *fp_smime_in = NULL;

  FILE *fp_err = mutt_file_mkstemp();
  if (!fp_err)
  {
    mutt_perror(_("Can't create temporary file"));
    return;
  }

  FILE *fp_out = mutt_file_mkstemp();
  if (!fp_out)
  {
    mutt_file_fclose(&fp_err);
    mutt_perror(_("Can't create temporary file"));
    return;
  }

  buf[0] = '\0';
  if (C_SmimeAskCertLabel)
  {
    if ((mutt_get_field(_("Label for certificate: "), buf, sizeof(buf), MUTT_COMP_NO_FLAGS) != 0) ||
        (buf[0] == '\0'))
    {
      mutt_file_fclose(&fp_out);
      mutt_file_fclose(&fp_err);
      return;
    }
  }

  mutt_endwin();
  certfile = smime_extract_certificate(infile);
  if (certfile)
  {
    mutt_endwin();

    pid_t pid = smime_invoke(&fp_smime_in, NULL, NULL, -1, fileno(fp_out),
                             fileno(fp_err), certfile, NULL, NULL, NULL, NULL,
                             NULL, NULL, C_SmimeImportCertCommand);
    if (pid == -1)
    {
      mutt_message(_("Error: unable to create OpenSSL subprocess"));
      return;
    }
    fputs(buf, fp_smime_in);
    fputc('\n', fp_smime_in);
    mutt_file_fclose(&fp_smime_in);

    filter_wait(pid);

    mutt_file_unlink(certfile);
    FREE(&certfile);
  }

  fflush(fp_out);
  rewind(fp_out);
  fflush(fp_err);
  rewind(fp_err);

  mutt_file_copy_stream(fp_out, stdout);
  mutt_file_copy_stream(fp_err, stdout);

  mutt_file_fclose(&fp_out);
  mutt_file_fclose(&fp_err);
}

/**
 * smime_class_verify_sender - Implements CryptModuleSpecs::smime_verify_sender()
 */
int smime_class_verify_sender(struct Mailbox *m, struct Email *e)
{
  char *mbox = NULL, *certfile = NULL;
  int rc = 1;

  struct Buffer *tempfname = mutt_buffer_pool_get();
  mutt_buffer_mktemp(tempfname);
  FILE *fp_out = mutt_file_fopen(mutt_b2s(tempfname), "w");
  if (!fp_out)
  {
    mutt_perror(mutt_b2s(tempfname));
    goto cleanup;
  }

  if (e->security & SEC_ENCRYPT)
  {
    mutt_copy_message(fp_out, m, e, MUTT_CM_DECODE_CRYPT & MUTT_CM_DECODE_SMIME,
                      CH_MIME | CH_WEED | CH_NONEWLINE, 0);
  }
  else
    mutt_copy_message(fp_out, m, e, MUTT_CM_NO_FLAGS, CH_NO_FLAGS, 0);

  fflush(fp_out);
  mutt_file_fclose(&fp_out);

  if (!TAILQ_EMPTY(&e->env->from))
  {
    mutt_expand_aliases(&e->env->from);
    mbox = TAILQ_FIRST(&e->env->from)->mailbox;
  }
  else if (!TAILQ_EMPTY(&e->env->sender))
  {
    mutt_expand_aliases(&e->env->sender);
    mbox = TAILQ_FIRST(&e->env->sender)->mailbox;
  }

  if (mbox)
  {
    certfile = smime_extract_signer_certificate(mutt_b2s(tempfname));
    if (certfile)
    {
      mutt_file_unlink(mutt_b2s(tempfname));
      if (smime_handle_cert_email(certfile, mbox, false, NULL, NULL))
      {
        if (isendwin())
          mutt_any_key_to_continue(NULL);
      }
      else
        rc = 0;
      mutt_file_unlink(certfile);
      FREE(&certfile);
    }
    else
      mutt_any_key_to_continue(_("no certfile"));
  }
  else
    mutt_any_key_to_continue(_("no mbox"));

  mutt_file_unlink(mutt_b2s(tempfname));

cleanup:
  mutt_buffer_pool_release(&tempfname);
  return rc;
}

/* Creating S/MIME - bodies */

/**
 * smime_invoke_encrypt - Use SMIME to encrypt a file
 * @param[out] fp_smime_in    stdin  for the command, or NULL (OPTIONAL)
 * @param[out] fp_smime_out   stdout for the command, or NULL (OPTIONAL)
 * @param[out] fp_smime_err   stderr for the command, or NULL (OPTIONAL)
 * @param[in]  fp_smime_infd  stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  fp_smime_outfd stdout for the command, or -1 (OPTIONAL)
 * @param[in]  fp_smime_errfd stderr for the command, or -1 (OPTIONAL)
 * @param[in]  fname      Filename to pass to the command
 * @param[in]  uids       List of IDs/fingerprints, space separated
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `fp_smime_in` has priority over `fp_smime_infd`.
 *       Likewise `fp_smime_out` and `fp_smime_err`.
 */
static pid_t smime_invoke_encrypt(FILE **fp_smime_in, FILE **fp_smime_out,
                                  FILE **fp_smime_err, int fp_smime_infd,
                                  int fp_smime_outfd, int fp_smime_errfd,
                                  const char *fname, const char *uids)
{
  return smime_invoke(fp_smime_in, fp_smime_out, fp_smime_err, fp_smime_infd,
                      fp_smime_outfd, fp_smime_errfd, fname, NULL, C_SmimeEncryptWith,
                      NULL, NULL, uids, NULL, C_SmimeEncryptCommand);
}

/**
 * smime_invoke_sign - Use SMIME to sign a file
 * @param[out] fp_smime_in    stdin  for the command, or NULL (OPTIONAL)
 * @param[out] fp_smime_out   stdout for the command, or NULL (OPTIONAL)
 * @param[out] fp_smime_err   stderr for the command, or NULL (OPTIONAL)
 * @param[in]  fp_smime_infd  stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  fp_smime_outfd stdout for the command, or -1 (OPTIONAL)
 * @param[in]  fp_smime_errfd stderr for the command, or -1 (OPTIONAL)
 * @param[in]  fname      Filename to pass to the command
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `fp_smime_in` has priority over `fp_smime_infd`.
 *       Likewise `fp_smime_out` and `fp_smime_err`.
 */
static pid_t smime_invoke_sign(FILE **fp_smime_in, FILE **fp_smime_out,
                               FILE **fp_smime_err, int fp_smime_infd, int fp_smime_outfd,
                               int fp_smime_errfd, const char *fname)
{
  return smime_invoke(fp_smime_in, fp_smime_out, fp_smime_err, fp_smime_infd, fp_smime_outfd,
                      fp_smime_errfd, fname, NULL, NULL, C_SmimeSignDigestAlg,
                      mutt_b2s(&SmimeKeyToUse), mutt_b2s(&SmimeCertToUse),
                      mutt_b2s(&SmimeIntermediateToUse), C_SmimeSignCommand);
}

/**
 * smime_class_build_smime_entity - Implements CryptModuleSpecs::smime_build_smime_entity()
 */
struct Body *smime_class_build_smime_entity(struct Body *a, char *certlist)
{
  char buf[1024], certfile[PATH_MAX];
  char *cert_end = NULL;
  FILE *fp_smime_in = NULL, *fp_smime_err = NULL, *fp_out = NULL, *fp_tmp = NULL;
  struct Body *t = NULL;
  int err = 0, empty, off;
  pid_t pid;

  struct Buffer *tempfile = mutt_buffer_pool_get();
  struct Buffer *smime_infile = mutt_buffer_pool_get();

  mutt_buffer_mktemp(tempfile);
  fp_out = mutt_file_fopen(mutt_b2s(tempfile), "w+");
  if (!fp_out)
  {
    mutt_perror(mutt_b2s(tempfile));
    goto cleanup;
  }

  fp_smime_err = mutt_file_mkstemp();
  if (!fp_smime_err)
  {
    mutt_perror(_("Can't create temporary file"));
    goto cleanup;
  }

  mutt_buffer_mktemp(smime_infile);
  fp_tmp = mutt_file_fopen(mutt_b2s(smime_infile), "w+");
  if (!fp_tmp)
  {
    mutt_perror(mutt_b2s(smime_infile));
    goto cleanup;
  }

  *certfile = '\0';
  for (char *cert_start = certlist; cert_start; cert_start = cert_end)
  {
    cert_end = strchr(cert_start, ' ');
    if (cert_end)
      *cert_end = '\0';
    if (*cert_start)
    {
      off = mutt_str_len(certfile);
      snprintf(certfile + off, sizeof(certfile) - off, "%s%s/%s",
               (off != 0) ? " " : "", NONULL(C_SmimeCertificates), cert_start);
    }
    if (cert_end)
      *cert_end++ = ' ';
  }

  /* write a MIME entity */
  mutt_write_mime_header(a, fp_tmp, NeoMutt->sub);
  fputc('\n', fp_tmp);
  mutt_write_mime_body(a, fp_tmp, NeoMutt->sub);
  mutt_file_fclose(&fp_tmp);

  pid = smime_invoke_encrypt(&fp_smime_in, NULL, NULL, -1, fileno(fp_out),
                             fileno(fp_smime_err), mutt_b2s(smime_infile), certfile);
  if (pid == -1)
  {
    mutt_file_unlink(mutt_b2s(smime_infile));
    goto cleanup;
  }

  mutt_file_fclose(&fp_smime_in);

  filter_wait(pid);
  mutt_file_unlink(mutt_b2s(smime_infile));

  fflush(fp_out);
  rewind(fp_out);
  empty = (fgetc(fp_out) == EOF);
  mutt_file_fclose(&fp_out);

  fflush(fp_smime_err);
  rewind(fp_smime_err);
  while (fgets(buf, sizeof(buf) - 1, fp_smime_err))
  {
    err = 1;
    fputs(buf, stdout);
  }
  mutt_file_fclose(&fp_smime_err);

  /* pause if there is any error output from SMIME */
  if (err)
    mutt_any_key_to_continue(NULL);

  if (empty)
  {
    /* fatal error while trying to encrypt message */
    if (err == 0)
      mutt_any_key_to_continue(_("No output from OpenSSL..."));
    mutt_file_unlink(mutt_b2s(tempfile));
    goto cleanup;
  }

  t = mutt_body_new();
  t->type = TYPE_APPLICATION;
  t->subtype = mutt_str_dup("x-pkcs7-mime");
  mutt_param_set(&t->parameter, "name", "smime.p7m");
  mutt_param_set(&t->parameter, "smime-type", "enveloped-data");
  t->encoding = ENC_BASE64; /* The output of OpenSSL SHOULD be binary */
  t->use_disp = true;
  t->disposition = DISP_ATTACH;
  t->d_filename = mutt_str_dup("smime.p7m");
  t->filename = mutt_buffer_strdup(tempfile);
  t->unlink = true; /* delete after sending the message */
  t->parts = NULL;
  t->next = NULL;

cleanup:
  if (fp_out)
  {
    mutt_file_fclose(&fp_out);
    mutt_file_unlink(mutt_b2s(tempfile));
  }
  mutt_file_fclose(&fp_smime_err);
  if (fp_tmp)
  {
    mutt_file_fclose(&fp_tmp);
    mutt_file_unlink(mutt_b2s(smime_infile));
  }
  mutt_buffer_pool_release(&tempfile);
  mutt_buffer_pool_release(&smime_infile);

  return t;
}

/**
 * openssl_md_to_smime_micalg - Change the algorithm names
 * @param md OpenSSL message digest name
 * @retval ptr SMIME Message Integrity Check algorithm
 *
 * The openssl -md doesn't want hyphens:
 *   md5, sha1,  sha224,  sha256,  sha384,  sha512
 * However, the micalg does:
 *   md5, sha-1, sha-224, sha-256, sha-384, sha-512
 *
 * @note The caller should free the returned string
 */
static char *openssl_md_to_smime_micalg(char *md)
{
  if (!md)
    return 0;

  char *micalg = NULL;
  if (mutt_istr_startswith(md, "sha"))
  {
    const size_t l = strlen(md) + 2;
    micalg = mutt_mem_malloc(l);
    snprintf(micalg, l, "sha-%s", md + 3);
  }
  else
  {
    micalg = mutt_str_dup(md);
  }

  return micalg;
}

/**
 * smime_class_sign_message - Implements CryptModuleSpecs::sign_message()
 */
struct Body *smime_class_sign_message(struct Body *a, const struct AddressList *from)
{
  struct Body *t = NULL;
  struct Body *retval = NULL;
  char buf[1024];
  struct Buffer *filetosign = NULL, *signedfile = NULL;
  FILE *fp_smime_in = NULL, *fp_smime_out = NULL, *fp_smime_err = NULL, *fp_sign = NULL;
  int err = 0;
  int empty = 0;
  pid_t pid;
  char *intermediates = NULL;

  char *signas = C_SmimeSignAs ? C_SmimeSignAs : C_SmimeDefaultKey;
  if (!signas || (*signas == '\0'))
  {
    mutt_error(_("Can't sign: No key specified. Use Sign As."));
    return NULL;
  }

  crypt_convert_to_7bit(a); /* Signed data _must_ be in 7-bit format. */

  filetosign = mutt_buffer_pool_get();
  signedfile = mutt_buffer_pool_get();

  mutt_buffer_mktemp(filetosign);
  fp_sign = mutt_file_fopen(mutt_b2s(filetosign), "w+");
  if (!fp_sign)
  {
    mutt_perror(mutt_b2s(filetosign));
    goto cleanup;
  }

  mutt_buffer_mktemp(signedfile);
  fp_smime_out = mutt_file_fopen(mutt_b2s(signedfile), "w+");
  if (!fp_smime_out)
  {
    mutt_perror(mutt_b2s(signedfile));
    goto cleanup;
  }

  mutt_write_mime_header(a, fp_sign, NeoMutt->sub);
  fputc('\n', fp_sign);
  mutt_write_mime_body(a, fp_sign, NeoMutt->sub);
  mutt_file_fclose(&fp_sign);

  mutt_buffer_printf(&SmimeKeyToUse, "%s/%s", NONULL(C_SmimeKeys), signas);
  mutt_buffer_printf(&SmimeCertToUse, "%s/%s", NONULL(C_SmimeCertificates), signas);

  struct SmimeKey *signas_key = smime_get_key_by_hash(signas, 1);
  if (!signas_key || mutt_str_equal("?", signas_key->issuer))
    intermediates = signas; /* so openssl won't complain in any case */
  else
    intermediates = signas_key->issuer;

  mutt_buffer_printf(&SmimeIntermediateToUse, "%s/%s",
                     NONULL(C_SmimeCertificates), intermediates);

  smime_key_free(&signas_key);

  pid = smime_invoke_sign(&fp_smime_in, NULL, &fp_smime_err, -1,
                          fileno(fp_smime_out), -1, mutt_b2s(filetosign));
  if (pid == -1)
  {
    mutt_perror(_("Can't open OpenSSL subprocess"));
    mutt_file_unlink(mutt_b2s(filetosign));
    goto cleanup;
  }
  fputs(SmimePass, fp_smime_in);
  fputc('\n', fp_smime_in);
  mutt_file_fclose(&fp_smime_in);

  filter_wait(pid);

  /* check for errors from OpenSSL */
  err = 0;
  fflush(fp_smime_err);
  rewind(fp_smime_err);
  while (fgets(buf, sizeof(buf) - 1, fp_smime_err))
  {
    err = 1;
    fputs(buf, stdout);
  }
  mutt_file_fclose(&fp_smime_err);

  fflush(fp_smime_out);
  rewind(fp_smime_out);
  empty = (fgetc(fp_smime_out) == EOF);
  mutt_file_fclose(&fp_smime_out);

  mutt_file_unlink(mutt_b2s(filetosign));

  if (err)
    mutt_any_key_to_continue(NULL);

  if (empty)
  {
    mutt_any_key_to_continue(_("No output from OpenSSL..."));
    mutt_file_unlink(mutt_b2s(signedfile));
    goto cleanup; /* fatal error while signing */
  }

  t = mutt_body_new();
  t->type = TYPE_MULTIPART;
  t->subtype = mutt_str_dup("signed");
  t->encoding = ENC_7BIT;
  t->use_disp = false;
  t->disposition = DISP_INLINE;

  mutt_generate_boundary(&t->parameter);

  char *micalg = openssl_md_to_smime_micalg(C_SmimeSignDigestAlg);
  mutt_param_set(&t->parameter, "micalg", micalg);
  FREE(&micalg);

  mutt_param_set(&t->parameter, "protocol", "application/x-pkcs7-signature");

  t->parts = a;
  retval = t;

  t->parts->next = mutt_body_new();
  t = t->parts->next;
  t->type = TYPE_APPLICATION;
  t->subtype = mutt_str_dup("x-pkcs7-signature");
  t->filename = mutt_buffer_strdup(signedfile);
  t->d_filename = mutt_str_dup("smime.p7s");
  t->use_disp = true;
  t->disposition = DISP_ATTACH;
  t->encoding = ENC_BASE64;
  t->unlink = true; /* ok to remove this file after sending. */

cleanup:
  if (fp_sign)
  {
    mutt_file_fclose(&fp_sign);
    mutt_file_unlink(mutt_b2s(filetosign));
  }
  if (fp_smime_out)
  {
    mutt_file_fclose(&fp_smime_out);
    mutt_file_unlink(mutt_b2s(signedfile));
  }
  mutt_buffer_pool_release(&filetosign);
  mutt_buffer_pool_release(&signedfile);
  return retval;
}

/* Handling S/MIME - bodies */

/**
 * smime_invoke_verify - Use SMIME to verify a file
 * @param[out] fp_smime_in    stdin  for the command, or NULL (OPTIONAL)
 * @param[out] fp_smime_out   stdout for the command, or NULL (OPTIONAL)
 * @param[out] fp_smime_err   stderr for the command, or NULL (OPTIONAL)
 * @param[in]  fp_smime_infd  stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  fp_smime_outfd stdout for the command, or -1 (OPTIONAL)
 * @param[in]  fp_smime_errfd stderr for the command, or -1 (OPTIONAL)
 * @param[in]  fname      Filename to pass to the command
 * @param[in]  sig_fname  Signature filename to pass to the command
 * @param[in]  opaque     If true, use `$smime_verify_opaque_command` else `$smime_verify_command`
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `fp_smime_in` has priority over `fp_smime_infd`.
 *       Likewise `fp_smime_out` and `fp_smime_err`.
 */
static pid_t smime_invoke_verify(FILE **fp_smime_in, FILE **fp_smime_out,
                                 FILE **fp_smime_err, int fp_smime_infd,
                                 int fp_smime_outfd, int fp_smime_errfd,
                                 const char *fname, const char *sig_fname, int opaque)
{
  return smime_invoke(fp_smime_in, fp_smime_out, fp_smime_err, fp_smime_infd, fp_smime_outfd,
                      fp_smime_errfd, fname, sig_fname, NULL, NULL, NULL, NULL, NULL,
                      (opaque ? C_SmimeVerifyOpaqueCommand : C_SmimeVerifyCommand));
}

/**
 * smime_invoke_decrypt - Use SMIME to decrypt a file
 * @param[out] fp_smime_in    stdin  for the command, or NULL (OPTIONAL)
 * @param[out] fp_smime_out   stdout for the command, or NULL (OPTIONAL)
 * @param[out] fp_smime_err   stderr for the command, or NULL (OPTIONAL)
 * @param[in]  fp_smime_infd  stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  fp_smime_outfd stdout for the command, or -1 (OPTIONAL)
 * @param[in]  fp_smime_errfd stderr for the command, or -1 (OPTIONAL)
 * @param[in]  fname      Filename to pass to the command
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `fp_smime_in` has priority over `fp_smime_infd`.
 *       Likewise `fp_smime_out` and `fp_smime_err`.
 */
static pid_t smime_invoke_decrypt(FILE **fp_smime_in, FILE **fp_smime_out,
                                  FILE **fp_smime_err, int fp_smime_infd, int fp_smime_outfd,
                                  int fp_smime_errfd, const char *fname)
{
  return smime_invoke(fp_smime_in, fp_smime_out, fp_smime_err, fp_smime_infd,
                      fp_smime_outfd, fp_smime_errfd, fname, NULL, NULL, NULL,
                      mutt_b2s(&SmimeKeyToUse), mutt_b2s(&SmimeCertToUse), NULL,
                      C_SmimeDecryptCommand);
}

/**
 * smime_class_verify_one - Implements CryptModuleSpecs::verify_one()
 */
int smime_class_verify_one(struct Body *sigbdy, struct State *s, const char *tempfile)
{
  FILE *fp = NULL, *fp_smime_out = NULL, *fp_smime_err = NULL;
  pid_t pid;
  int badsig = -1;

  LOFF_T tmpoffset = 0;
  size_t tmplength = 0;
  int orig_type = sigbdy->type;

  struct Buffer *signedfile = mutt_buffer_pool_get();

  mutt_buffer_printf(signedfile, "%s.sig", tempfile);

  /* decode to a tempfile, saving the original destination */
  fp = s->fp_out;
  s->fp_out = mutt_file_fopen(mutt_b2s(signedfile), "w");
  if (!s->fp_out)
  {
    mutt_perror(mutt_b2s(signedfile));
    goto cleanup;
  }
  /* decoding the attachment changes the size and offset, so save a copy
   * of the "real" values now, and restore them after processing */
  tmplength = sigbdy->length;
  tmpoffset = sigbdy->offset;

  /* if we are decoding binary bodies, we don't want to prefix each
   * line with the prefix or else the data will get corrupted.  */
  char *save_prefix = s->prefix;
  s->prefix = NULL;

  mutt_decode_attachment(sigbdy, s);

  sigbdy->length = ftello(s->fp_out);
  sigbdy->offset = 0;
  mutt_file_fclose(&s->fp_out);

  /* restore final destination and substitute the tempfile for input */
  s->fp_out = fp;
  fp = s->fp_in;
  s->fp_in = fopen(mutt_b2s(signedfile), "r");

  /* restore the prefix */
  s->prefix = save_prefix;

  sigbdy->type = orig_type;

  fp_smime_err = mutt_file_mkstemp();
  if (!fp_smime_err)
  {
    mutt_perror(_("Can't create temporary file"));
    goto cleanup;
  }

  crypt_current_time(s, "OpenSSL");

  pid = smime_invoke_verify(NULL, &fp_smime_out, NULL, -1, -1, fileno(fp_smime_err),
                            tempfile, mutt_b2s(signedfile), 0);
  if (pid != -1)
  {
    fflush(fp_smime_out);
    mutt_file_fclose(&fp_smime_out);

    if (filter_wait(pid))
      badsig = -1;
    else
    {
      char *line = NULL;
      size_t linelen;

      fflush(fp_smime_err);
      rewind(fp_smime_err);

      line = mutt_file_read_line(line, &linelen, fp_smime_err, NULL, 0);
      if (linelen && mutt_istr_equal(line, "verification successful"))
        badsig = 0;

      FREE(&line);
    }
  }

  fflush(fp_smime_err);
  rewind(fp_smime_err);
  mutt_file_copy_stream(fp_smime_err, s->fp_out);
  mutt_file_fclose(&fp_smime_err);

  state_attach_puts(s, _("[-- End of OpenSSL output --]\n\n"));

  mutt_file_unlink(mutt_b2s(signedfile));

  sigbdy->length = tmplength;
  sigbdy->offset = tmpoffset;

  /* restore the original source stream */
  mutt_file_fclose(&s->fp_in);
  s->fp_in = fp;

cleanup:
  mutt_buffer_pool_release(&signedfile);
  return badsig;
}

/**
 * smime_handle_entity - Handle type application/pkcs7-mime
 * @param m           Body to handle
 * @param s           State to use
 * @param fp_out_file File for the result
 * @retval ptr Body for parsed MIME part
 *
 * This can either be a signed or an encrypted message.
 */
static struct Body *smime_handle_entity(struct Body *m, struct State *s, FILE *fp_out_file)
{
  struct Buffer tmpfname = mutt_buffer_make(0);
  FILE *fp_smime_out = NULL, *fp_smime_in = NULL, *fp_smime_err = NULL;
  FILE *fp_tmp = NULL, *fp_out = NULL;
  struct stat info;
  struct Body *p = NULL;
  pid_t pid = -1;
  SecurityFlags type = mutt_is_application_smime(m);

  if (!(type & APPLICATION_SMIME))
    return NULL;

  /* Because of the mutt_body_handler() we avoid the buffer pool. */
  fp_smime_out = mutt_file_mkstemp();
  if (!fp_smime_out)
  {
    mutt_perror(_("Can't create temporary file"));
    goto cleanup;
  }

  fp_smime_err = mutt_file_mkstemp();
  if (!fp_smime_err)
  {
    mutt_perror(_("Can't create temporary file"));
    goto cleanup;
  }

  mutt_buffer_mktemp(&tmpfname);
  fp_tmp = mutt_file_fopen(mutt_b2s(&tmpfname), "w+");
  if (!fp_tmp)
  {
    mutt_perror(mutt_b2s(&tmpfname));
    goto cleanup;
  }

  fseeko(s->fp_in, m->offset, SEEK_SET);

  mutt_file_copy_bytes(s->fp_in, fp_tmp, m->length);

  fflush(fp_tmp);
  mutt_file_fclose(&fp_tmp);

  if ((type & SEC_ENCRYPT) &&
      ((pid = smime_invoke_decrypt(&fp_smime_in, NULL, NULL, -1, fileno(fp_smime_out),
                                   fileno(fp_smime_err), mutt_b2s(&tmpfname))) == -1))
  {
    mutt_file_unlink(mutt_b2s(&tmpfname));
    if (s->flags & MUTT_DISPLAY)
    {
      state_attach_puts(
          s, _("[-- Error: unable to create OpenSSL subprocess --]\n"));
    }
    goto cleanup;
  }
  else if ((type & SEC_SIGNOPAQUE) &&
           ((pid = smime_invoke_verify(&fp_smime_in, NULL, NULL, -1,
                                       fileno(fp_smime_out), fileno(fp_smime_err), NULL,
                                       mutt_b2s(&tmpfname), SEC_SIGNOPAQUE)) == -1))
  {
    mutt_file_unlink(mutt_b2s(&tmpfname));
    if (s->flags & MUTT_DISPLAY)
    {
      state_attach_puts(
          s, _("[-- Error: unable to create OpenSSL subprocess --]\n"));
    }
    goto cleanup;
  }

  if (type & SEC_ENCRYPT)
  {
    if (!smime_class_valid_passphrase())
      smime_class_void_passphrase();
    fputs(SmimePass, fp_smime_in);
    fputc('\n', fp_smime_in);
  }

  mutt_file_fclose(&fp_smime_in);

  filter_wait(pid);
  mutt_file_unlink(mutt_b2s(&tmpfname));

  if (s->flags & MUTT_DISPLAY)
  {
    fflush(fp_smime_err);
    rewind(fp_smime_err);

    const int c = fgetc(fp_smime_err);
    if (c != EOF)
    {
      ungetc(c, fp_smime_err);

      crypt_current_time(s, "OpenSSL");
      mutt_file_copy_stream(fp_smime_err, s->fp_out);
      state_attach_puts(s, _("[-- End of OpenSSL output --]\n\n"));
    }

    if (type & SEC_ENCRYPT)
    {
      state_attach_puts(s,
                        _("[-- The following data is S/MIME encrypted --]\n"));
    }
    else
      state_attach_puts(s, _("[-- The following data is S/MIME signed --]\n"));
  }

  fflush(fp_smime_out);
  rewind(fp_smime_out);

  if (type & SEC_ENCRYPT)
  {
    /* void the passphrase, even if that wasn't the problem */
    if (fgetc(fp_smime_out) == EOF)
    {
      mutt_error(_("Decryption failed"));
      smime_class_void_passphrase();
    }
    rewind(fp_smime_out);
  }

  if (fp_out_file)
    fp_out = fp_out_file;
  else
  {
    fp_out = mutt_file_mkstemp();
    if (!fp_out)
    {
      mutt_perror(_("Can't create temporary file"));
      goto cleanup;
    }
  }
  char buf[8192];
  while (fgets(buf, sizeof(buf) - 1, fp_smime_out))
  {
    const size_t len = mutt_str_len(buf);
    if ((len > 1) && (buf[len - 2] == '\r'))
    {
      buf[len - 2] = '\n';
      buf[len - 1] = '\0';
    }
    fputs(buf, fp_out);
  }
  fflush(fp_out);
  rewind(fp_out);

  p = mutt_read_mime_header(fp_out, 0);
  if (p)
  {
    fstat(fileno(fp_out), &info);
    p->length = info.st_size - p->offset;

    mutt_parse_part(fp_out, p);

    if (s->flags & MUTT_DISPLAY)
      mutt_protected_headers_handler(p, s);

    /* Store any protected headers in the parent so they can be
     * accessed for index updates after the handler recursion is done.
     * This is done before the handler to prevent a nested encrypted
     * handler from freeing the headers. */
    mutt_env_free(&m->mime_headers);
    m->mime_headers = p->mime_headers;
    p->mime_headers = NULL;

    if (s->fp_out)
    {
      rewind(fp_out);
      FILE *fp_tmp_buffer = s->fp_in;
      s->fp_in = fp_out;
      mutt_body_handler(p, s);
      s->fp_in = fp_tmp_buffer;
    }

    /* Embedded multipart signed protected headers override the
     * encrypted headers.  We need to do this after the handler so
     * they can be printed in the pager. */
    if (!(type & SMIME_SIGN) && mutt_is_multipart_signed(p) && p->parts &&
        p->parts->mime_headers)
    {
      mutt_env_free(&m->mime_headers);
      m->mime_headers = p->parts->mime_headers;
      p->parts->mime_headers = NULL;
    }
  }
  mutt_file_fclose(&fp_smime_out);

  if (!fp_out_file)
  {
    mutt_file_fclose(&fp_out);
    mutt_file_unlink(mutt_b2s(&tmpfname));
  }
  fp_out = NULL;

  if (s->flags & MUTT_DISPLAY)
  {
    if (type & SEC_ENCRYPT)
      state_attach_puts(s, _("\n[-- End of S/MIME encrypted data. --]\n"));
    else
      state_attach_puts(s, _("\n[-- End of S/MIME signed data. --]\n"));
  }

  if (type & SEC_SIGNOPAQUE)
  {
    char *line = NULL;
    size_t linelen;

    rewind(fp_smime_err);

    line = mutt_file_read_line(line, &linelen, fp_smime_err, NULL, 0);
    if (linelen && mutt_istr_equal(line, "verification successful"))
      m->goodsig = true;
    FREE(&line);
  }
  else if (p)
  {
    m->goodsig = p->goodsig;
    m->badsig = p->badsig;
  }

cleanup:
  mutt_file_fclose(&fp_smime_out);
  mutt_file_fclose(&fp_smime_err);
  mutt_file_fclose(&fp_tmp);
  mutt_file_fclose(&fp_out);
  mutt_buffer_dealloc(&tmpfname);
  return p;
}

/**
 * smime_class_decrypt_mime - Implements CryptModuleSpecs::decrypt_mime()
 */
int smime_class_decrypt_mime(FILE *fp_in, FILE **fp_out, struct Body *b, struct Body **cur)
{
  struct State s = { 0 };
  LOFF_T tmpoffset = b->offset;
  size_t tmplength = b->length;
  int orig_type = b->type;
  int rc = -1;

  if (!mutt_is_application_smime(b))
    return -1;

  if (b->parts)
    return -1;

  s.fp_in = fp_in;
  fseeko(s.fp_in, b->offset, SEEK_SET);

  FILE *fp_tmp = mutt_file_mkstemp();
  if (!fp_tmp)
  {
    mutt_perror(_("Can't create temporary file"));
    return -1;
  }

  s.fp_out = fp_tmp;
  mutt_decode_attachment(b, &s);
  fflush(fp_tmp);
  b->length = ftello(s.fp_out);
  b->offset = 0;
  rewind(fp_tmp);
  s.fp_in = fp_tmp;
  s.fp_out = 0;

  *fp_out = mutt_file_mkstemp();
  if (!*fp_out)
  {
    mutt_perror(_("Can't create temporary file"));
    goto bail;
  }

  *cur = smime_handle_entity(b, &s, *fp_out);
  if (!*cur)
    goto bail;

  (*cur)->goodsig = b->goodsig;
  (*cur)->badsig = b->badsig;
  rc = 0;

bail:
  b->type = orig_type;
  b->length = tmplength;
  b->offset = tmpoffset;
  mutt_file_fclose(&fp_tmp);
  if (*fp_out)
    rewind(*fp_out);

  return rc;
}

/**
 * smime_class_application_handler - Implements CryptModuleSpecs::application_handler()
 */
int smime_class_application_handler(struct Body *m, struct State *s)
{
  int rc = -1;

  /* clear out any mime headers before the handler, so they can't be spoofed. */
  mutt_env_free(&m->mime_headers);

  struct Body *tattach = smime_handle_entity(m, s, NULL);
  if (tattach)
  {
    rc = 0;
    mutt_body_free(&tattach);
  }
  return rc;
}

/**
 * smime_class_send_menu - Implements CryptModuleSpecs::send_menu()
 */
int smime_class_send_menu(struct Email *e)
{
  struct SmimeKey *key = NULL;
  const char *prompt = NULL;
  const char *letters = NULL;
  const char *choices = NULL;
  int choice;

  if (!(WithCrypto & APPLICATION_SMIME))
    return e->security;

  e->security |= APPLICATION_SMIME;

  /* Opportunistic encrypt is controlling encryption.
   * NOTE: "Signing" and "Clearing" only adjust the sign bit, so we have different
   *       letter choices for those.  */
  if (C_CryptOpportunisticEncrypt && (e->security & SEC_OPPENCRYPT))
  {
    /* L10N: S/MIME options (opportunistic encryption is on) */
    prompt = _("S/MIME (s)ign, encrypt (w)ith, sign (a)s, (c)lear, or (o)ppenc "
               "mode off?");
    /* L10N: S/MIME options (opportunistic encryption is on) */
    letters = _("swaco");
    choices = "SwaCo";
  }
  /* Opportunistic encryption option is set, but is toggled off
   * for this message.  */
  else if (C_CryptOpportunisticEncrypt)
  {
    /* L10N: S/MIME options (opportunistic encryption is off) */
    prompt = _("S/MIME (e)ncrypt, (s)ign, encrypt (w)ith, sign (a)s, (b)oth, "
               "(c)lear, or (o)ppenc mode?");
    /* L10N: S/MIME options (opportunistic encryption is off) */
    letters = _("eswabco");
    choices = "eswabcO";
  }
  /* Opportunistic encryption is unset */
  else
  {
    /* L10N: S/MIME options */
    prompt = _("S/MIME (e)ncrypt, (s)ign, encrypt (w)ith, sign (a)s, (b)oth, "
               "or (c)lear?");
    /* L10N: S/MIME options */
    letters = _("eswabc");
    choices = "eswabc";
  }

  choice = mutt_multi_choice(prompt, letters);
  if (choice > 0)
  {
    switch (choices[choice - 1])
    {
      case 'a': /* sign (a)s */
        key = smime_ask_for_key(_("Sign as: "), KEYFLAG_CANSIGN, false);
        if (key)
        {
          mutt_str_replace(&C_SmimeSignAs, key->hash);
          smime_key_free(&key);

          e->security |= SEC_SIGN;

          /* probably need a different passphrase */
          crypt_smime_void_passphrase();
        }

        break;

      case 'b': /* (b)oth */
        e->security |= (SEC_ENCRYPT | SEC_SIGN);
        break;

      case 'c': /* (c)lear */
        e->security &= ~(SEC_ENCRYPT | SEC_SIGN);
        break;

      case 'C':
        e->security &= ~SEC_SIGN;
        break;

      case 'e': /* (e)ncrypt */
        e->security |= SEC_ENCRYPT;
        e->security &= ~SEC_SIGN;
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

      case 'w': /* encrypt (w)ith */
      {
        e->security |= SEC_ENCRYPT;
        do
        {
          struct Buffer errmsg = mutt_buffer_make(0);
          int rc = CSR_SUCCESS;
          switch (mutt_multi_choice(_("Choose algorithm family: (1) DES, (2) "
                                      "RC2, (3) AES, or (c)lear?"),
                                    // L10N: Options for: Choose algorithm family: (1) DES, (2) RC2, (3) AES, or (c)lear?
                                    _("123c")))
          {
            case 1:
              switch (choice = mutt_multi_choice(_("(1) DES, (2) Triple-DES?"),
                                                 // L10N: Options for: (1) DES, (2) Triple-DES
                                                 _("12")))
              {
                case 1:
                  rc = cs_subset_str_string_set(NeoMutt->sub, "smime_encrypt_with",
                                                "des", &errmsg);
                  break;
                case 2:
                  rc = cs_subset_str_string_set(NeoMutt->sub, "smime_encrypt_with",
                                                "des3", &errmsg);
                  break;
              }
              break;

            case 2:
              switch (choice = mutt_multi_choice(
                          _("(1) RC2-40, (2) RC2-64, (3) RC2-128?"),
                          // L10N: Options for: (1) RC2-40, (2) RC2-64, (3) RC2-128
                          _("123")))
              {
                case 1:
                  rc = cs_subset_str_string_set(NeoMutt->sub, "smime_encrypt_with",
                                                "rc2-40", &errmsg);
                  break;
                case 2:
                  rc = cs_subset_str_string_set(NeoMutt->sub, "smime_encrypt_with",
                                                "rc2-64", &errmsg);
                  break;
                case 3:
                  rc = cs_subset_str_string_set(NeoMutt->sub, "smime_encrypt_with",
                                                "rc2-128", &errmsg);
                  break;
              }
              break;

            case 3:
              switch (choice = mutt_multi_choice(
                          _("(1) AES128, (2) AES192, (3) AES256?"),
                          // L10N: Options for: (1) AES128, (2) AES192, (3) AES256
                          _("123")))
              {
                case 1:
                  rc = cs_subset_str_string_set(NeoMutt->sub, "smime_encrypt_with",
                                                "aes128", &errmsg);
                  break;
                case 2:
                  rc = cs_subset_str_string_set(NeoMutt->sub, "smime_encrypt_with",
                                                "aes192", &errmsg);
                  break;
                case 3:
                  rc = cs_subset_str_string_set(NeoMutt->sub, "smime_encrypt_with",
                                                "aes256", &errmsg);
                  break;
              }
              break;

            case 4:
              rc = cs_subset_str_string_set(NeoMutt->sub, "smime_encrypt_with",
                                            NULL, &errmsg);
            /* (c)lear */
            /* fallthrough */
            case -1: /* Ctrl-G or Enter */
              choice = 0;
              break;
          }

          if ((CSR_RESULT(rc) != CSR_SUCCESS) && !mutt_buffer_is_empty(&errmsg))
            mutt_error("%s", mutt_b2s(&errmsg));

          mutt_buffer_dealloc(&errmsg);
        } while (choice == -1);
        break;
      }
    }
  }

  return e->security;
}
