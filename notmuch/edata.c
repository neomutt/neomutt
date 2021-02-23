/**
 * @file
 * Notmuch-specific Email data
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
 * @page nm_edata Notmuch-specific Email data
 *
 * Notmuch-specific Email data
 */

#include "config.h"
#include "mutt/lib.h"
#include "email/lib.h"
#include "edata.h"

/**
 * nm_edata_free - Free data attached to an Email
 * @param[out] ptr Email data
 *
 * Each email has an attached NmEmailData, which contains things like the tags
 * (labels).
 */
void nm_edata_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct NmEmailData *edata = *ptr;

  mutt_debug(LL_DEBUG2, "nm: freeing email %p\n", (void *) edata);
  FREE(&edata->folder);
  FREE(&edata->oldpath);
  FREE(&edata->virtual_id);

  FREE(ptr);
}

/**
 * nm_edata_new - Create a new NmEmailData for an email
 * @retval ptr New NmEmailData struct
 */
struct NmEmailData *nm_edata_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct NmEmailData));
}

/**
 * nm_edata_get - Get the Notmuch Email data
 * @param e Email
 * @retval ptr  Success
 * @retval NULL Error
 */
struct NmEmailData *nm_edata_get(struct Email *e)
{
  if (!e)
    return NULL;

  return e->nm_edata;
}
