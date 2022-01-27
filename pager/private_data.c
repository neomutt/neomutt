/**
 * @file
 * Private state data for the Pager
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
 * @page pager_private_data Private state data for the Pager
 *
 * Private state data for the Pager
 */

#include "config.h"
#include "mutt/lib.h"
#include "private_data.h"
#include "pager/lib.h"

/**
 * pager_private_data_free - Free Pager Data
 * @param win Window
 * @param ptr Pager Data to free
 */
void pager_private_data_free(struct MuttWindow *win, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct PagerPrivateData *priv = *ptr;

  mutt_debug(LL_NOTIFY, "NT_PAGER_DELETE: %p\n", priv);
  notify_send(priv->notify, NT_PAGER, NT_PAGER_DELETE, priv);
  notify_free(&priv->notify);

  attr_color_list_clear(&priv->ansi_list);

  FREE(ptr);
}

/**
 * pager_private_data_new - Create new Pager Data
 * @retval ptr New PagerPrivateData
 */
struct PagerPrivateData *pager_private_data_new(void)
{
  struct PagerPrivateData *priv = mutt_mem_calloc(1, sizeof(struct PagerPrivateData));

  priv->notify = notify_new();

  // TODO initialize fields

  TAILQ_INIT(&priv->ansi_list);

  return priv;
}
