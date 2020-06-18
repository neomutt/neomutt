/**
 * @file
 * Wrapper around calls to external PGP program
 *
 * @authors
 * Copyright (C) 1997-2003 Thomas Roessler <roessler@does-not-exist.org>
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
 * @page crypt_pgpinvoke Wrapper around calls to external PGP program
 *
 * This file contains the new pgp invocation code.
 *
 * @note This is almost entirely format based.
 */

#include "config.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "gui/lib.h"
#include "pgpinvoke.h"
#include "lib.h"
#include "format_flags.h"
#include "mutt_globals.h"
#include "mutt_logging.h"
#include "muttlib.h"
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
  bool need_passphrase;  ///< %p
  const char *fname;     ///< %f
  const char *sig_fname; ///< %s
  const char *signas;    ///< %a
  const char *ids;       ///< %r
};

/**
 * pgp_command_format_str - Format a PGP command string - Implements ::format_t
 *
 * | Expando | Description
 * |:--------|:-----------------------------------------------------------------
 * | \%a     | Value of `$pgp_sign_as` if set, otherwise `$pgp_default_key`
 * | \%f     | File containing a message
 * | \%p     | Expands to PGPPASSFD=0 when a pass phrase is needed, to an empty string otherwise
 * | \%r     | One or more key IDs (or fingerprints if available)
 * | \%s     | File containing the signature part of a multipart/signed attachment when verifying it
 */
static const char *pgp_command_format_str(char *buf, size_t buflen, size_t col, int cols,
                                          char op, const char *src, const char *prec,
                                          const char *if_str, const char *else_str,
                                          intptr_t data, MuttFormatFlags flags)
{
  char fmt[128];
  struct PgpCommandContext *cctx = (struct PgpCommandContext *) data;
  bool optional = (flags & MUTT_FORMAT_OPTIONAL);

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
        optional = false;
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
        optional = false;
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
        optional = false;
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
        optional = false;
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
        optional = false;
      break;
    }
    default:
    {
      *buf = '\0';
      break;
    }
  }

  if (optional)
  {
    mutt_expando_format(buf, buflen, col, cols, if_str, pgp_command_format_str,
                        data, MUTT_FORMAT_NO_FLAGS);
  }
  else if (flags & MUTT_FORMAT_OPTIONAL)
  {
    mutt_expando_format(buf, buflen, col, cols, else_str,
                        pgp_command_format_str, data, MUTT_FORMAT_NO_FLAGS);
  }

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
  mutt_expando_format(buf, buflen, 0, buflen, NONULL(fmt), pgp_command_format_str,
                      (intptr_t) cctx, MUTT_FORMAT_NO_FLAGS);
  mutt_debug(LL_DEBUG2, "%s\n", buf);
}

/**
 * pgp_invoke - Run a PGP command
 * @param[out] fp_pgp_in       stdin  for the command, or NULL (OPTIONAL)
 * @param[out] fp_pgp_out      stdout for the command, or NULL (OPTIONAL)
 * @param[out] fp_pgp_err      stderr for the command, or NULL (OPTIONAL)
 * @param[in]  fd_pgp_in       stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  fd_pgp_out      stdout for the command, or -1 (OPTIONAL)
 * @param[in]  fd_pgp_err      stderr for the command, or -1 (OPTIONAL)
 * @param[in]  need_passphrase Is a passphrase needed?
 * @param[in]  fname           Filename to pass to the command
 * @param[in]  sig_fname       Signature filename to pass to the command
 * @param[in]  ids             List of IDs/fingerprints, space separated
 * @param[in]  format          printf-like format string
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `fp_pgp_in` has priority over `fd_pgp_in`.
 *       Likewise `fp_pgp_out` and `fp_pgp_err`.
 */
