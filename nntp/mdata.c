/**
 * @file
 * Nntp-specific Mailbox data
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
 * @page nntp_mdata Nntp-specific Mailbox data
 *
 * Nntp-specific Mailbox data
 */

#include "config.h"
#include "private.h"
#include "mutt/lib.h"
#include "mdata.h"
#include "bcache/lib.h"

/**
 * nntp_mdata_free - Free the private Mailbox data - Implements Mailbox::mdata_free()
 */
void nntp_mdata_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct NntpMboxData *mdata = *ptr;

  nntp_acache_free(mdata);
  mutt_bcache_close(&mdata->bcache);
  FREE(&mdata->newsrc_ent);
  FREE(&mdata->desc);
  FREE(ptr);
}
