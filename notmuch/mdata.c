/**
 * @file
 * Notmuch-specific Mailbox data
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
 * @page nm_mdata Notmuch-specific Mailbox data
 *
 * Notmuch-specific Mailbox data
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mdata.h"
#include "progress/lib.h"
#include "query.h"

/**
 * nm_mdata_free - Free the private Mailbox data - Implements Mailbox::mdata_free()
 *
 * The NmMboxData struct stores global Notmuch data, such as the connection to
 * the database.  This function will close the database, free the resources and
 * the struct itself.
 */
void nm_mdata_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct NmMboxData *mdata = *ptr;

  mutt_debug(LL_DEBUG1, "nm: freeing context data %p\n", mdata);

  url_free(&mdata->db_url);
  FREE(&mdata->db_query);
  progress_free(&mdata->progress);
  FREE(ptr);
}

/**
 * nm_mdata_new - Create a new NmMboxData object from a query
 * @param url Notmuch query string
 * @retval ptr New NmMboxData struct
 *
 * A new NmMboxData struct is created, then the query is parsed and saved
 * within it.  This should be freed using nm_mdata_free().
 */
struct NmMboxData *nm_mdata_new(const char *url)
{
  if (!url)
    return NULL;

  struct NmMboxData *mdata = mutt_mem_calloc(1, sizeof(struct NmMboxData));
  mutt_debug(LL_DEBUG1, "nm: initialize mailbox mdata %p\n", (void *) mdata);

  const short c_nm_db_limit = cs_subset_number(NeoMutt->sub, "nm_db_limit");
  const char *const c_nm_query_type = cs_subset_string(NeoMutt->sub, "nm_query_type");
  mdata->db_limit = c_nm_db_limit;
  mdata->query_type = nm_string_to_query_type(c_nm_query_type);
  mdata->db_url = url_parse(url);
  if (!mdata->db_url)
  {
    mutt_error(_("failed to parse notmuch url: %s"), url);
    FREE(&mdata);
    return NULL;
  }
  return mdata;
}

/**
 * nm_mdata_get - Get the Notmuch Mailbox data
 * @param m Mailbox
 * @retval ptr  Success
 * @retval NULL Failure, not a Notmuch mailbox
 */
struct NmMboxData *nm_mdata_get(struct Mailbox *m)
{
  if (!m || (m->type != MUTT_NOTMUCH))
    return NULL;

  return m->mdata;
}
