/**
 * @file
 * Compose Attach Data
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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
 * @page compose_attach_data Attachment Data
 *
 * Compose Attach Data
 */

#include "config.h"
#include "mutt/lib.h"
#include "attach_data.h"
#include "attach/lib.h"

/**
 * attach_data_free - Free the Compose Attach Data - Implements Menu::mdata_free() - @ingroup menu_mdata_free
 */
void attach_data_free(struct Menu *menu, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct ComposeAttachData *attach_data = *ptr;

  mutt_actx_free(&attach_data->actx);

  FREE(ptr);
}

/**
 * attach_data_new - Create new Compose Attach Data
 * @param e Email
 * @retval ptr New Compose Attach Data
 */
struct ComposeAttachData *attach_data_new(struct Email *e)
{
  struct ComposeAttachData *attach_data = mutt_mem_calloc(1, sizeof(struct ComposeAttachData));

  attach_data->actx = mutt_actx_new();
  attach_data->actx->email = e;

  return attach_data;
}
