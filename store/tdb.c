/**
 * @file
 * Trivial DataBase (TDB) backend for the key/value Store
 *
 * @authors
 * Copyright (C) 2020 Tino Reichardt <github@mcmilk.de>
 * Copyright (C) 2020-2023 Richard Russon <rich@flatcap.org>
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
 * @page store_tdb Trivial DataBase (TDB)
 *
 * Trivial DataBase (TDB) backend for the key/value Store.
 * https://tdb.samba.org/
 */

#include "config.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <tdb.h>
#include "mutt/lib.h"
#include "lib.h"

/**
 * store_tdb_open - Open a connection to a Store - Implements StoreOps::open() - @ingroup store_open
 */
static StoreHandle *store_tdb_open(const char *path, bool create)
{
  if (!path)
    return NULL;

  /* TDB_NOLOCK - Don't do any locking
   * TDB_NOSYNC - Don't use synchronous transactions
   * TDB_INCOMPATIBLE_HASH - Better hashing
   */
  const int flags = TDB_NOLOCK | TDB_INCOMPATIBLE_HASH | TDB_NOSYNC;
  const int hash_size = 33533; // Based on test timings for 100K emails

  struct tdb_context *db = tdb_open(path, hash_size, flags,
                                    (create ? O_CREAT : 0) | O_RDWR, 00600);

  // Return an opaque pointer
  return (StoreHandle *) db;
}

/**
 * store_tdb_fetch - Fetch a Value from the Store - Implements StoreOps::fetch() - @ingroup store_fetch
 */
static void *store_tdb_fetch(StoreHandle *store, const char *key, size_t klen, size_t *vlen)
{
  if (!store)
    return NULL;

  // Decloak an opaque pointer
  TDB_CONTEXT *db = store;
  TDB_DATA dkey;
  TDB_DATA data;

  dkey.dptr = (unsigned char *) key;
  dkey.dsize = klen;
  data = tdb_fetch(db, dkey);

  *vlen = data.dsize;
  return data.dptr;
}

/**
 * store_tdb_free - Free a Value returned by fetch() - Implements StoreOps::free() - @ingroup store_free
 */
static void store_tdb_free(StoreHandle *store, void **ptr)
{
  FREE(ptr);
}

/**
 * store_tdb_store - Write a Value to the Store - Implements StoreOps::store() - @ingroup store_store
 */
static int store_tdb_store(StoreHandle *store, const char *key, size_t klen,
                           void *value, size_t vlen)
{
  if (!store)
    return -1;

  // Decloak an opaque pointer
  TDB_CONTEXT *db = store;
  TDB_DATA dkey;
  TDB_DATA databuf;

  dkey.dptr = (unsigned char *) key;
  dkey.dsize = klen;

  databuf.dsize = vlen;
  databuf.dptr = value;

  return tdb_store(db, dkey, databuf, TDB_INSERT);
}

/**
 * store_tdb_delete_record - Delete a record from the Store - Implements StoreOps::delete_record() - @ingroup store_delete_record
 */
static int store_tdb_delete_record(StoreHandle *store, const char *key, size_t klen)
{
  if (!store)
    return -1;

  // Decloak an opaque pointer
  TDB_CONTEXT *db = store;
  TDB_DATA dkey;

  dkey.dptr = (unsigned char *) key;
  dkey.dsize = klen;

  return tdb_delete(db, dkey);
}

/**
 * store_tdb_close - Close a Store connection - Implements StoreOps::close() - @ingroup store_close
 */
static void store_tdb_close(StoreHandle **ptr)
{
  if (!ptr || !*ptr)
    return;

  // Decloak an opaque pointer
  TDB_CONTEXT *db = *ptr;
  tdb_close(db);
  *ptr = NULL;
}

/**
 * store_tdb_version - Get a Store version string - Implements StoreOps::version() - @ingroup store_version
 */
static const char *store_tdb_version(void)
{
  // TDB doesn't supply any version info
  return "tdb";
}

STORE_BACKEND_OPS(tdb)
