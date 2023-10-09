/**
 * @file
 * Lightning Memory-Mapped Database (LMDB) backend for the key/value Store
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
 * @page store_lmdb Lightning Memory-Mapped Database (LMDB)
 *
 * Lightning Memory-Mapped Database (LMDB) backend for the key/value Store.
 * https://symas.com/lmdb/
 */

#include "config.h"
#include <stddef.h>
#include <lmdb.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "lib.h"

/** The maximum size of the database file (2GiB).
 * The file is mmap(2)'d into memory. */
#if (UINTPTR_MAX == 0xffffffff)
/// Maximum LMDB database size: 32-bit, limit to 2GiB
static const size_t LMDB_DB_SIZE = 2147483648;
#elif (UINTPTR_MAX == 0xffffffffffffffff)
/// Maximum LMDB database size: 64 bit, limit to 100GiB
static const size_t LMDB_DB_SIZE = 107374182400;
#else
#error
#endif

/**
 * enum LmdbTxnMode - LMDB transaction state
 */
enum LmdbTxnMode
{
  TXN_UNINITIALIZED, ///< Transaction is uninitialised
  TXN_READ,          ///< Read transaction in progress
  TXN_WRITE,         ///< Write transaction in progress
};

/**
 * struct LmdbStoreData - LMDB store
 */
struct LmdbStoreData
{
  MDB_env *env;
  MDB_txn *txn;
  MDB_dbi db;
  enum LmdbTxnMode txn_mode;
};

/**
 * lmdb_sdata_free - Free Lmdb Store Data
 * @param ptr Lmdb Store Data to free
 */
static void lmdb_sdata_free(struct LmdbStoreData **ptr)
{
  FREE(ptr);
}

/**
 * lmdb_sdata_new - Create new Lmdb Store Data
 * @retval ptr New Lmdb Store Data
 */
static struct LmdbStoreData *lmdb_sdata_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct LmdbStoreData));
}

/**
 * lmdb_get_read_txn - Get an LMDB read transaction
 * @param sdata LMDB store
 * @retval num LMDB return code, e.g. MDB_SUCCESS
 */
static int lmdb_get_read_txn(struct LmdbStoreData *sdata)
{
  int rc;

  if (sdata->txn && ((sdata->txn_mode == TXN_READ) || (sdata->txn_mode == TXN_WRITE)))
    return MDB_SUCCESS;

  if (sdata->txn)
    rc = mdb_txn_renew(sdata->txn);
  else
    rc = mdb_txn_begin(sdata->env, NULL, MDB_RDONLY, &sdata->txn);

  if (rc == MDB_SUCCESS)
  {
    sdata->txn_mode = TXN_READ;
  }
  else
  {
    mutt_debug(LL_DEBUG2, "%s: %s\n",
               sdata->txn ? "mdb_txn_renew" : "mdb_txn_begin", mdb_strerror(rc));
  }

  return rc;
}

/**
 * lmdb_get_write_txn - Get an LMDB write transaction
 * @param sdata LMDB store
 * @retval num LMDB return code, e.g. MDB_SUCCESS
 */
static int lmdb_get_write_txn(struct LmdbStoreData *sdata)
{
  if (sdata->txn)
  {
    if (sdata->txn_mode == TXN_WRITE)
      return MDB_SUCCESS;

    /* Free up the memory for readonly or reset transactions */
    mdb_txn_abort(sdata->txn);
  }

  int rc = mdb_txn_begin(sdata->env, NULL, 0, &sdata->txn);
  if (rc == MDB_SUCCESS)
    sdata->txn_mode = TXN_WRITE;
  else
    mutt_debug(LL_DEBUG2, "mdb_txn_begin: %s\n", mdb_strerror(rc));

  return rc;
}

/**
 * store_lmdb_open - Open a connection to a Store - Implements StoreOps::open() - @ingroup store_open
 */
static StoreHandle *store_lmdb_open(const char *path)
{
  if (!path)
    return NULL;

  struct LmdbStoreData *sdata = lmdb_sdata_new();

  int rc = mdb_env_create(&sdata->env);
  if (rc != MDB_SUCCESS)
  {
    mutt_debug(LL_DEBUG2, "mdb_env_create: %s\n", mdb_strerror(rc));
    lmdb_sdata_free(&sdata);
    return NULL;
  }

  mdb_env_set_mapsize(sdata->env, LMDB_DB_SIZE);

  rc = mdb_env_open(sdata->env, path, MDB_NOSUBDIR, 0644);
  if (rc != MDB_SUCCESS)
  {
    mutt_debug(LL_DEBUG2, "mdb_env_open: %s\n", mdb_strerror(rc));
    goto fail_env;
  }

  rc = lmdb_get_read_txn(sdata);
  if (rc != MDB_SUCCESS)
  {
    mutt_debug(LL_DEBUG2, "mdb_txn_begin: %s\n", mdb_strerror(rc));
    goto fail_env;
  }

  rc = mdb_dbi_open(sdata->txn, NULL, MDB_CREATE, &sdata->db);
  if (rc != MDB_SUCCESS)
  {
    mutt_debug(LL_DEBUG2, "mdb_dbi_open: %s\n", mdb_strerror(rc));
    goto fail_dbi;
  }

  mdb_txn_reset(sdata->txn);
  sdata->txn_mode = TXN_UNINITIALIZED;
  // Return an opaque pointer
  return (StoreHandle *) sdata;

fail_dbi:
  mdb_txn_abort(sdata->txn);
  sdata->txn_mode = TXN_UNINITIALIZED;
  sdata->txn = NULL;

fail_env:
  mdb_env_close(sdata->env);
  lmdb_sdata_free(&sdata);
  return NULL;
}

