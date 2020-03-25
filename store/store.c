/**
 * @file
 * Shared store code
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
 * @page store_store Shared store code
 *
 * Shared store code
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "mutt/lib.h"
#include "lib.h"

#define HCACHE_BACKEND(name) extern const struct HcacheOps hcache_##name##_ops;
HCACHE_BACKEND(bdb)
HCACHE_BACKEND(gdbm)
HCACHE_BACKEND(kyotocabinet)
HCACHE_BACKEND(lmdb)
HCACHE_BACKEND(qdbm)
HCACHE_BACKEND(tdb)
HCACHE_BACKEND(tokyocabinet)
#undef HCACHE_BACKEND

/**
 * hcache_ops - Backend implementations
 */
const struct HcacheOps *hcache_ops[] = {
#ifdef HAVE_TC
  &hcache_tokyocabinet_ops,
#endif
#ifdef HAVE_KC
  &hcache_kyotocabinet_ops,
#endif
#ifdef HAVE_QDBM
  &hcache_qdbm_ops,
#endif
#ifdef HAVE_GDBM
  &hcache_gdbm_ops,
#endif
#ifdef HAVE_BDB
  &hcache_bdb_ops,
#endif
#ifdef HAVE_TDB
  &hcache_tdb_ops,
#endif
#ifdef HAVE_LMDB
  &hcache_lmdb_ops,
#endif
  NULL,
};

/**
 * mutt_hcache_backend_list - Get a list of backend names
 * @retval ptr Comma-space-separated list of names
 *
 * @note The returned string must be free'd by the caller
 */
const char *mutt_hcache_backend_list(void)
{
  char tmp[256] = { 0 };
  const struct HcacheOps **ops = hcache_ops;
  size_t len = 0;

  for (; *ops; ops++)
  {
    if (len != 0)
    {
      len += snprintf(tmp + len, sizeof(tmp) - len, ", ");
    }
    len += snprintf(tmp + len, sizeof(tmp) - len, "%s", (*ops)->name);
  }

  return mutt_str_strdup(tmp);
}

/**
 * hcache_get_backend_ops - Get the API functions for an hcache backend
 * @param backend Name of the backend
 * @retval ptr Set of function pointers
 */
const struct HcacheOps *hcache_get_backend_ops(const char *backend)
{
  const struct HcacheOps **ops = hcache_ops;

  if (!backend || !*backend)
  {
    return *ops;
  }

  for (; *ops; ops++)
    if (strcmp(backend, (*ops)->name) == 0)
      break;

  return *ops;
}

/**
 * mutt_hcache_is_valid_backend - Is the string a valid hcache backend
 * @param s String identifying a backend
 * @retval true  s is recognized as a valid backend
 * @retval false otherwise
 */
bool mutt_hcache_is_valid_backend(const char *s)
{
  return hcache_get_backend_ops(s);
}
