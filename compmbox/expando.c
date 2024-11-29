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
 * compress_from - Compress: From filename - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void compress_from(const struct ExpandoNode *node, void *data,
                          MuttFormatFlags flags, struct Buffer *buf)
{
  const struct Mailbox *m = data;

  struct Buffer *quoted = buf_pool_get();
  buf_quote_filename(quoted, m->realpath, false);
  buf_copy(buf, quoted);
  buf_pool_release(&quoted);
}

/**
 * compress_to - Compress: To filename - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void compress_to(const struct ExpandoNode *node, void *data,
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
  { ED_COMPRESS, ED_CMP_FROM, compress_from, NULL },
  { ED_COMPRESS, ED_CMP_TO,   compress_to,   NULL },
  { -1, -1, NULL, NULL },
  // clang-format on
};