/**
 * store_lmdb_fetch - Fetch a Value from the Store - Implements StoreOps::fetch() - @ingroup store_fetch
 */
static void *store_lmdb_fetch(StoreHandle *store, const char *key, size_t klen, size_t *vlen)
{
  if (!store)
    return NULL;

  MDB_val dkey = { 0 };
  MDB_val data = { 0 };

  // Decloak an opaque pointer
  struct LmdbStoreData *sdata = store;

  dkey.mv_data = (void *) key;
  dkey.mv_size = klen;
  data.mv_data = NULL;
  data.mv_size = 0;
  int rc = lmdb_get_read_txn(sdata);
  if (rc != MDB_SUCCESS)
  {
    sdata->txn = NULL;
    mutt_debug(LL_DEBUG2, "txn_renew: %s\n", mdb_strerror(rc));
    return NULL;
  }
  rc = mdb_get(sdata->txn, sdata->db, &dkey, &data);
  if (rc == MDB_NOTFOUND)
  {
    return NULL;
  }
  if (rc != MDB_SUCCESS)
  {
    mutt_debug(LL_DEBUG2, "mdb_get: %s\n", mdb_strerror(rc));
    return NULL;
  }

  *vlen = data.mv_size;
  return data.mv_data;
}

/**
 * store_lmdb_free - Free a Value returned by fetch() - Implements StoreOps::free() - @ingroup store_free
 */
static void store_lmdb_free(StoreHandle *store, void **ptr)
{
  /* LMDB data is owned by the database */
}

/**
 * store_lmdb_store - Write a Value to the Store - Implements StoreOps::store() - @ingroup store_store
 */
static int store_lmdb_store(StoreHandle *store, const char *key, size_t klen,
                            void *value, size_t vlen)
{
  if (!store)
    return -1;

  MDB_val dkey = { 0 };
  MDB_val databuf = { 0 };

  // Decloak an opaque pointer
  struct LmdbStoreData *sdata = store;

  dkey.mv_data = (void *) key;
  dkey.mv_size = klen;
  databuf.mv_data = value;
  databuf.mv_size = vlen;
  int rc = lmdb_get_write_txn(sdata);
  if (rc != MDB_SUCCESS)
  {
    mutt_debug(LL_DEBUG2, "lmdb_get_write_txn: %s\n", mdb_strerror(rc));
    return rc;
  }
  rc = mdb_put(sdata->txn, sdata->db, &dkey, &databuf, 0);
  if (rc != MDB_SUCCESS)
  {
    mutt_debug(LL_DEBUG2, "mdb_put: %s\n", mdb_strerror(rc));
    mdb_txn_abort(sdata->txn);
    sdata->txn_mode = TXN_UNINITIALIZED;
    sdata->txn = NULL;
  }
  return rc;
}

/**
 * store_lmdb_delete_record - Delete a record from the Store - Implements StoreOps::delete_record() - @ingroup store_delete_record
 */
static int store_lmdb_delete_record(StoreHandle *store, const char *key, size_t klen)
{
  if (!store)
    return -1;

  MDB_val dkey = { 0 };

  // Decloak an opaque pointer
  struct LmdbStoreData *sdata = store;

  dkey.mv_data = (void *) key;
  dkey.mv_size = klen;
  int rc = lmdb_get_write_txn(sdata);
  if (rc != MDB_SUCCESS)
  {
    mutt_debug(LL_DEBUG2, "lmdb_get_write_txn: %s\n", mdb_strerror(rc));
    return rc;
  }
  rc = mdb_del(sdata->txn, sdata->db, &dkey, NULL);
  if ((rc != MDB_SUCCESS) && (rc != MDB_NOTFOUND))
  {
    mutt_debug(LL_DEBUG2, "mdb_del: %s\n", mdb_strerror(rc));
    mdb_txn_abort(sdata->txn);
    sdata->txn_mode = TXN_UNINITIALIZED;
    sdata->txn = NULL;
  }

  return rc;
}

/**
 * store_lmdb_close - Close a Store connection - Implements StoreOps::close() - @ingroup store_close
 */
static void store_lmdb_close(StoreHandle **ptr)
{
  if (!ptr || !*ptr)
    return;

  // Decloak an opaque pointer
  struct LmdbStoreData *sdata = *ptr;

  if (sdata->txn)
  {
    if (sdata->txn_mode == TXN_WRITE)
      mdb_txn_commit(sdata->txn);
    else
      mdb_txn_abort(sdata->txn);

    sdata->txn_mode = TXN_UNINITIALIZED;
    sdata->txn = NULL;
  }

  mdb_env_close(sdata->env);
  lmdb_sdata_free((struct LmdbStoreData **) ptr);
}

/**
 * store_lmdb_version - Get a Store version string - Implements StoreOps::version() - @ingroup store_version
 */
static const char *store_lmdb_version(void)
{
  return "lmdb " MDB_VERSION_STRING;
}

STORE_BACKEND_OPS(lmdb)
