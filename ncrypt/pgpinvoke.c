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

/* This file contains the new pgp invocation code.  Note that this
 * is almost entirely format based.
 */

#include "config.h"
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "lib/lib.h"
#include "address.h"
#include "filter.h"
#include "format_flags.h"
#include "globals.h"
#include "mutt_curses.h"
#include "mutt_idna.h"
#include "ncrypt.h"
#include "pgp.h"
#include "pgpkey.h"
#include "protos.h"
#include "rfc822.h"

/**
 * struct PgpCommandContext - Data for a PGP command
 *
 * The actual command line formatter.
 */
struct PgpCommandContext
{
  short need_passphrase; /**< %p */
  const char *fname;     /**< %f */
  const char *sig_fname; /**< %s */
  const char *signas;    /**< %a */
  const char *ids;       /**< %r */
};

static const char *_mutt_fmt_pgp_command(char *dest, size_t destlen, size_t col,
                                         int cols, char op, const char *src,
                                         const char *prefix, const char *ifstring,
                                         const char *elsestring,
                                         unsigned long data, enum FormatFlag flags)
{
  char fmt[16];
  struct PgpCommandContext *cctx = (struct PgpCommandContext *) data;
  int optional = (flags & MUTT_FORMAT_OPTIONAL);

  switch (op)
  {
    case 'r':
    {
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prefix);
        snprintf(dest, destlen, fmt, NONULL(cctx->ids));
      }
      else if (!cctx->ids)
        optional = 0;
      break;
    }

    case 'a':
    {
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prefix);
        snprintf(dest, destlen, fmt, NONULL(cctx->signas));
      }
      else if (!cctx->signas)
        optional = 0;
      break;
    }

    case 's':
    {
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prefix);
        snprintf(dest, destlen, fmt, NONULL(cctx->sig_fname));
      }
      else if (!cctx->sig_fname)
        optional = 0;
      break;
    }

    case 'f':
    {
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prefix);
        snprintf(dest, destlen, fmt, NONULL(cctx->fname));
      }
      else if (!cctx->fname)
        optional = 0;
      break;
    }

    case 'p':
    {
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prefix);
        snprintf(dest, destlen, fmt, cctx->need_passphrase ? "PGPPASSFD=0" : "");
      }
      else if (!cctx->need_passphrase || pgp_use_gpg_agent())
        optional = 0;
      break;
    }
    default:
    {
      *dest = '\0';
      break;
    }
  }

  if (optional)
    mutt_expando_format(dest, destlen, col, cols, ifstring, _mutt_fmt_pgp_command, data, 0);
  else if (flags & MUTT_FORMAT_OPTIONAL)
    mutt_expando_format(dest, destlen, col, cols, elsestring,
                        _mutt_fmt_pgp_command, data, 0);

  return src;
}

static void mutt_pgp_command(char *d, size_t dlen,
                             struct PgpCommandContext *cctx, const char *fmt)
{
  mutt_expando_format(d, dlen, 0, MuttIndexWindow->cols, NONULL(fmt),
                      _mutt_fmt_pgp_command, (unsigned long) cctx, 0);
  mutt_debug(2, "mutt_pgp_command: %s\n", d);
}

/*
 * Glue.
 */

static pid_t pgp_invoke(FILE **pgpin, FILE **pgpout, FILE **pgperr, int pgpinfd,
                        int pgpoutfd, int pgperrfd, short need_passphrase,
                        const char *fname, const char *sig_fname,
                        const char *signas, const char *ids, const char *format)
{
  struct PgpCommandContext cctx;
  char cmd[HUGE_STRING];

  memset(&cctx, 0, sizeof(cctx));

  if (!format || !*format)
    return (pid_t) -1;

  cctx.need_passphrase = need_passphrase;
  cctx.fname = fname;
  cctx.sig_fname = sig_fname;
  cctx.signas = signas;
  cctx.ids = ids;

  mutt_pgp_command(cmd, sizeof(cmd), &cctx, format);

  return mutt_create_filter_fd(cmd, pgpin, pgpout, pgperr, pgpinfd, pgpoutfd, pgperrfd);
}

/*
 * The exported interface.
 *
 * This is historic and may be removed at some point.
 */

pid_t pgp_invoke_decode(FILE **pgpin, FILE **pgpout, FILE **pgperr, int pgpinfd,
                        int pgpoutfd, int pgperrfd, const char *fname, short need_passphrase)
{
  return pgp_invoke(pgpin, pgpout, pgperr, pgpinfd, pgpoutfd, pgperrfd,
                    need_passphrase, fname, NULL, PgpSignAs, NULL, PgpDecodeCommand);
}

pid_t pgp_invoke_verify(FILE **pgpin, FILE **pgpout, FILE **pgperr, int pgpinfd,
                        int pgpoutfd, int pgperrfd, const char *fname, const char *sig_fname)
{
  return pgp_invoke(pgpin, pgpout, pgperr, pgpinfd, pgpoutfd, pgperrfd, 0,
                    fname, sig_fname, PgpSignAs, NULL, PgpVerifyCommand);
}

pid_t pgp_invoke_decrypt(FILE **pgpin, FILE **pgpout, FILE **pgperr, int pgpinfd,
                         int pgpoutfd, int pgperrfd, const char *fname)
{
  return pgp_invoke(pgpin, pgpout, pgperr, pgpinfd, pgpoutfd, pgperrfd, 1,
                    fname, NULL, PgpSignAs, NULL, PgpDecryptCommand);
}

