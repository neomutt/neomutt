/**
 * @file
 * Trivial DataBase backend for the header cache
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
 * @page hc_tdb TDB
 *
 * Use a Trivial DataBase file as a header cache backend.
 */

#include "config.h"
#include <stddef.h>
#include <fcntl.h>
#include <tdb.h>
#include "mutt/lib.h"
#include "lib.h"

/**
 * hcache_tdb_open - Implements HcacheOps::open()
 */
static void *hcache_tdb_open(const char *path)
{
  /**
   * TDB_NOLOCK - not needed
   * TDB_NOSYNC - not needed
   * TDB_INCOMPATIBLE_HASH - faster then the old one
   */
  int flags = TDB_NOLOCK | TDB_INCOMPATIBLE_HASH | TDB_NOSYNC;
  return tdb_open(path, 33657, flags, O_CREAT | O_RDWR, 00600);
}

/**
 * hcache_tdb_fetch - Implements HcacheOps::fetch()
 */
static void *hcache_tdb_fetch(void *ctx, const char *key, size_t keylen, size_t *dlen)
{
  if (!ctx)
    return NULL;

  TDB_CONTEXT *db = ctx;
  TDB_DATA dkey;
  TDB_DATA data;

  dkey.dptr = (unsigned char *) key;
  dkey.dsize = keylen;
  data = tdb_fetch(db, dkey);

  *dlen = data.dsize;
  return data.dptr;
}

/**
 * hcache_tdb_free - Implements HcacheOps::free()
 */
static void hcache_tdb_free(void *vctx, void **data)
{
  FREE(data);
}

/**
 * hcache_tdb_store - Implements HcacheOps::store()
 */
static int hcache_tdb_store(void *ctx, const char *key, size_t keylen, void *data, size_t dlen)
{
  if (!ctx)
    return -1;

  TDB_CONTEXT *db = ctx;
  TDB_DATA dkey;
  TDB_DATA databuf;

  dkey.dptr = (unsigned char *) key;
  dkey.dsize = keylen;

  databuf.dsize = dlen;
  databuf.dptr = data;

  return tdb_store(db, dkey, databuf, TDB_INSERT);
}

/**
 * hcache_tdb_delete_header - Implements HcacheOps::delete_header()
 */
static int hcache_tdb_delete_header(void *ctx, const char *key, size_t keylen)
{
  if (!ctx)
    return -1;

  TDB_CONTEXT *db = ctx;
  TDB_DATA dkey;

  dkey.dptr = (unsigned char *) key;
  dkey.dsize = keylen;

  return tdb_delete(db, dkey);
}

/**
 * hcache_tdb_close - Implements HcacheOps::close()
 */
static void hcache_tdb_close(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  TDB_CONTEXT *db = *ptr;
  tdb_close(db);
  *ptr = NULL;
}

/**
 * hcache_tdb_backend - Implements HcacheOps::backend()
 */
static const char *hcache_tdb_backend(void)
{
  return "tdb";
}

HCACHE_BACKEND_OPS(tdb)
