/**
 * @file
 * Private state data for the Browser
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
 * @page browser_private_data Private data for the Browser
 *
 * Private state data for the Browser
 */

#include "config.h"
#include "mutt/lib.h"
#include "private_data.h"

/**
 * browser_private_data_free - Free Private Browser Data - Implements MuttWindow::wdata_free() - @ingroup window_wdata_free
 */
void browser_private_data_free(struct BrowserPrivateData **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct BrowserPrivateData *priv = *ptr;

  mutt_buffer_pool_release(&priv->OldLastDir);
  mutt_buffer_pool_release(&priv->prefix);
  destroy_state(&priv->state);

  FREE(ptr);
}

/**
 * browser_private_data_new - Create new Browser Data
 * @retval ptr New BrowserPrivateData
 */
struct BrowserPrivateData *browser_private_data_new(void)
{
  struct BrowserPrivateData *priv = mutt_mem_calloc(1, sizeof(struct BrowserPrivateData));

  priv->OldLastDir = mutt_buffer_pool_get();
  priv->prefix = mutt_buffer_pool_get();

  return priv;
}
