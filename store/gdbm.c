/**
 * @file
 * GNU dbm backend for the key/value Store
 *
 * @authors
 * Copyright (C) 2004 Thomas Glanzmann <sithglan@stud.uni-erlangen.de>
 * Copyright (C) 2004 Tobias Werth <sitowert@stud.uni-erlangen.de>
 * Copyright (C) 2004 Brian Fundakowski Feldman <green@FreeBSD.org>
 * Copyright (C) 2016 Pietro Cerutti <gahr@gahr.ch>
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
 * @page store_gdbm GNU dbm (GDBM)
 *
 * GNU dbm backend for the key/value Store.
 * https://www.gnu.org.ua/software/gdbm/
 */

#include "config.h"
#include <stddef.h>
#include <gdbm.h>
#include "mutt/lib.h"
#include "lib.h"
#include "mutt_globals.h"

/**
 * store_gdbm_open - Implements StoreOps::open()
 */
static void *store_gdbm_open(const char *path)
{
  if (!path)
    return NULL;

  const int pagesize = 4096;

  GDBM_FILE db = gdbm_open((char *) path, pagesize, GDBM_WRCREAT, 00600, NULL);
  if (db)
    return db;

  /* if rw failed try ro */
  return gdbm_open((char *) path, pagesize, GDBM_READER, 00600, NULL);
}

/**
 * store_gdbm_fetch - Implements StoreOps::fetch()
 */
static void *store_gdbm_fetch(void *store, const char *key, size_t klen, size_t *vlen)
{
  if (!store)
    return NULL;

  datum dkey;
  datum data;

  GDBM_FILE db = store;

  dkey.dptr = (char *) key;
  dkey.dsize = klen;
  data = gdbm_fetch(db, dkey);

  *vlen = data.dsize;
  return data.dptr;
}

/**
 * store_gdbm_free - Implements StoreOps::free()
 */
static void store_gdbm_free(void *store, void **ptr)
{
  FREE(ptr);
}

/**
 * store_gdbm_store - Implements StoreOps::store()
 */
static int store_gdbm_store(void *store, const char *key, size_t klen, void *value, size_t vlen)
{
  if (!store)
    return -1;

  datum dkey;
  datum databuf;

  GDBM_FILE db = store;

  dkey.dptr = (char *) key;
  dkey.dsize = klen;

  databuf.dsize = vlen;
  databuf.dptr = value;

  return gdbm_store(db, dkey, databuf, GDBM_REPLACE);
}

/**
 * store_gdbm_delete_record - Implements StoreOps::delete_record()
 */
static int store_gdbm_delete_record(void *store, const char *key, size_t klen)
{
  if (!store)
    return -1;

  datum dkey;

  GDBM_FILE db = store;

  dkey.dptr = (char *) key;
  dkey.dsize = klen;

  return gdbm_delete(db, dkey);
}

/**
 * store_gdbm_close - Implements StoreOps::close()
 */
static void store_gdbm_close(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  GDBM_FILE db = *ptr;
  gdbm_close(db);
  *ptr = NULL;
}

/**
 * store_gdbm_version - Implements StoreOps::version()
 */
static const char *store_gdbm_version(void)
{
  return gdbm_version;
}

STORE_BACKEND_OPS(gdbm)