static pid_t pgp_invoke(FILE **fp_pgp_in, FILE **fp_pgp_out, FILE **fp_pgp_err,
                        int fd_pgp_in, int fd_pgp_out, int fd_pgp_err,
                        bool need_passphrase, const char *fname,
                        const char *sig_fname, const char *ids, const char *format)
{
  struct PgpCommandContext cctx = { 0 };
  char cmd[STR_COMMAND];

  if (!format || (*format == '\0'))
    return (pid_t) -1;

  cctx.need_passphrase = need_passphrase;
  cctx.fname = fname;
  cctx.sig_fname = sig_fname;
  if (C_PgpSignAs)
    cctx.signas = C_PgpSignAs;
  else
    cctx.signas = C_PgpDefaultKey;
  cctx.ids = ids;

  mutt_pgp_command(cmd, sizeof(cmd), &cctx, format);

  return filter_create_fd(cmd, fp_pgp_in, fp_pgp_out, fp_pgp_err, fd_pgp_in,
                          fd_pgp_out, fd_pgp_err);
}

/*
 * The exported interface.
 *
 * This is historic and may be removed at some point.
 */

/**
 * pgp_invoke_decode - Use PGP to decode a message
 * @param[out] fp_pgp_in       stdin  for the command, or NULL (OPTIONAL)
 * @param[out] fp_pgp_out      stdout for the command, or NULL (OPTIONAL)
 * @param[out] fp_pgp_err      stderr for the command, or NULL (OPTIONAL)
 * @param[in]  fd_pgp_in       stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  fd_pgp_out      stdout for the command, or -1 (OPTIONAL)
 * @param[in]  fd_pgp_err      stderr for the command, or -1 (OPTIONAL)
 * @param[in]  fname           Filename to pass to the command
 * @param[in]  need_passphrase Is a passphrase needed?
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `fp_pgp_in` has priority over `fd_pgp_in`.
 *       Likewise `fp_pgp_out` and `fp_pgp_err`.
 */
pid_t pgp_invoke_decode(FILE **fp_pgp_in, FILE **fp_pgp_out, FILE **fp_pgp_err,
                        int fd_pgp_in, int fd_pgp_out, int fd_pgp_err,
                        const char *fname, bool need_passphrase)
{
  return pgp_invoke(fp_pgp_in, fp_pgp_out, fp_pgp_err, fd_pgp_in, fd_pgp_out, fd_pgp_err,
                    need_passphrase, fname, NULL, NULL, C_PgpDecodeCommand);
}

/**
 * pgp_invoke_verify - Use PGP to verify a message
 * @param[out] fp_pgp_in  stdin  for the command, or NULL (OPTIONAL)
 * @param[out] fp_pgp_out stdout for the command, or NULL (OPTIONAL)
 * @param[out] fp_pgp_err stderr for the command, or NULL (OPTIONAL)
 * @param[in]  fd_pgp_in  stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  fd_pgp_out stdout for the command, or -1 (OPTIONAL)
 * @param[in]  fd_pgp_err stderr for the command, or -1 (OPTIONAL)
 * @param[in]  fname      Filename to pass to the command
 * @param[in]  sig_fname  Signature filename to pass to the command
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `fp_pgp_in` has priority over `fd_pgp_in`.
 *       Likewise `fp_pgp_out` and `fp_pgp_err`.
 */
pid_t pgp_invoke_verify(FILE **fp_pgp_in, FILE **fp_pgp_out, FILE **fp_pgp_err,
                        int fd_pgp_in, int fd_pgp_out, int fd_pgp_err,
                        const char *fname, const char *sig_fname)
{
  return pgp_invoke(fp_pgp_in, fp_pgp_out, fp_pgp_err, fd_pgp_in, fd_pgp_out,
                    fd_pgp_err, false, fname, sig_fname, NULL, C_PgpVerifyCommand);
}

/**
 * pgp_invoke_decrypt - Use PGP to decrypt a file
 * @param[out] fp_pgp_in  stdin  for the command, or NULL (OPTIONAL)
 * @param[out] fp_pgp_out stdout for the command, or NULL (OPTIONAL)
 * @param[out] fp_pgp_err stderr for the command, or NULL (OPTIONAL)
 * @param[in]  fd_pgp_in  stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  fd_pgp_out stdout for the command, or -1 (OPTIONAL)
 * @param[in]  fd_pgp_err stderr for the command, or -1 (OPTIONAL)
 * @param[in]  fname      Filename to pass to the command
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `fp_pgp_in` has priority over `fd_pgp_in`.
 *       Likewise `fp_pgp_out` and `fp_pgp_err`.
 */