pid_t pgp_invoke_sign(FILE **pgpin, FILE **pgpout, FILE **pgperr, int pgpinfd,
                      int pgpoutfd, int pgperrfd, const char *fname)
{
  return pgp_invoke(pgpin, pgpout, pgperr, pgpinfd, pgpoutfd, pgperrfd, 1,
                    fname, NULL, PgpSignAs, NULL, PgpSignCommand);
}

pid_t pgp_invoke_encrypt(FILE **pgpin, FILE **pgpout, FILE **pgperr,
                         int pgpinfd, int pgpoutfd, int pgperrfd,
                         const char *fname, const char *uids, int sign)
{
  if (sign)
    return pgp_invoke(pgpin, pgpout, pgperr, pgpinfd, pgpoutfd, pgperrfd, 1,
                      fname, NULL, PgpSignAs, uids, PgpEncryptSignCommand);
  else
    return pgp_invoke(pgpin, pgpout, pgperr, pgpinfd, pgpoutfd, pgperrfd, 0,
                      fname, NULL, PgpSignAs, uids, PgpEncryptOnlyCommand);
}

pid_t pgp_invoke_traditional(FILE **pgpin, FILE **pgpout, FILE **pgperr,
                             int pgpinfd, int pgpoutfd, int pgperrfd,
                             const char *fname, const char *uids, int flags)
{
  if (flags & ENCRYPT)
    return pgp_invoke(pgpin, pgpout, pgperr, pgpinfd, pgpoutfd, pgperrfd,
                      flags & SIGN ? 1 : 0, fname, NULL, PgpSignAs, uids,
                      flags & SIGN ? PgpEncryptSignCommand : PgpEncryptOnlyCommand);
  else
    return pgp_invoke(pgpin, pgpout, pgperr, pgpinfd, pgpoutfd, pgperrfd, 1,
                      fname, NULL, PgpSignAs, NULL, PgpClearSignCommand);
}

void pgp_invoke_import(const char *fname)
{
  char _fname[_POSIX_PATH_MAX + SHORT_STRING];
  char cmd[HUGE_STRING];
  struct PgpCommandContext cctx;

  memset(&cctx, 0, sizeof(cctx));

  mutt_quote_filename(_fname, sizeof(_fname), fname);
  cctx.fname = _fname;
  cctx.signas = PgpSignAs;

  mutt_pgp_command(cmd, sizeof(cmd), &cctx, PgpImportCommand);
  mutt_system(cmd);
}

void pgp_invoke_getkeys(struct Address *addr)
{
  char buff[LONG_STRING];
  char tmp[LONG_STRING];
  char cmd[HUGE_STRING];
  int devnull;

  char *personal = NULL;

  struct PgpCommandContext cctx;

  if (!PgpGetkeysCommand)
    return;

  memset(&cctx, 0, sizeof(cctx));

  personal = addr->personal;
  addr->personal = NULL;

  *tmp = '\0';
  mutt_addrlist_to_local(addr);
  rfc822_write_address_single(tmp, sizeof(tmp), addr, 0);
  mutt_quote_filename(buff, sizeof(buff), tmp);

  addr->personal = personal;

  cctx.ids = buff;

  mutt_pgp_command(cmd, sizeof(cmd), &cctx, PgpGetkeysCommand);

  devnull = open("/dev/null", O_RDWR);

  if (!isendwin())
    mutt_message(_("Fetching PGP key..."));

  mutt_system(cmd);

  if (!isendwin())
    mutt_clear_error();

  if (devnull >= 0)
    close(devnull);
}

pid_t pgp_invoke_export(FILE **pgpin, FILE **pgpout, FILE **pgperr, int pgpinfd,
                        int pgpoutfd, int pgperrfd, const char *uids)
{
  return pgp_invoke(pgpin, pgpout, pgperr, pgpinfd, pgpoutfd, pgperrfd, 0, NULL,
                    NULL, PgpSignAs, uids, PgpExportCommand);
}

pid_t pgp_invoke_verify_key(FILE **pgpin, FILE **pgpout, FILE **pgperr, int pgpinfd,
                            int pgpoutfd, int pgperrfd, const char *uids)
{
  return pgp_invoke(pgpin, pgpout, pgperr, pgpinfd, pgpoutfd, pgperrfd, 0, NULL,
                    NULL, PgpSignAs, uids, PgpVerifyKeyCommand);
}

pid_t pgp_invoke_list_keys(FILE **pgpin, FILE **pgpout, FILE **pgperr,
                           int pgpinfd, int pgpoutfd, int pgperrfd,
                           enum PgpRing keyring, struct ListHead *hints)
{
  char uids[HUGE_STRING];
  char tmpuids[HUGE_STRING];
  char quoted[HUGE_STRING];

  *uids = '\0';

  struct ListNode *np;
  STAILQ_FOREACH(np, hints, entries)
  {
    mutt_quote_filename(quoted, sizeof(quoted), (char *) np->data);
    snprintf(tmpuids, sizeof(tmpuids), "%s %s", uids, quoted);
    strcpy(uids, tmpuids);
  }

  return pgp_invoke(pgpin, pgpout, pgperr, pgpinfd, pgpoutfd, pgperrfd, 0, NULL,
                    NULL, PgpSignAs, uids,
                    keyring == PGP_SECRING ? PgpListSecringCommand : PgpListPubringCommand);
}
