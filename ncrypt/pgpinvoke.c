/**
 * @file
 * Wrapper around calls to external PGP program
 *
 * @authors
 * Copyright (C) 1997-2003 Thomas Roessler <roessler@does-not-exist.org>
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
 * @page crypt_pgpinvoke Wrapper around calls to external PGP program
 *
 * This file contains the new pgp invocation code.
 *
 * @note This is almost entirely format based.
 */

#include "config.h"
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "email/lib.h"
#include "filter.h"
#include "format_flags.h"
#include "globals.h"
#include "mutt_curses.h"
#include "mutt_logging.h"
#include "mutt_window.h"
#include "muttlib.h"
#include "ncrypt.h"
#include "pgpkey.h"
#include "protos.h"
#ifdef CRYPT_BACKEND_CLASSIC_PGP
#include "pgp.h"
#endif

/* These Config Variables are only used in ncrypt/pgpinvoke.c */
char *C_PgpClearsignCommand; ///< Config: (pgp) External command to inline-sign a message
char *C_PgpDecodeCommand; ///< Config: (pgp) External command to decode a PGP attachment
char *C_PgpDecryptCommand; ///< Config: (pgp) External command to decrypt a PGP message
char *C_PgpEncryptOnlyCommand; ///< Config: (pgp) External command to encrypt, but not sign a message
char *C_PgpEncryptSignCommand; ///< Config: (pgp) External command to encrypt and sign a message
char *C_PgpExportCommand; ///< Config: (pgp) External command to export a public key from the user's keyring
char *C_PgpGetkeysCommand; ///< Config: (pgp) External command to download a key for an email address
char *C_PgpImportCommand; ///< Config: (pgp) External command to import a key into the user's keyring
char *C_PgpListPubringCommand; ///< Config: (pgp) External command to list the public keys in a user's keyring
char *C_PgpListSecringCommand; ///< Config: (pgp) External command to list the private keys in a user's keyring
char *C_PgpSignCommand; ///< Config: (pgp) External command to create a detached PGP signature
char *C_PgpVerifyCommand; ///< Config: (pgp) External command to verify PGP signatures
char *C_PgpVerifyKeyCommand; ///< Config: (pgp) External command to verify key information

/**
 * struct PgpCommandContext - Data for a PGP command
 *
 * The actual command line formatter.
 */
struct PgpCommandContext
{
  bool need_passphrase;  /**< %p */
  const char *fname;     /**< %f */
  const char *sig_fname; /**< %s */
  const char *signas;    /**< %a */
  const char *ids;       /**< %r */
};

/**
 * fmt_pgp_command - Format a PGP command string - Implements ::format_t
 *
 * | Expando | Description
 * |:--------|:-----------------------------------------------------------------
 * | \%a     | Value of $pgp_sign_as if set, otherwise $pgp_default_key
 * | \%f     | File containing a message
 * | \%p     | Expands to PGPPASSFD=0 when a pass phrase is needed, to an empty string otherwise
 * | \%r     | One or more key IDs (or fingerprints if available)
 * | \%s     | File containing the signature part of a multipart/signed attachment when verifying it
 */
static const char *fmt_pgp_command(char *buf, size_t buflen, size_t col, int cols,
                                   char op, const char *src, const char *prec,
                                   const char *if_str, const char *else_str,
                                   unsigned long data, int flags)
{
  char fmt[128];
  struct PgpCommandContext *cctx = (struct PgpCommandContext *) data;
  int optional = (flags & MUTT_FORMAT_OPTIONAL);

  switch (op)
  {
    case 'a':
    {
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(cctx->signas));
      }
      else if (!cctx->signas)
        optional = 0;
      break;
    }
    case 'f':
    {
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(cctx->fname));
      }
      else if (!cctx->fname)
        optional = 0;
      break;
    }
    case 'p':
    {
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, cctx->need_passphrase ? "PGPPASSFD=0" : "");
      }
      else if (!cctx->need_passphrase || pgp_use_gpg_agent())
        optional = 0;
      break;
    }
    case 'r':
    {
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(cctx->ids));
      }
      else if (!cctx->ids)
        optional = 0;
      break;
    }
    case 's':
    {
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(cctx->sig_fname));
      }
      else if (!cctx->sig_fname)
        optional = 0;
      break;
    }
    default:
    {
      *buf = '\0';
      break;
    }
  }

  if (optional)
    mutt_expando_format(buf, buflen, col, cols, if_str, fmt_pgp_command, data, 0);
  else if (flags & MUTT_FORMAT_OPTIONAL)
    mutt_expando_format(buf, buflen, col, cols, else_str, fmt_pgp_command, data, 0);

  return src;
}

