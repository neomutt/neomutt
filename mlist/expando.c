/**
 * @file
 * Mlist Expando definitions
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
 * @page mlist_expando Mlist Expando definitions
 *
 * Mlist Expando definitions
 */

#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "expando.h"
#include "expando/lib.h"

/**
 * mlist_url - Mlist: Mailing List URL - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void mlist_url(const struct ExpandoNode *node, void *data,
                      MuttFormatFlags flags, struct Buffer *buf)
{
  struct MlistExpandoData *med = data;

  buf_strcpy(buf, med->url);
}

/**
 * MlistRenderCallbacks - Callbacks for Mlist Expandos
 *
 * @sa MlistFormatDef, ExpandoDataMlist
 */
const struct ExpandoRenderCallback MlistRenderCallbacks[] = {
  // clang-format off
  { ED_MLIST, ED_MLS_URL, mlist_url, NULL },
  { -1, -1, NULL, NULL },
  // clang-format on
};
