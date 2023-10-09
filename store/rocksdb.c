/**
 * @file
 * RocksDB backend for the key/value Store
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
 * @page store_rocksdb RocksDB
 *
 * Use the RocksDB - A persistent key-value store for fast storage environments
 * https://rocksdb.org/
 */

#include "config.h"
#include <stddef.h>
#include <rocksdb/c.h>
#include "mutt/lib.h"
#include "lib.h"

/**
 * struct RocksDbStoreData - RocksDB store
 */
struct RocksDbStoreData
{
  rocksdb_t *db;
  rocksdb_options_t *options;
  rocksdb_readoptions_t *read_options;
  rocksdb_writeoptions_t *write_options;
  char *err;
};

/**
 * rocksdb_sdata_free - Free RocksDb Store Data
 * @param ptr RocksDb Store Data to free
 */
static void rocksdb_sdata_free(struct RocksDbStoreData **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct RocksDbStoreData *sdata = *ptr;
  FREE(&sdata->err);

  FREE(ptr);
}

/**
 * rocksdb_sdata_new - Create new RocksDb Store Data
 * @retval ptr New RocksDb Store Data
 */
static struct RocksDbStoreData *rocksdb_sdata_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct RocksDbStoreData));
}

/**
 * store_rocksdb_open - Open a connection to a Store - Implements StoreOps::open() - @ingroup store_open
 */
static StoreHandle *store_rocksdb_open(const char *path)
{
  if (!path)
    return NULL;

  struct RocksDbStoreData *sdata = rocksdb_sdata_new();

  /* RocksDB stores errors in form of strings */
  sdata->err = NULL;

  /* setup generic options, create new db and limit log to one file */
  sdata->options = rocksdb_options_create();
  rocksdb_options_set_create_if_missing(sdata->options, 1);
  rocksdb_options_set_keep_log_file_num(sdata->options, 1);

  /* setup read options, we verify with checksums */
  sdata->read_options = rocksdb_readoptions_create();
  rocksdb_readoptions_set_verify_checksums(sdata->read_options, 1);

  /* setup write options, no sync needed, disable WAL */
  sdata->write_options = rocksdb_writeoptions_create();
  rocksdb_writeoptions_set_sync(sdata->write_options, 0);
  rocksdb_writeoptions_disable_WAL(sdata->write_options, 1);

  rocksdb_options_set_compression(sdata->options, rocksdb_no_compression);

  /* open database and check for error in sdata->error */
  sdata->db = rocksdb_open(sdata->options, path, &sdata->err);
  if (sdata->err)
  {
    rocksdb_free(sdata->err);
    FREE(&sdata);
    return NULL;
  }

  // Return an opaque pointer
  return (StoreHandle *) sdata;
}

/**
 * store_rocksdb_fetch - Fetch a Value from the Store - Implements StoreOps::fetch() - @ingroup store_fetch
 */
static void *store_rocksdb_fetch(StoreHandle *store, const char *key, size_t klen, size_t *vlen)
{
  if (!store)
    return NULL;

  // Decloak an opaque pointer
  struct RocksDbStoreData *sdata = store;

  void *rv = rocksdb_get(sdata->db, sdata->read_options, key, klen, vlen, &sdata->err);
  if (sdata->err)
  {
    rocksdb_free(sdata->err);
    sdata->err = NULL;
    return NULL;
  }

  return rv;
}

/**
 * store_rocksdb_free - Free a Value returned by fetch() - Implements StoreOps::free() - @ingroup store_free
 */
static void store_rocksdb_free(StoreHandle *store, void **ptr)
{
  FREE(ptr);
}

/**
 * store_rocksdb_store - Write a Value to the Store - Implements StoreOps::store() - @ingroup store_store
 */
static int store_rocksdb_store(StoreHandle *store, const char *key, size_t klen,
                               void *value, size_t vlen)
{
  if (!store)
    return -1;

  // Decloak an opaque pointer
  struct RocksDbStoreData *sdata = store;

  rocksdb_put(sdata->db, sdata->write_options, key, klen, value, vlen, &sdata->err);
  if (sdata->err)
  {
    rocksdb_free(sdata->err);
    sdata->err = NULL;
    return -1;
  }

  return 0;
}

/**
 * store_rocksdb_delete_record - Delete a record from the Store - Implements StoreOps::delete_record() - @ingroup store_delete_record
 */
static int store_rocksdb_delete_record(StoreHandle *store, const char *key, size_t klen)
{
  if (!store)
    return -1;

  // Decloak an opaque pointer
  struct RocksDbStoreData *sdata = store;

  rocksdb_delete(sdata->db, sdata->write_options, key, klen, &sdata->err);
  if (sdata->err)
  {
    rocksdb_free(sdata->err);
    sdata->err = NULL;
    return -1;
  }

  return 0;
}

/**
 * store_rocksdb_close - Close a Store connection - Implements StoreOps::close() - @ingroup store_close
 */
static void store_rocksdb_close(StoreHandle **ptr)
{
  if (!ptr || !*ptr)
    return;

  // Decloak an opaque pointer
  struct RocksDbStoreData *sdata = *ptr;

  /* close database and free resources */
  rocksdb_close(sdata->db);
  rocksdb_options_destroy(sdata->options);
  rocksdb_readoptions_destroy(sdata->read_options);
  rocksdb_writeoptions_destroy(sdata->write_options);

  rocksdb_sdata_free((struct RocksDbStoreData **) ptr);
}

/**
 * store_rocksdb_version - Get a Store version string - Implements StoreOps::version() - @ingroup store_version
 */
static const char *store_rocksdb_version(void)
{
/* return sth. like "RocksDB 6.7.3" */
#define RDBVER(major, minor, patch) #major "." #minor "." #patch
  return "RocksDB " RDBVER(ROCKSDB_MAJOR, ROCKSDB_MINOR, ROCKSDB_PATCH);
}

STORE_BACKEND_OPS(rocksdb)