/**
 * mutt_pgp_command - Prepare a PGP Command
 * @param buf    Buffer for the result
 * @param buflen Length of buffer
 * @param cctx   Data to pass to the formatter
 * @param fmt    printf-like formatting string
 */
static void mutt_pgp_command(char *buf, size_t buflen,
                             struct PgpCommandContext *cctx, const char *fmt)
{
  mutt_expando_format(buf, buflen, 0, MuttIndexWindow->cols, NONULL(fmt),
                      fmt_pgp_command, (unsigned long) cctx, 0);
  mutt_debug(LL_DEBUG2, "%s\n", buf);
}

/**
 * pgp_invoke - Run a PGP command
 * @param[out] pgpin           stdin  for the command, or NULL (OPTIONAL)
 * @param[out] pgpout          stdout for the command, or NULL (OPTIONAL)
 * @param[out] pgperr          stderr for the command, or NULL (OPTIONAL)
 * @param[in]  pgpinfd         stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  pgpoutfd        stdout for the command, or -1 (OPTIONAL)
 * @param[in]  pgperrfd        stderr for the command, or -1 (OPTIONAL)
 * @param[in]  need_passphrase Is a passphrase needed?
 * @param[in]  fname           Filename to pass to the command
 * @param[in]  sig_fname       Signature filename to pass to the command
 * @param[in]  ids             List of IDs/fingerprints, space separated
 * @param[in]  format          printf-like format string
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `pgpin` has priority over `pgpinfd`.
 *       Likewise `pgpout` and `pgperr`.
 */
static pid_t pgp_invoke(FILE **pgpin, FILE **pgpout, FILE **pgperr, int pgpinfd, int pgpoutfd,
                        int pgperrfd, bool need_passphrase, const char *fname,
                        const char *sig_fname, const char *ids, const char *format)
{
  struct PgpCommandContext cctx = { 0 };
  char cmd[HUGE_STRING];

  if (!format || !*format)
    return (pid_t) -1;

  cctx.need_passphrase = need_passphrase;
  cctx.fname = fname;
  cctx.sig_fname = sig_fname;
  if (C_PgpSignAs && *C_PgpSignAs)
    cctx.signas = C_PgpSignAs;
  else
    cctx.signas = C_PgpDefaultKey;
  cctx.ids = ids;

  mutt_pgp_command(cmd, sizeof(cmd), &cctx, format);

  return mutt_create_filter_fd(cmd, pgpin, pgpout, pgperr, pgpinfd, pgpoutfd, pgperrfd);
}

/*
 * The exported interface.
 *
 * This is historic and may be removed at some point.
 */

/**
 * pgp_invoke_decode - Use PGP to decode a message
 * @param[out] pgpin           stdin  for the command, or NULL (OPTIONAL)
 * @param[out] pgpout          stdout for the command, or NULL (OPTIONAL)
 * @param[out] pgperr          stderr for the command, or NULL (OPTIONAL)
 * @param[in]  pgpinfd         stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  pgpoutfd        stdout for the command, or -1 (OPTIONAL)
 * @param[in]  pgperrfd        stderr for the command, or -1 (OPTIONAL)
 * @param[in]  fname           Filename to pass to the command
 * @param[in]  need_passphrase Is a passphrase needed?
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `pgpin` has priority over `pgpinfd`.
 *       Likewise `pgpout` and `pgperr`.
 */
pid_t pgp_invoke_decode(FILE **pgpin, FILE **pgpout, FILE **pgperr, int pgpinfd,
                        int pgpoutfd, int pgperrfd, const char *fname, bool need_passphrase)
{
  return pgp_invoke(pgpin, pgpout, pgperr, pgpinfd, pgpoutfd, pgperrfd,
                    need_passphrase, fname, NULL, NULL, C_PgpDecodeCommand);
}

