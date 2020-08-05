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

struct MSN
{
  struct Email **cache;
  size_t         size;
  size_t         capacity;
};

size_t imap_msn_count(const struct MSN *msn)
{
  return msn ? msn->size : 0;
}

struct Email *imap_msn_get(const struct MSN *msn, size_t idx)
{
  if (!msn || idx > msn->size)
    return NULL;

  return msn->cache[idx];
}

void imap_msn_set(struct MSN *msn, size_t idx, struct Email *e)
{
  if (!msn || idx > msn->capacity)
    return;
  msn->cache[idx] = e;
  msn->size = MAX(msn->size, idx + 1);
}

size_t imap_msn_shrink(struct MSN *msn, size_t num)
{
  if (!msn || num == 0)
    return 0;

  size_t shrinked = 0;
  for (; msn->size && num; msn->size--, num--, shrinked++)
  {
    imap_msn_invalidate(msn, msn->size - 1);
  }

  return shrinked;
}

void imap_msn_invalidate(struct MSN *msn, size_t num)
{
  if (!msn || num > msn->size)
    return;

  msn->cache[num] = NULL;
}

void imap_msn_free(struct MSN **msn)
{
  if (!msn)
    return;

  FREE(&(*msn)->cache);
  FREE(msn);
}

/**
 * imap_msn_reserve - Create lookup table of MSN to Header
 * @param mdata Imap Mailbox data
 * @param num Number of MSNs in use
 *
 * Mapping from Message Sequence Number to Header
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
