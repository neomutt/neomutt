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
 * @page imap_msn IMAP MSN helper functions
 *
 * IMAP MSN helper functions
 */

#include "msn.h"
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "private.h"

struct Email;

/**
 * struct MSN - A cache to map IMAP MSNs to Emails
 */
struct MSN
{
  struct Email **cache;      ///< Email cache as a linear array indexed by MSN
  size_t         highest;    ///< Highest occupied slot number
  size_t         capacity;   ///< Number of allocated slots
};

/**
 * imap_msn_reserve - Create / reallocate the cache
 * @param msn MSN structure
 * @param num Number of MSNs to make room for
 */
void imap_msn_reserve(struct MSN **msnp, size_t num)
{
  if (!*msnp)
  {
    *msnp = mutt_mem_calloc(1, sizeof(struct MSN));
  }

  struct MSN *msn = *msnp;

  if (num <= msn->capacity)
    return;

  /* This is a conservative check to protect against a malicious imap
   * server.  Most likely size_t is bigger than an unsigned int, but
   * if msn_count is this big, we have a serious problem. */
  if (num >= (UINT_MAX / sizeof(struct Email *)))
  {
    mutt_error(_("Out of memory"));
    mutt_exit(1);
  }

  /* Add a little padding, like mx_allloc_memory() */
  num += 25;

  static const size_t esize = sizeof(struct Email *);
  if (!msn->cache)
    msn->cache = mutt_mem_calloc(num, esize);
  else
  {
    mutt_mem_realloc(&msn->cache, num * esize);
    memset(msn->cache + msn->capacity, 0, esize * (num - msn->capacity));
  }

  msn->capacity = num;
}

/**
 * imap_msn_free - Free the cache
 * @param msn MSN structure
 */
void imap_msn_free(struct MSN **msn)
{
  if (!msn)
    return;

  FREE(&(*msn)->cache);
  FREE(msn);
}

/**
 * imap_msn_highest - Return the highest MSN in use
 * @param msn MSN structure
 * @retval num The highest MSN in use
 */
size_t imap_msn_highest(const struct MSN *msn)
{
  return msn ? msn->highest : 0;
}

/**
 * imap_msn_get - Return the Email associated with an msn
 * @param msn MSN structure
 * @param idx Index to retrieve
 * @retval ptr Pointer to Email or NULL
 */
struct Email *imap_msn_get(const struct MSN *msn, size_t idx)
{
  if (!msn || idx > msn->highest)
    return NULL;

  return msn->cache[idx];
}

/**
 * imap_msn_set - Cache an Email into a given position
 * @param msn MSN structure
 * @param idx Index in the cache
 * @param e   Email to cache
 */
void imap_msn_set(struct MSN *msn, size_t idx, struct Email *e)
{
  if (!msn || idx > msn->capacity)
    return;
  msn->cache[idx] = e;
  msn->highest = MAX(msn->highest, idx + 1);
}

/**
 * imap_msn_shrink - Remove a number of entries from the end of the cache
 * @param msn MSN structure
 * @param num Number of entries to remove
 * @retval num Number of entries actually removed
 */
size_t imap_msn_shrink(struct MSN *msn, size_t num)
{
  if (!msn || num == 0)
    return 0;

  size_t shrinked = 0;
  for (; msn->highest && num; msn->highest--, num--, shrinked++)
  {
    imap_msn_remove(msn, msn->highest - 1);
  }

  return shrinked;
}

/**
 * imap_msn_remove - Remove an entry from the cache
 * @param msn MSN structure
 * @param idx Index to invalidate
 */
void imap_msn_remove(struct MSN *msn, size_t idx)
{
  if (!msn || idx > msn->highest)
    return;

  msn->cache[idx] = NULL;
}