/**
 * pgp_invoke_verify - Use PGP to verify a message
 * @param[out] pgpin     stdin  for the command, or NULL (OPTIONAL)
 * @param[out] pgpout    stdout for the command, or NULL (OPTIONAL)
 * @param[out] pgperr    stderr for the command, or NULL (OPTIONAL)
 * @param[in]  pgpinfd   stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  pgpoutfd  stdout for the command, or -1 (OPTIONAL)
 * @param[in]  pgperrfd  stderr for the command, or -1 (OPTIONAL)
 * @param[in]  fname     Filename to pass to the command
 * @param[in]  sig_fname Signature filename to pass to the command
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `pgpin` has priority over `pgpinfd`.
 *       Likewise `pgpout` and `pgperr`.
 */
pid_t pgp_invoke_verify(FILE **pgpin, FILE **pgpout, FILE **pgperr, int pgpinfd,
                        int pgpoutfd, int pgperrfd, const char *fname, const char *sig_fname)
{
  return pgp_invoke(pgpin, pgpout, pgperr, pgpinfd, pgpoutfd, pgperrfd, false,
                    fname, sig_fname, NULL, C_PgpVerifyCommand);
}

/**
 * pgp_invoke_decrypt - Use PGP to decrypt a file
 * @param[out] pgpin    stdin  for the command, or NULL (OPTIONAL)
 * @param[out] pgpout   stdout for the command, or NULL (OPTIONAL)
 * @param[out] pgperr   stderr for the command, or NULL (OPTIONAL)
 * @param[in]  pgpinfd  stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  pgpoutfd stdout for the command, or -1 (OPTIONAL)
 * @param[in]  pgperrfd stderr for the command, or -1 (OPTIONAL)
 * @param[in]  fname    Filename to pass to the command
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `pgpin` has priority over `pgpinfd`.
 *       Likewise `pgpout` and `pgperr`.
 */
pid_t pgp_invoke_decrypt(FILE **pgpin, FILE **pgpout, FILE **pgperr, int pgpinfd,
                         int pgpoutfd, int pgperrfd, const char *fname)
{
  return pgp_invoke(pgpin, pgpout, pgperr, pgpinfd, pgpoutfd, pgperrfd, true,
                    fname, NULL, NULL, C_PgpDecryptCommand);
}

/**
 * pgp_invoke_sign - Use PGP to sign a file
 * @param[out] pgpin    stdin  for the command, or NULL (OPTIONAL)
 * @param[out] pgpout   stdout for the command, or NULL (OPTIONAL)
 * @param[out] pgperr   stderr for the command, or NULL (OPTIONAL)
 * @param[in]  pgpinfd  stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  pgpoutfd stdout for the command, or -1 (OPTIONAL)
 * @param[in]  pgperrfd stderr for the command, or -1 (OPTIONAL)
 * @param[in]  fname    Filename to pass to the command
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `pgpin` has priority over `pgpinfd`.
 *       Likewise `pgpout` and `pgperr`.
 */
pid_t pgp_invoke_sign(FILE **pgpin, FILE **pgpout, FILE **pgperr, int pgpinfd,
                      int pgpoutfd, int pgperrfd, const char *fname)
{
  return pgp_invoke(pgpin, pgpout, pgperr, pgpinfd, pgpoutfd, pgperrfd, true,
                    fname, NULL, NULL, C_PgpSignCommand);
}

/**
 * pgp_invoke_encrypt - Use PGP to encrypt a file
 * @param[out] pgpin    stdin  for the command, or NULL (OPTIONAL)
 * @param[out] pgpout   stdout for the command, or NULL (OPTIONAL)
 * @param[out] pgperr   stderr for the command, or NULL (OPTIONAL)
 * @param[in]  pgpinfd  stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  pgpoutfd stdout for the command, or -1 (OPTIONAL)
 * @param[in]  pgperrfd stderr for the command, or -1 (OPTIONAL)
 * @param[in]  fname    Filename to pass to the command
 * @param[in]  uids     List of IDs/fingerprints, space separated
 * @param[in]  sign     If true, also sign the file
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `pgpin` has priority over `pgpinfd`.
 *       Likewise `pgpout` and `pgperr`.
 */
