/**
 * @file
 * IMAP MSN helper functions
 *
 * @authors
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
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
 * @page imap_msn MSN helper functions
 *
 * IMAP MSN helper functions
 */

#include "config.h"
#include <limits.h>
#include <stdlib.h>
#include "mutt/lib.h"
#include "msn.h"
#include "mdata.h" // IWYU pragma: keep

/**
 * imap_msn_reserve - Create / reallocate the cache
 * @param msn MSN structure
 * @param num Number of MSNs to make room for
 */
void imap_msn_reserve(struct MSNArray *msn, size_t num)
{
  /* This is a conservative check to protect against a malicious imap
   * server.  Most likely size_t is bigger than an unsigned int, but
   * if msn_count is this big, we have a serious problem. */
  if (num >= (UINT_MAX / sizeof(struct Email *)))
  {
    mutt_error(_("Out of memory"));
    mutt_exit(1);
  }

  ARRAY_RESERVE(msn, num);
}

/**
 * imap_msn_free - Free the cache
 * @param msn MSN structure
 */
void imap_msn_free(struct MSNArray *msn)
{
  ARRAY_FREE(msn);
}

/**
 * imap_msn_highest - Return the highest MSN in use
 * @param msn MSN structure
 * @retval num The highest MSN in use
 */
size_t imap_msn_highest(const struct MSNArray *msn)
{
  return ARRAY_SIZE(msn);
}

/**
 * imap_msn_get - Return the Email associated with an msn
 * @param msn MSN structure
 * @param idx Index to retrieve
 * @retval ptr Pointer to Email or NULL
 */
struct Email *imap_msn_get(const struct MSNArray *msn, size_t idx)
{
  struct Email **ep = ARRAY_GET(msn, idx);
  return ep ? *ep : NULL;
}

/**
 * imap_msn_set - Cache an Email into a given position
 * @param msn MSN structure
 * @param idx Index in the cache
 * @param e   Email to cache
 */
void imap_msn_set(struct MSNArray *msn, size_t idx, struct Email *e)
{
  ARRAY_SET(msn, idx, e);
}

/**
 * imap_msn_shrink - Remove a number of entries from the end of the cache
 * @param msn MSN structure
 * @param num Number of entries to remove
 * @retval num Number of entries actually removed
 */
size_t imap_msn_shrink(struct MSNArray *msn, size_t num)
{
  return ARRAY_SHRINK(msn, num);
}

/**
 * imap_msn_remove - Remove an entry from the cache
 * @param msn MSN structure
 * @param idx Index to invalidate
 */
void imap_msn_remove(struct MSNArray *msn, size_t idx)
{
  struct Email **ep = ARRAY_GET(msn, idx);
  if (ep)
    *ep = NULL;
}
