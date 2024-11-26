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
 * @page ncrypt_expando_smime Ncrypt Expando definitions
 *
 * Ncrypt Expando definitions
 */

#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "expando_smime.h"
#include "expando/lib.h"
#include "muttlib.h"
#include "smime.h"

/**
 * smime_command_algorithm - Smime Command: algorithm - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void smime_command_algorithm(const struct ExpandoNode *node, void *data,
                                    MuttFormatFlags flags, struct Buffer *buf)
{
  const struct SmimeCommandContext *cctx = data;

  const char *s = cctx->cryptalg;
  buf_strcpy(buf, s);
}

/**
 * smime_command_certificate_ids - Smime Command: certificate IDs - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void smime_command_certificate_ids(const struct ExpandoNode *node, void *data,
                                          MuttFormatFlags flags, struct Buffer *buf)
{
  const struct SmimeCommandContext *cctx = data;

  const char *s = cctx->certificates;
  buf_strcpy(buf, s);
}

/**
 * smime_command_certificate_path - Smime Command: CA location - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void smime_command_certificate_path(const struct ExpandoNode *node, void *data,
                                           MuttFormatFlags flags, struct Buffer *buf)
{
  const char *const c_smime_ca_location = cs_subset_path(NeoMutt->sub, "smime_ca_location");

  struct Buffer *path = buf_pool_get();
  struct Buffer *buf1 = buf_pool_get();
  struct Buffer *buf2 = buf_pool_get();
  struct stat st = { 0 };

  buf_strcpy(path, c_smime_ca_location);
  buf_expand_path(path);
  buf_quote_filename(buf1, buf_string(path), true);

  if ((stat(buf_string(path), &st) != 0) || !S_ISDIR(st.st_mode))
  {
    buf_printf(buf2, "-CAfile %s", buf_string(buf1));
  }
  else
  {
    buf_printf(buf2, "-CApath %s", buf_string(buf1));
  }

  buf_copy(buf, buf2);

  buf_pool_release(&path);
  buf_pool_release(&buf1);
  buf_pool_release(&buf2);
}

/**
 * smime_command_digest_algorithm - Smime Command: Message digest algorithm - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void smime_command_digest_algorithm(const struct ExpandoNode *node, void *data,
                                           MuttFormatFlags flags, struct Buffer *buf)
{
  const struct SmimeCommandContext *cctx = data;

  const char *s = cctx->digestalg;
  buf_strcpy(buf, s);
}

/**
 * smime_command_intermediate_ids - Smime Command: Intermediate certificates - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void smime_command_intermediate_ids(const struct ExpandoNode *node, void *data,
                                           MuttFormatFlags flags, struct Buffer *buf)
{
  const struct SmimeCommandContext *cctx = data;

  const char *s = cctx->intermediates;
  buf_strcpy(buf, s);
}

/**
 * smime_command_key - Smime Command: Key-pair - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void smime_command_key(const struct ExpandoNode *node, void *data,
                              MuttFormatFlags flags, struct Buffer *buf)
{
  const struct SmimeCommandContext *cctx = data;

  const char *s = cctx->key;
  buf_strcpy(buf, s);
}

/**
 * smime_command_message_file - Smime Command: Filename of message - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void smime_command_message_file(const struct ExpandoNode *node, void *data,
                                       MuttFormatFlags flags, struct Buffer *buf)
{
  const struct SmimeCommandContext *cctx = data;

  const char *s = cctx->fname;
  buf_strcpy(buf, s);
}

/**
 * smime_command_signature_file - Smime Command: Filename of signature - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void smime_command_signature_file(const struct ExpandoNode *node, void *data,
                                         MuttFormatFlags flags, struct Buffer *buf)
{
  const struct SmimeCommandContext *cctx = data;

  const char *s = cctx->sig_fname;
  buf_strcpy(buf, s);
}

/**
 * SmimeCommandRenderData - Callbacks for Smime Command Expandos
 *
 * @sa SmimeCommandFormatDef, ExpandoDataGlobal, ExpandoDataSmimeCmd
 */
const struct ExpandoRenderData SmimeCommandRenderData[] = {
  // clang-format off
  { ED_SMIME_CMD, ED_SMI_ALGORITHM,        smime_command_algorithm,        NULL },
  { ED_SMIME_CMD, ED_SMI_CERTIFICATE_IDS,  smime_command_certificate_ids,  NULL },
  { ED_SMIME_CMD, ED_SMI_CERTIFICATE_PATH, smime_command_certificate_path, NULL },
  { ED_SMIME_CMD, ED_SMI_DIGEST_ALGORITHM, smime_command_digest_algorithm, NULL },
  { ED_SMIME_CMD, ED_SMI_INTERMEDIATE_IDS, smime_command_intermediate_ids, NULL },
  { ED_SMIME_CMD, ED_SMI_KEY,              smime_command_key,              NULL },
  { ED_SMIME_CMD, ED_SMI_MESSAGE_FILE,     smime_command_message_file,     NULL },
  { ED_SMIME_CMD, ED_SMI_SIGNATURE_FILE,   smime_command_signature_file,   NULL },
  { -1, -1, NULL, NULL },
  // clang-format on
};