pid_t pgp_invoke_encrypt(FILE **pgpin, FILE **pgpout, FILE **pgperr,
                         int pgpinfd, int pgpoutfd, int pgperrfd,
                         const char *fname, const char *uids, bool sign)
{
  if (sign)
  {
    return pgp_invoke(pgpin, pgpout, pgperr, pgpinfd, pgpoutfd, pgperrfd, true,
                      fname, NULL, uids, C_PgpEncryptSignCommand);
  }
  else
  {
    return pgp_invoke(pgpin, pgpout, pgperr, pgpinfd, pgpoutfd, pgperrfd, false,
                      fname, NULL, uids, C_PgpEncryptOnlyCommand);
  }
}

/**
 * pgp_invoke_traditional - Use PGP to create in inline-signed message
 * @param[out] pgpin    stdin  for the command, or NULL (OPTIONAL)
 * @param[out] pgpout   stdout for the command, or NULL (OPTIONAL)
 * @param[out] pgperr   stderr for the command, or NULL (OPTIONAL)
 * @param[in]  pgpinfd  stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  pgpoutfd stdout for the command, or -1 (OPTIONAL)
 * @param[in]  pgperrfd stderr for the command, or -1 (OPTIONAL)
 * @param[in]  fname    Filename to pass to the command
 * @param[in]  uids     List of IDs/fingerprints, space separated
 * @param[in]  flags    Flags, e.g. #SEC_SIGN, #SEC_ENCRYPT
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `pgpin` has priority over `pgpinfd`.
 *       Likewise `pgpout` and `pgperr`.
 */
pid_t pgp_invoke_traditional(FILE **pgpin, FILE **pgpout, FILE **pgperr,
                             int pgpinfd, int pgpoutfd, int pgperrfd,
                             const char *fname, const char *uids, int flags)
{
  if (flags & SEC_ENCRYPT)
  {
    return pgp_invoke(pgpin, pgpout, pgperr, pgpinfd, pgpoutfd, pgperrfd,
                      (flags & SEC_SIGN), fname, NULL, uids,
                      (flags & SEC_SIGN) ? C_PgpEncryptSignCommand : C_PgpEncryptOnlyCommand);
  }
  else
  {
    return pgp_invoke(pgpin, pgpout, pgperr, pgpinfd, pgpoutfd, pgperrfd, true,
                      fname, NULL, NULL, C_PgpClearsignCommand);
  }
}

/**
 * pgp_class_invoke_import - Implements CryptModuleSpecs::pgp_invoke_import()
 */
void pgp_class_invoke_import(const char *fname)
{
  char tmp_fname[PATH_MAX + 128];
  char cmd[HUGE_STRING];
  struct PgpCommandContext cctx = { 0 };

  mutt_file_quote_filename(fname, tmp_fname, sizeof(tmp_fname));
  cctx.fname = tmp_fname;
  if (C_PgpSignAs && *C_PgpSignAs)
    cctx.signas = C_PgpSignAs;
  else
    cctx.signas = C_PgpDefaultKey;

  mutt_pgp_command(cmd, sizeof(cmd), &cctx, C_PgpImportCommand);
  if (mutt_system(cmd) != 0)
    mutt_debug(LL_DEBUG1, "Error running \"%s\"!", cmd);
}

/**
 * pgp_class_invoke_getkeys - Implements CryptModuleSpecs::pgp_invoke_getkeys()
 */
void pgp_class_invoke_getkeys(struct Address *addr)
{
  char buf[PATH_MAX];
  char tmp[LONG_STRING];
  char cmd[HUGE_STRING];
  int devnull;

  char *personal = NULL;

  struct PgpCommandContext cctx = { 0 };

  if (!C_PgpGetkeysCommand)
    return;

  personal = addr->personal;
  addr->personal = NULL;

  *tmp = '\0';
  mutt_addrlist_to_local(addr);
  mutt_addr_write_single(tmp, sizeof(tmp), addr, false);
  mutt_file_quote_filename(tmp, buf, sizeof(buf));

  addr->personal = personal;

  cctx.ids = buf;

  mutt_pgp_command(cmd, sizeof(cmd), &cctx, C_PgpGetkeysCommand);

  devnull = open("/dev/null", O_RDWR);

  if (!isendwin())
    mutt_message(_("Fetching PGP key..."));

  if (mutt_system(cmd) != 0)
    mutt_debug(LL_DEBUG1, "Error running \"%s\"!", cmd);

  if (!isendwin())
    mutt_clear_error();

  if (devnull >= 0)
    close(devnull);
}