pid_t pgp_invoke_decrypt(FILE **fp_pgp_in, FILE **fp_pgp_out, FILE **fp_pgp_err,
                         int fd_pgp_in, int fd_pgp_out, int fd_pgp_err, const char *fname)
{
  return pgp_invoke(fp_pgp_in, fp_pgp_out, fp_pgp_err, fd_pgp_in, fd_pgp_out,
                    fd_pgp_err, true, fname, NULL, NULL, C_PgpDecryptCommand);
}

/**
 * pgp_invoke_sign - Use PGP to sign a file
 * @param[out] fp_pgp_in  stdin  for the command, or NULL (OPTIONAL)
 * @param[out] fp_pgp_out stdout for the command, or NULL (OPTIONAL)
 * @param[out] fp_pgp_err stderr for the command, or NULL (OPTIONAL)
 * @param[in]  fd_pgp_in  stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  fd_pgp_out stdout for the command, or -1 (OPTIONAL)
 * @param[in]  fd_pgp_err stderr for the command, or -1 (OPTIONAL)
 * @param[in]  fname      Filename to pass to the command
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `fp_pgp_in` has priority over `fd_pgp_in`.
 *       Likewise `fp_pgp_out` and `fp_pgp_err`.
 */
pid_t pgp_invoke_sign(FILE **fp_pgp_in, FILE **fp_pgp_out, FILE **fp_pgp_err,
                      int fd_pgp_in, int fd_pgp_out, int fd_pgp_err, const char *fname)
{
  return pgp_invoke(fp_pgp_in, fp_pgp_out, fp_pgp_err, fd_pgp_in, fd_pgp_out,
                    fd_pgp_err, true, fname, NULL, NULL, C_PgpSignCommand);
}

/**
 * pgp_invoke_encrypt - Use PGP to encrypt a file
 * @param[out] fp_pgp_in  stdin  for the command, or NULL (OPTIONAL)
 * @param[out] fp_pgp_out stdout for the command, or NULL (OPTIONAL)
 * @param[out] fp_pgp_err stderr for the command, or NULL (OPTIONAL)
 * @param[in]  fd_pgp_in  stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  fd_pgp_out stdout for the command, or -1 (OPTIONAL)
 * @param[in]  fd_pgp_err stderr for the command, or -1 (OPTIONAL)
 * @param[in]  fname      Filename to pass to the command
 * @param[in]  uids       List of IDs/fingerprints, space separated
 * @param[in]  sign       If true, also sign the file
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `fp_pgp_in` has priority over `fd_pgp_in`.
 *       Likewise `fp_pgp_out` and `fp_pgp_err`.
 */
pid_t pgp_invoke_encrypt(FILE **fp_pgp_in, FILE **fp_pgp_out, FILE **fp_pgp_err,
                         int fd_pgp_in, int fd_pgp_out, int fd_pgp_err,
                         const char *fname, const char *uids, bool sign)
{
  if (sign)
  {
    return pgp_invoke(fp_pgp_in, fp_pgp_out, fp_pgp_err, fd_pgp_in, fd_pgp_out,
                      fd_pgp_err, true, fname, NULL, uids, C_PgpEncryptSignCommand);
  }
  else
  {
    return pgp_invoke(fp_pgp_in, fp_pgp_out, fp_pgp_err, fd_pgp_in, fd_pgp_out,
                      fd_pgp_err, false, fname, NULL, uids, C_PgpEncryptOnlyCommand);
  }
}

/**
 * pgp_invoke_traditional - Use PGP to create in inline-signed message
 * @param[out] fp_pgp_in  stdin  for the command, or NULL (OPTIONAL)
 * @param[out] fp_pgp_out stdout for the command, or NULL (OPTIONAL)
 * @param[out] fp_pgp_err stderr for the command, or NULL (OPTIONAL)
 * @param[in]  fd_pgp_in  stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  fd_pgp_out stdout for the command, or -1 (OPTIONAL)
 * @param[in]  fd_pgp_err stderr for the command, or -1 (OPTIONAL)
 * @param[in]  fname      Filename to pass to the command
 * @param[in]  uids       List of IDs/fingerprints, space separated
 * @param[in]  flags      Flags, see #SecurityFlags
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `fp_pgp_in` has priority over `fd_pgp_in`.
 *       Likewise `fp_pgp_out` and `fp_pgp_err`.
 */
