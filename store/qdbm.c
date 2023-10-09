/**
 * @file
 * Quick Database Manager (QDBM) backend for the key/value Store
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
 * @page store_qdbm Quick Database Manager (QDBM)
 *
 * Quick Database Manager (QDBM) backend for the key/value Store.
 * https://dbmx.net/qdbm/
 */

#include "config.h"
#include <stddef.h>
#include <depot.h>
#include <stdbool.h>
#include <villa.h>
#include "mutt/lib.h"
#include "lib.h"

/**
 * store_qdbm_open - Open a connection to a Store - Implements StoreOps::open() - @ingroup store_open
 */
static StoreHandle *store_qdbm_open(const char *path)
{
  if (!path)
    return NULL;

  VILLA *db = vlopen(path, VL_OWRITER | VL_OCREAT, VL_CMPLEX);

  // Return an opaque pointer
  return (StoreHandle *) db;
}

/**
 * store_qdbm_fetch - Fetch a Value from the Store - Implements StoreOps::fetch() - @ingroup store_fetch
 */
static void *store_qdbm_fetch(StoreHandle *store, const char *key, size_t klen, size_t *vlen)
{
  if (!store)
    return NULL;

  // Decloak an opaque pointer
  VILLA *db = store;
  int sp = 0;
  void *rv = vlget(db, key, klen, &sp);
  *vlen = sp;
  return rv;
}

/**
 * store_qdbm_free - Free a Value returned by fetch() - Implements StoreOps::free() - @ingroup store_free
 */
static void store_qdbm_free(StoreHandle *store, void **ptr)
{
  FREE(ptr);
}

/**
 * store_qdbm_store - Write a Value to the Store - Implements StoreOps::store() - @ingroup store_store
 */
static int store_qdbm_store(StoreHandle *store, const char *key, size_t klen,
                            void *value, size_t vlen)
{
  if (!store)
    return -1;

  // Decloak an opaque pointer
  VILLA *db = store;
  /* Not sure if dpecode is reset on success, so better to explicitly return 0
   * on success */
  bool success = vlput(db, key, klen, value, vlen, VL_DOVER);
  return success ? 0 : dpecode ? dpecode : -1;
}

/**
 * store_qdbm_delete_record - Delete a record from the Store - Implements StoreOps::delete_record() - @ingroup store_delete_record
 */
static int store_qdbm_delete_record(StoreHandle *store, const char *key, size_t klen)
{
  if (!store)
    return -1;

  // Decloak an opaque pointer
  VILLA *db = store;
  /* Not sure if dpecode is reset on success, so better to explicitly return 0
   * on success */
  bool success = vlout(db, key, klen);
  return success ? 0 : dpecode ? dpecode : -1;
}

/**
 * store_qdbm_close - Close a Store connection - Implements StoreOps::close() - @ingroup store_close
 */
static void store_qdbm_close(StoreHandle **ptr)
{
  if (!ptr || !*ptr)
    return;

  // Decloak an opaque pointer
  VILLA *db = *ptr;
  vlclose(db);
  *ptr = NULL;
}

/**
 * store_qdbm_version - Get a Store version string - Implements StoreOps::version() - @ingroup store_version
 */
static const char *store_qdbm_version(void)
{
  return "qdbm " _QDBM_VERSION;
}

STORE_BACKEND_OPS(qdbm)