/**
 * pgp_invoke_export - Use PGP to export a key from the user's keyring
 * @param[out] pgpin    stdin  for the command, or NULL (OPTIONAL)
 * @param[out] pgpout   stdout for the command, or NULL (OPTIONAL)
 * @param[out] pgperr   stderr for the command, or NULL (OPTIONAL)
 * @param[in]  pgpinfd  stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  pgpoutfd stdout for the command, or -1 (OPTIONAL)
 * @param[in]  pgperrfd stderr for the command, or -1 (OPTIONAL)
 * @param[in]  uids     List of IDs/fingerprints, space separated
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `pgpin` has priority over `pgpinfd`.
 *       Likewise `pgpout` and `pgperr`.
 */
pid_t pgp_invoke_export(FILE **pgpin, FILE **pgpout, FILE **pgperr, int pgpinfd,
                        int pgpoutfd, int pgperrfd, const char *uids)
{
  return pgp_invoke(pgpin, pgpout, pgperr, pgpinfd, pgpoutfd, pgperrfd, false,
                    NULL, NULL, uids, C_PgpExportCommand);
}

/**
 * pgp_invoke_verify_key - Use PGP to verify a key
 * @param[out] pgpin    stdin  for the command, or NULL (OPTIONAL)
 * @param[out] pgpout   stdout for the command, or NULL (OPTIONAL)
 * @param[out] pgperr   stderr for the command, or NULL (OPTIONAL)
 * @param[in]  pgpinfd  stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  pgpoutfd stdout for the command, or -1 (OPTIONAL)
 * @param[in]  pgperrfd stderr for the command, or -1 (OPTIONAL)
 * @param[in]  uids     List of IDs/fingerprints, space separated
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `pgpin` has priority over `pgpinfd`.
 *       Likewise `pgpout` and `pgperr`.
 */
pid_t pgp_invoke_verify_key(FILE **pgpin, FILE **pgpout, FILE **pgperr, int pgpinfd,
                            int pgpoutfd, int pgperrfd, const char *uids)
{
  return pgp_invoke(pgpin, pgpout, pgperr, pgpinfd, pgpoutfd, pgperrfd, false,
                    NULL, NULL, uids, C_PgpVerifyKeyCommand);
}

/**
 * pgp_invoke_list_keys - Find matching PGP Keys
 * @param[out] pgpin  stdin  for the command, or NULL (OPTIONAL)
 * @param[out] pgpout stdout for the command, or NULL (OPTIONAL)
 * @param[out] pgperr stderr for the command, or NULL (OPTIONAL)
 * @param[in]  pgpinfd  stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  pgpoutfd stdout for the command, or -1 (OPTIONAL)
 * @param[in]  pgperrfd stderr for the command, or -1 (OPTIONAL)
 * @param[in]  keyring  Keyring type, e.g. #PGP_SECRING
 * @param[in]  hints    Match keys to these strings
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `pgpin` has priority over `pgpinfd`.
 *       Likewise `pgpout` and `pgperr`.
 */
pid_t pgp_invoke_list_keys(FILE **pgpin, FILE **pgpout, FILE **pgperr,
                           int pgpinfd, int pgpoutfd, int pgperrfd,
                           enum PgpRing keyring, struct ListHead *hints)
{
  char quoted[HUGE_STRING];

  struct Buffer *uids = mutt_buffer_pool_get();

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, hints, entries)
  {
    mutt_file_quote_filename((char *) np->data, quoted, sizeof(quoted));
    mutt_buffer_addstr(uids, quoted);
    if (STAILQ_NEXT(np, entries))
      mutt_buffer_addch(uids, ' ');
  }

  pid_t rc = pgp_invoke(pgpin, pgpout, pgperr, pgpinfd, pgpoutfd, pgperrfd, 0,
                        NULL, NULL, mutt_b2s(uids),
                        keyring == PGP_SECRING ? C_PgpListSecringCommand : C_PgpListPubringCommand);

  mutt_buffer_pool_release(&uids);
  return rc;
}
