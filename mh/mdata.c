/**
 * @file
 * Mh-specific Mailbox data
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
 * @page mh_mdata Mh-specific Mailbox data
 *
 * Mh-specific Mailbox data
 */

#include "config.h"
#include "mutt/lib.h"
#include "core/lib.h"
#include "mdata.h"

/**
 * mh_mdata_free - Free the private Mailbox data - Implements Mailbox::mdata_free() - @ingroup mailbox_mdata_free
 */
void mh_mdata_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  FREE(ptr);
}

/**
 * mh_mdata_new - Create a new MhMboxData object
 * @retval ptr New MhMboxData struct
 */
struct MhMboxData *mh_mdata_new(void)
{
  return MUTT_MEM_CALLOC(1, struct MhMboxData);
}

/**
 * mh_mdata_get - Get the private data for this Mailbox
 * @param m Mailbox
 * @retval ptr MhMboxData
 */
struct MhMboxData *mh_mdata_get(struct Mailbox *m)
{
  if (m && (m->type == MUTT_MH))
    return m->mdata;

  return NULL;
}
