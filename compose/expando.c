/**
 * @file
 * Alias Expando definitions
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
 * @page compose_expando Alias Expando definitions
 *
 * Alias Expando definitions
 */

#include <stdbool.h>
#include <stdio.h>
#include "private.h"
#include "mutt/lib.h"
#include "expando.h"
#include "expando/lib.h"
#include "menu/lib.h"
#include "attach_data.h"
#include "globals.h"
#include "muttlib.h"
#include "shared_data.h"

/**
 * num_attachments - Count the number of attachments
 * @param adata Attachment data
 * @retval num Number of attachments
 */
static int num_attachments(const struct ComposeAttachData *adata)
{
  if (!adata || !adata->menu)
    return 0;
  return adata->menu->max;
}

/**
 * compose_a_num - Compose: Number of attachments - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long compose_a_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct ComposeSharedData *shared = data;

  return num_attachments(shared->adata);
}

/**
 * compose_h - Compose: Hostname - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void compose_h(const struct ExpandoNode *node, void *data,
                      MuttFormatFlags flags, struct Buffer *buf)
{
  const char *s = ShortHostname;
  buf_strcpy(buf, s);
}

/**
 * compose_l_num - Compose: Size in bytes - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long compose_l_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct ComposeSharedData *shared = data;
  return cum_attachs_size(shared->sub, shared->adata);
}

/**
 * compose_l - Compose: Size in bytes - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void compose_l(const struct ExpandoNode *node, void *data,
                      MuttFormatFlags flags, struct Buffer *buf)
{
  const struct ComposeSharedData *shared = data;

  char tmp[128] = { 0 };

  mutt_str_pretty_size(tmp, sizeof(tmp), cum_attachs_size(shared->sub, shared->adata));
  buf_strcpy(buf, tmp);
}

/**
 * compose_v - Compose: Version - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void compose_v(const struct ExpandoNode *node, void *data,
                      MuttFormatFlags flags, struct Buffer *buf)
{
  const char *s = mutt_make_version();
  buf_strcpy(buf, s);
}

/**
 * ComposeRenderData - Callbacks for Compose Expandos
 *
 * @sa ComposeFormatDef, ExpandoDataCompose, ExpandoDataGlobal
 */
const struct ExpandoRenderData ComposeRenderData[] = {
  // clang-format off
  { ED_COMPOSE, ED_COM_ATTACH_COUNT, NULL,      compose_a_num },
  { ED_GLOBAL,  ED_GLO_HOSTNAME,     compose_h, NULL },
  { ED_COMPOSE, ED_COM_ATTACH_SIZE,  compose_l, compose_l_num },
  { ED_GLOBAL,  ED_GLO_VERSION,      compose_v, NULL },
  { -1, -1, NULL, NULL },
  // clang-format on
};
