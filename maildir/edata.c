/**
 * @file
 * Maildir-specific Email data
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
 * @page maildir_edata Maildir-specific Email data
 *
 * Maildir-specific Email data
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "edata.h"

/**
 * maildir_edata_free - Free the private Email data - Implements Email::edata_free()
 */
void maildir_edata_free(void **ptr)
{
  struct MaildirEmailData *edata = *ptr;
  FREE(&edata->maildir_flags);

  FREE(ptr);
}

/**
 * maildir_edata_new - Create a new MaildirEmailData object
 * @retval ptr New MaildirEmailData struct
 */
struct MaildirEmailData *maildir_edata_new(void)
{
  struct MaildirEmailData *edata = mutt_mem_calloc(1, sizeof(struct MaildirEmailData));
  return edata;
}

/**
 * maildir_edata_get - Get the private data for this Email
 * @param e Email
 * @retval ptr MaildirEmailData
 */
struct MaildirEmailData *maildir_edata_get(struct Email *e)
{
  if (!e)
    return NULL;
  return e->edata;
}
