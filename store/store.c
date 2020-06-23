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

#define STORE_BACKEND(name) extern const struct StoreOps store_##name##_ops;
STORE_BACKEND(bdb)
STORE_BACKEND(gdbm)
STORE_BACKEND(kyotocabinet)
STORE_BACKEND(lmdb)
STORE_BACKEND(qdbm)
STORE_BACKEND(rocksdb)
STORE_BACKEND(tdb)
STORE_BACKEND(tokyocabinet)
#undef STORE_BACKEND

/**
 * store_ops - Backend implementations
 */
const struct StoreOps *store_ops[] = {
#ifdef HAVE_TC
  &store_tokyocabinet_ops,
#endif
#ifdef HAVE_KC
  &store_kyotocabinet_ops,
#endif
#ifdef HAVE_QDBM
  &store_qdbm_ops,
#endif
#ifdef HAVE_ROCKSDB
  &store_rocksdb_ops,
#endif
#ifdef HAVE_GDBM
  &store_gdbm_ops,
#endif
#ifdef HAVE_BDB
  &store_bdb_ops,
#endif
#ifdef HAVE_TDB
  &store_tdb_ops,
#endif
#ifdef HAVE_LMDB
  &store_lmdb_ops,
#endif
  NULL,
};

/**
 * store_backend_list - Get a list of backend names
 * @retval ptr Comma-space-separated list of names
 *
 * The caller should free the string.
 */
const char *store_backend_list(void)
{
  char tmp[256] = { 0 };
  const struct StoreOps **ops = store_ops;
  size_t len = 0;

  for (; *ops; ops++)
  {
    if (len != 0)
    {
      len += snprintf(tmp + len, sizeof(tmp) - len, ", ");
    }
    len += snprintf(tmp + len, sizeof(tmp) - len, "%s", (*ops)->name);
  }

  return mutt_str_dup(tmp);
}

/**
 * store_get_backend_ops - Get the API functions for an store backend
 * @param str Name of the Store
 * @retval ptr Set of function pointers
 */
const struct StoreOps *store_get_backend_ops(const char *str)
{
  const struct StoreOps **ops = store_ops;

  if (!str || (*str == '\0'))
  {
    return *ops;
  }

  for (; *ops; ops++)
    if (strcmp(str, (*ops)->name) == 0)
      break;

  return *ops;
}

/**
 * store_is_valid_backend - Is the string a valid Store backend
 * @param str Store name
 * @retval true  s is recognized as a valid backend
 * @retval false otherwise
 */
bool store_is_valid_backend(const char *str)
{
  return store_get_backend_ops(str);
}
