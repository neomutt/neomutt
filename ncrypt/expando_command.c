/**
 * @file
 * Ncrypt Expando definitions
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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
 * @page ncrypt_expando_command Ncrypt Expando definitions
 *
 * Ncrypt Expando definitions
 */

#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "expando_command.h"
#include "expando/lib.h"
#include "pgp.h"

/**
 * pgp_command_a - PGP Command: $pgp_sign_as or $pgp_default_key - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void pgp_command_a(const struct ExpandoNode *node, void *data,
                          MuttFormatFlags flags, struct Buffer *buf)
{
  const struct PgpCommandContext *cctx = data;

  const char *s = cctx->signas;
  buf_strcpy(buf, s);
}

/**
 * pgp_command_f - PGP Command: Filename of message - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void pgp_command_f(const struct ExpandoNode *node, void *data,
                          MuttFormatFlags flags, struct Buffer *buf)
{
  const struct PgpCommandContext *cctx = data;

  const char *s = cctx->fname;
  buf_strcpy(buf, s);
}

/**
 * pgp_command_p - PGP Command: PGPPASSFD=0 if passphrase is needed - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void pgp_command_p(const struct ExpandoNode *node, void *data,
                          MuttFormatFlags flags, struct Buffer *buf)
{
  const struct PgpCommandContext *cctx = data;

  const char *s = cctx->need_passphrase ? "PGPPASSFD=0" : "";
  buf_strcpy(buf, s);
}

/**
 * pgp_command_r - PGP Command: key IDs - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void pgp_command_r(const struct ExpandoNode *node, void *data,
                          MuttFormatFlags flags, struct Buffer *buf)
{
  const struct PgpCommandContext *cctx = data;

  const char *s = cctx->ids;
  buf_strcpy(buf, s);
}

/**
 * pgp_command_s - PGP Command: Filename of signature - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void pgp_command_s(const struct ExpandoNode *node, void *data,
                          MuttFormatFlags flags, struct Buffer *buf)
{
  const struct PgpCommandContext *cctx = data;

  const char *s = cctx->sig_fname;
  buf_strcpy(buf, s);
}

/**
 * PgpCommandRenderData - Callbacks for PGP Command Expandos
 *
 * @sa PgpCommandFormatDef, ExpandoDataPgpCmd
 */
const struct ExpandoRenderData PgpCommandRenderData[] = {
  // clang-format off
  { ED_PGP_CMD, ED_PGC_SIGN_AS,        pgp_command_a, NULL },
  { ED_PGP_CMD, ED_PGC_FILE_MESSAGE,   pgp_command_f, NULL },
  { ED_PGP_CMD, ED_PGC_NEED_PASS,      pgp_command_p, NULL },
  { ED_PGP_CMD, ED_PGC_KEY_IDS,        pgp_command_r, NULL },
  { ED_PGP_CMD, ED_PGC_FILE_SIGNATURE, pgp_command_s, NULL },
  { -1, -1, NULL, NULL },
  // clang-format on
};