pid_t pgp_invoke_traditional(FILE **fp_pgp_in, FILE **fp_pgp_out, FILE **fp_pgp_err,
                             int fd_pgp_in, int fd_pgp_out, int fd_pgp_err,
                             const char *fname, const char *uids, SecurityFlags flags)
{
  if (flags & SEC_ENCRYPT)
  {
    return pgp_invoke(fp_pgp_in, fp_pgp_out, fp_pgp_err, fd_pgp_in, fd_pgp_out,
                      fd_pgp_err, (flags & SEC_SIGN), fname, NULL, uids,
                      (flags & SEC_SIGN) ? C_PgpEncryptSignCommand : C_PgpEncryptOnlyCommand);
  }
  else
  {
    return pgp_invoke(fp_pgp_in, fp_pgp_out, fp_pgp_err, fd_pgp_in, fd_pgp_out,
                      fd_pgp_err, true, fname, NULL, NULL, C_PgpClearsignCommand);
  }
}

/**
 * pgp_class_invoke_import - Implements CryptModuleSpecs::pgp_invoke_import()
 */
void pgp_class_invoke_import(const char *fname)
{
  char cmd[STR_COMMAND];
  struct PgpCommandContext cctx = { 0 };

  struct Buffer *buf_fname = mutt_buffer_pool_get();

  mutt_buffer_quote_filename(buf_fname, fname, true);
  cctx.fname = mutt_b2s(buf_fname);
  if (C_PgpSignAs)
    cctx.signas = C_PgpSignAs;
  else
    cctx.signas = C_PgpDefaultKey;

  mutt_pgp_command(cmd, sizeof(cmd), &cctx, C_PgpImportCommand);
  if (mutt_system(cmd) != 0)
    mutt_debug(LL_DEBUG1, "Error running \"%s\"", cmd);

  mutt_buffer_pool_release(&buf_fname);
}

/**
 * pgp_class_invoke_getkeys - Implements CryptModuleSpecs::pgp_invoke_getkeys()
 */
void pgp_class_invoke_getkeys(struct Address *addr)
{
  char tmp[1024];
  char cmd[STR_COMMAND];

  char *personal = NULL;

  struct PgpCommandContext cctx = { 0 };

  if (!C_PgpGetkeysCommand)
    return;

  struct Buffer *buf = mutt_buffer_pool_get();
  personal = addr->personal;
  addr->personal = NULL;

  *tmp = '\0';
  mutt_addr_to_local(addr);
  mutt_addr_write(tmp, sizeof(tmp), addr, false);
  mutt_buffer_quote_filename(buf, tmp, true);

  addr->personal = personal;

  cctx.ids = mutt_b2s(buf);

  mutt_pgp_command(cmd, sizeof(cmd), &cctx, C_PgpGetkeysCommand);

  int fd_null = open("/dev/null", O_RDWR);

  if (!isendwin())
    mutt_message(_("Fetching PGP key..."));

  if (mutt_system(cmd) != 0)
    mutt_debug(LL_DEBUG1, "Error running \"%s\"", cmd);

  if (!isendwin())
    mutt_clear_error();

  if (fd_null >= 0)
    close(fd_null);

  mutt_buffer_pool_release(&buf);
}

/**
 * pgp_invoke_export - Use PGP to export a key from the user's keyring
 * @param[out] fp_pgp_in  stdin  for the command, or NULL (OPTIONAL)
 * @param[out] fp_pgp_out stdout for the command, or NULL (OPTIONAL)
 * @param[out] fp_pgp_err stderr for the command, or NULL (OPTIONAL)
 * @param[in]  fd_pgp_in  stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  fd_pgp_out stdout for the command, or -1 (OPTIONAL)
 * @param[in]  fd_pgp_err stderr for the command, or -1 (OPTIONAL)
 * @param[in]  uids       List of IDs/fingerprints, space separated
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `fp_pgp_in` has priority over `fd_pgp_in`.
 *       Likewise `fp_pgp_out` and `fp_pgp_err`.
 */
