/**
 * @file
 * Maildir-specific Mailbox data
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page maildir_mdata Maildir-specific Mailbox data
 *
 * Maildir-specific Mailbox data
 */

#include "config.h"
#include "mutt/lib.h"
#include "core/lib.h"
#include "mdata.h"

/**
 * maildir_mdata_free - Free the private Mailbox data - Implements Mailbox::mdata_free()
 */
void maildir_mdata_free(void **ptr)
{
  // struct MaildirMboxData *mdata = *ptr;
  FREE(ptr);
}

/**
 * maildir_mdata_new - Create a new MaildirMboxData object
 * @retval ptr New MaildirMboxData struct
 */
struct MaildirMboxData *maildir_mdata_new(void)
{
  struct MaildirMboxData *mdata = mutt_mem_calloc(1, sizeof(struct MaildirMboxData));
  return mdata;
}

/**
 * maildir_mdata_get - Get the private data for this Mailbox
 * @param m Mailbox
 * @retval ptr MaildirMboxData
 */
struct MaildirMboxData *maildir_mdata_get(struct Mailbox *m)
{
  if (!m || ((m->type != MUTT_MAILDIR) && (m->type != MUTT_MH)))
    return NULL;
  return m->mdata;
}
