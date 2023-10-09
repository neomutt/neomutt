/**
 * @file
 * Private state data for Attachments
 *
 * @authors
 * Copyright (C) 2022 Richard Russon <rich@flatcap.org>
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
 * @page attach_private_data Private state data for Attachments
 *
 * Private state data for Attachments
 */

#include "config.h"
#include "mutt/lib.h"
#include "private_data.h"

/**
 * attach_private_data_free - Free the Attach Data - Implements Menu::mdata_free() - @ingroup menu_mdata_free
 * @param menu Menu
 * @param ptr  Attach Data to free
 */
void attach_private_data_free(struct Menu *menu, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  FREE(ptr);
}

/**
 * attach_private_data_new - Create new Attach Data
 * @retval ptr New AttachPrivateData
 */
struct AttachPrivateData *attach_private_data_new(void)
{
  struct AttachPrivateData *priv = mutt_mem_calloc(1, sizeof(struct AttachPrivateData));

  return priv;
}