pid_t pgp_invoke_export(FILE **fp_pgp_in, FILE **fp_pgp_out, FILE **fp_pgp_err,
                        int fd_pgp_in, int fd_pgp_out, int fd_pgp_err, const char *uids)
{
  return pgp_invoke(fp_pgp_in, fp_pgp_out, fp_pgp_err, fd_pgp_in, fd_pgp_out,
                    fd_pgp_err, false, NULL, NULL, uids, C_PgpExportCommand);
}

/**
 * pgp_invoke_verify_key - Use PGP to verify a key
 * @param[out] fp_pgp_in  stdin  for the command, or NULL (OPTIONAL)
 * @param[out] fp_pgp_out stdout for the command, or NULL (OPTIONAL)
 * @param[out] fp_pgp_err stderr for the command, or NULL (OPTIONAL)
 * @param[in]  fd_pgp_in  stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  fd_pgp_out stdout for the command, or -1 (OPTIONAL)
 * @param[in]  fd_pgp_err stderr for the command, or -1 (OPTIONAL)
 * @param[in]  uids       List of IDs/fingerprints, space separated
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `fp_pgp_in` has priority over `fd_pgp_in`.
 *       Likewise `fp_pgp_out` and `fp_pgp_err`.
 */
pid_t pgp_invoke_verify_key(FILE **fp_pgp_in, FILE **fp_pgp_out, FILE **fp_pgp_err,
                            int fd_pgp_in, int fd_pgp_out, int fd_pgp_err, const char *uids)
{
  return pgp_invoke(fp_pgp_in, fp_pgp_out, fp_pgp_err, fd_pgp_in, fd_pgp_out,
                    fd_pgp_err, false, NULL, NULL, uids, C_PgpVerifyKeyCommand);
}

/**
 * pgp_invoke_list_keys - Find matching PGP Keys
 * @param[out] fp_pgp_in  stdin  for the command, or NULL (OPTIONAL)
 * @param[out] fp_pgp_out stdout for the command, or NULL (OPTIONAL)
 * @param[out] fp_pgp_err stderr for the command, or NULL (OPTIONAL)
 * @param[in]  fd_pgp_in  stdin  for the command, or -1 (OPTIONAL)
 * @param[in]  fd_pgp_out stdout for the command, or -1 (OPTIONAL)
 * @param[in]  fd_pgp_err stderr for the command, or -1 (OPTIONAL)
 * @param[in]  keyring    Keyring type, e.g. #PGP_SECRING
 * @param[in]  hints      Match keys to these strings
 * @retval num PID of the created process
 * @retval -1  Error creating pipes or forking
 *
 * @note `fp_pgp_in` has priority over `fd_pgp_in`.
 *       Likewise `fp_pgp_out` and `fp_pgp_err`.
 */
pid_t pgp_invoke_list_keys(FILE **fp_pgp_in, FILE **fp_pgp_out, FILE **fp_pgp_err,
                           int fd_pgp_in, int fd_pgp_out, int fd_pgp_err,
                           enum PgpRing keyring, struct ListHead *hints)
{
  struct Buffer *uids = mutt_buffer_pool_get();
  struct Buffer *quoted = mutt_buffer_pool_get();

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, hints, entries)
  {
    mutt_buffer_quote_filename(quoted, (char *) np->data, true);
    mutt_buffer_addstr(uids, mutt_b2s(quoted));
    if (STAILQ_NEXT(np, entries))
      mutt_buffer_addch(uids, ' ');
  }

  pid_t rc = pgp_invoke(fp_pgp_in, fp_pgp_out, fp_pgp_err, fd_pgp_in,
                        fd_pgp_out, fd_pgp_err, 0, NULL, NULL, mutt_b2s(uids),
                        (keyring == PGP_SECRING) ? C_PgpListSecringCommand :
                                                   C_PgpListPubringCommand);

  mutt_buffer_pool_release(&uids);
  mutt_buffer_pool_release(&quoted);
  return rc;
}
