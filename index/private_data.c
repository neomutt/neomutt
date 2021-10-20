/**
 * @file
 * Private state data for the Index
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
 * @page index_private_data Private data for the Index
 *
 * Private state data for the Index
 */

#include "config.h"
#include "mutt/lib.h"
#include "private_data.h"

/**
 * index_private_data_free - Free Private Index Data - Implements MuttWindow::wdata_free() - @ingroup window_wdata_free
 */
void index_private_data_free(struct MuttWindow *win, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  // struct IndexPrivateData *priv = *ptr;

  FREE(ptr);
}

/**
 * index_private_data_new - Create new Index Data
 * @retval ptr New IndexPrivateData
 */
struct IndexPrivateData *index_private_data_new(struct IndexSharedData *shared)
{
  struct IndexPrivateData *priv = mutt_mem_calloc(1, sizeof(struct IndexPrivateData));

  priv->shared = shared;
  priv->newcount = -1;
  priv->oldcount = -1;

  return priv;
}
