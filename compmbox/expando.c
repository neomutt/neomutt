/**
 * @file
 * Compress Expando definitions
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
 * @page compmbox_expando Compress Expando definitions
 *
 * Compress Expando definitions
 */

#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "expando.h"
#include "lib.h"
#include "expando/lib.h"

/**
 * compress_f - Compress: From filename - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void compress_f(const struct ExpandoNode *node, void *data,
                       MuttFormatFlags flags, struct Buffer *buf)
{
  const struct Mailbox *m = data;

  struct Buffer *quoted = buf_pool_get();
  buf_quote_filename(quoted, m->realpath, false);
  buf_copy(buf, quoted);
  buf_pool_release(&quoted);
}

/**
 * compress_t - Compress: To filename - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void compress_t(const struct ExpandoNode *node, void *data,
                       MuttFormatFlags flags, struct Buffer *buf)
{
  const struct Mailbox *m = data;

  struct Buffer *quoted = buf_pool_get();
  buf_quote_filename(quoted, mailbox_path(m), false);
  buf_copy(buf, quoted);
  buf_pool_release(&quoted);
}

/**
 * CompressRenderData - Callbacks for Compression Hook Expandos
 *
 * @sa CompressFormatDef, ExpandoDataCompress
 */
const struct ExpandoRenderData CompressRenderData[] = {
  // clang-format off
  { ED_COMPRESS, ED_CMP_FROM, compress_f, NULL },
  { ED_COMPRESS, ED_CMP_TO,   compress_t, NULL },
  { -1, -1, NULL, NULL },
  // clang-format on
};
