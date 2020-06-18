/**
 * @file
 * Trivial DataBase (TDB) backend for the key/value Store
 *
 * @authors
 * Copyright (C) 2020 Tino Reichardt <milky-neomutt@mcmilk.de>
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
#include <stddef.h>
#include <fcntl.h>
#include <tdb.h>
#include "mutt/lib.h"
#include "lib.h"

/**
 * store_tdb_open - Implements StoreOps::open()
 */
static void *store_tdb_open(const char *path)
{
  if (!path)
    return NULL;

  /* TDB_NOLOCK - Don't do any locking
   * TDB_NOSYNC - Don't use synchronous transactions
   * TDB_INCOMPATIBLE_HASH - Better hashing
   */
  const int flags = TDB_NOLOCK | TDB_INCOMPATIBLE_HASH | TDB_NOSYNC;
  const int hash_size = 33533; // Based on test timings for 100K emails
  return tdb_open(path, hash_size, flags, O_CREAT | O_RDWR, 00600);
}

/**
 * store_tdb_fetch - Implements StoreOps::fetch()
 */
static void *store_tdb_fetch(void *store, const char *key, size_t klen, size_t *vlen)
{
  if (!store)
    return NULL;

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
 * store_tdb_free - Implements StoreOps::free()
 */
static void store_tdb_free(void *store, void **ptr)
{
  FREE(ptr);
}

/**
 * store_tdb_store - Implements StoreOps::store()
 */
static int store_tdb_store(void *store, const char *key, size_t klen, void *value, size_t vlen)
{
  if (!store)
    return -1;

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
 * store_tdb_delete_record - Implements StoreOps::delete_record()
 */
static int store_tdb_delete_record(void *store, const char *key, size_t klen)
{
  if (!store)
    return -1;

  TDB_CONTEXT *db = store;
  TDB_DATA dkey;

  dkey.dptr = (unsigned char *) key;
  dkey.dsize = klen;

  return tdb_delete(db, dkey);
}

/**
 * store_tdb_close - Implements StoreOps::close()
 */
static void store_tdb_close(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  TDB_CONTEXT *db = *ptr;
  tdb_close(db);
  *ptr = NULL;
}

/**
 * store_tdb_version - Implements StoreOps::version()
 */
static const char *store_tdb_version(void)
{
  // TDB doesn't supply any version info
  return "tdb";
}

STORE_BACKEND_OPS(tdb)
