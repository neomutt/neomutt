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
#include <fcntl.h>
#include <rocksdb/c.h>
#include <rocksdb/version.h>
#include <string.h>
#include "mutt/lib.h"
#include "lib.h"

/**
 * struct RocksDB_Ctx - Berkeley DB context
 */
struct RocksDB_Ctx
{
  rocksdb_t *db;
  rocksdb_options_t *options;
  rocksdb_readoptions_t *read_options;
  rocksdb_writeoptions_t *write_options;
  char *err;
};

/**
 * store_rocksdb_open - Implements StoreOps::open()
 */
static void *store_rocksdb_open(const char *path)
{
  if (!path)
    return NULL;

  struct RocksDB_Ctx *ctx = mutt_mem_malloc(sizeof(struct RocksDB_Ctx));

  /* RocksDB store errors in form of strings */
  ctx->err = NULL;

  /* setup generic options, create new db and limit log to one file */
  ctx->options = rocksdb_options_create();
  rocksdb_options_set_create_if_missing(ctx->options, 1);
  rocksdb_options_set_keep_log_file_num(ctx->options, 1);

  /* setup read options, we verify with checksums */
  ctx->read_options = rocksdb_readoptions_create();
  rocksdb_readoptions_set_verify_checksums(ctx->read_options, 1);

  /* setup write options, no sync needed, disable WAL */
  ctx->write_options = rocksdb_writeoptions_create();
  rocksdb_writeoptions_set_sync(ctx->write_options, 0);
  rocksdb_writeoptions_disable_WAL(ctx->write_options, 1);

  rocksdb_options_set_compression(ctx->options, rocksdb_no_compression);

  /* open database and check for error in ctx->error */
  ctx->db = rocksdb_open(ctx->options, path, &ctx->err);
  if (ctx->err)
  {
    rocksdb_free(ctx->err);
    FREE(&ctx);
    return NULL;
  }

  return ctx;
}

/**
 * store_rocksdb_fetch - Implements StoreOps::fetch()
 */
static void *store_rocksdb_fetch(void *store, const char *key, size_t keylen, size_t *dlen)
{
  if (!store)
    return NULL;

  struct RocksDB_Ctx *ctx = store;

  void *rv = rocksdb_get(ctx->db, ctx->read_options, key, keylen, dlen, &ctx->err);
  if (ctx->err)
  {
    rocksdb_free(ctx->err);
    ctx->err = NULL;
    return NULL;
  }

  return rv;
}

/**
 * store_rocksdb_free - Implements StoreOps::free()
 */
static void store_rocksdb_free(void *store, void **ptr)
{
  FREE(ptr);
}

/**
 * store_rocksdb_store - Implements StoreOps::store()
 */
static int store_rocksdb_store(void *store, const char *key, size_t keylen,
                               void *data, size_t dlen)
{
  if (!store)
    return -1;

  struct RocksDB_Ctx *ctx = store;

  rocksdb_put(ctx->db, ctx->write_options, key, keylen, data, dlen, &ctx->err);
  if (ctx->err)
  {
    rocksdb_free(ctx->err);
    ctx->err = NULL;
    return -1;
  }

  return 0;
}

/**
 * store_rocksdb_delete_record - Implements StoreOps::delete_record()
 */
static int store_rocksdb_delete_record(void *store, const char *key, size_t keylen)
{
  if (!store)
    return -1;

  struct RocksDB_Ctx *ctx = store;

  rocksdb_delete(ctx->db, ctx->write_options, key, keylen, &ctx->err);
  if (ctx->err)
  {
    rocksdb_free(ctx->err);
    ctx->err = NULL;
    return -1;
  }

  return 0;
}

/**
 * store_rocksdb_close - Implements StoreOps::close()
 */
static void store_rocksdb_close(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct RocksDB_Ctx *ctx = *ptr;

  /* close database and free resources */
  rocksdb_close(ctx->db);
  rocksdb_options_destroy(ctx->options);
  rocksdb_readoptions_destroy(ctx->read_options);
  rocksdb_writeoptions_destroy(ctx->write_options);

  FREE(ptr);
  *ptr = NULL;
}

/**
 * store_rocksdb_version - Implements StoreOps::version()
 */
static const char *store_rocksdb_version(void)
{
/* return sth. like "RocksDB 6.7.3" */
#define RDBVER(major, minor, patch) #major "." #minor "." #patch
  return "RocksDB " RDBVER(ROCKSDB_MAJOR, ROCKSDB_MINOR, ROCKSDB_PATCH);
}

STORE_BACKEND_OPS(rocksdb)
