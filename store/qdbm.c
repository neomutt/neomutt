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
 * https://fallabs.com/qdbm/
 */

#include "config.h"
#include <stddef.h>
#include <depot.h>
#include <stdbool.h>
#include <villa.h>
#include "mutt/lib.h"
#include "lib.h"
#include "mutt_globals.h"

/**
 * store_qdbm_open - Implements StoreOps::open()
 */
static void *store_qdbm_open(const char *path)
{
  if (!path)
    return NULL;

  return vlopen(path, VL_OWRITER | VL_OCREAT, VL_CMPLEX);
}

/**
 * store_qdbm_fetch - Implements StoreOps::fetch()
 */
static void *store_qdbm_fetch(void *store, const char *key, size_t klen, size_t *vlen)
{
  if (!store)
    return NULL;

  VILLA *db = store;
  int sp = 0;
  void *rv = vlget(db, key, klen, &sp);
  *vlen = sp;
  return rv;
}

/**
 * store_qdbm_free - Implements StoreOps::free()
 */
static void store_qdbm_free(void *store, void **ptr)
{
  FREE(ptr);
}

/**
 * store_qdbm_store - Implements StoreOps::store()
 */
static int store_qdbm_store(void *store, const char *key, size_t klen, void *value, size_t vlen)
{
  if (!store)
    return -1;

  VILLA *db = store;
  /* Not sure if dbecode is reset on success, so better to explicitly return 0
   * on success */
  bool success = vlput(db, key, klen, value, vlen, VL_DOVER);
  return success ? 0 : dpecode ? dpecode : -1;
}

/**
 * store_qdbm_delete_record - Implements StoreOps::delete_record()
 */
static int store_qdbm_delete_record(void *store, const char *key, size_t klen)
{
  if (!store)
    return -1;

  VILLA *db = store;
  /* Not sure if dbecode is reset on success, so better to explicitly return 0
   * on success */
  bool success = vlout(db, key, klen);
  return success ? 0 : dpecode ? dpecode : -1;
}

/**
 * store_qdbm_close - Implements StoreOps::close()
 */
static void store_qdbm_close(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  VILLA *db = *ptr;
  vlclose(db);
  *ptr = NULL;
}

/**
 * store_qdbm_version - Implements StoreOps::version()
 */
static const char *store_qdbm_version(void)
{
  return "qdbm " _QDBM_VERSION;
}

STORE_BACKEND_OPS(qdbm)
