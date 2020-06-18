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
#include "mutt/lib.h"
#include "lib.h"

/** The maximum size of the database file (2GiB).
 * The file is mmap(2)'d into memory. */
const size_t LMDB_DB_SIZE = 2147483648;

/**
 * enum MdbTxnMode - LMDB transaction state
 */
enum MdbTxnMode
{
  TXN_UNINITIALIZED, ///< Transaction is uninitialised
  TXN_READ,          ///< Read transaction in progress
  TXN_WRITE,         ///< Write transaction in progress
};

/**
 * struct StoreLmdbCtx - LMDB context
 */
struct StoreLmdbCtx
{
  MDB_env *env;
  MDB_txn *txn;
  MDB_dbi db;
  enum MdbTxnMode txn_mode;
};

/**
 * mdb_get_r_txn - Get an LMDB read transaction
 * @param ctx LMDB context
 * @retval num LMDB return code, e.g. MDB_SUCCESS
 */
static int mdb_get_r_txn(struct StoreLmdbCtx *ctx)
{
  int rc;

  if (ctx->txn && ((ctx->txn_mode == TXN_READ) || (ctx->txn_mode == TXN_WRITE)))
    return MDB_SUCCESS;

  if (ctx->txn)
    rc = mdb_txn_renew(ctx->txn);
  else
    rc = mdb_txn_begin(ctx->env, NULL, MDB_RDONLY, &ctx->txn);

  if (rc == MDB_SUCCESS)
    ctx->txn_mode = TXN_READ;
  else
  {
    mutt_debug(LL_DEBUG2, "%s: %s\n",
               ctx->txn ? "mdb_txn_renew" : "mdb_txn_begin", mdb_strerror(rc));
  }

  return rc;
}

/**
 * mdb_get_w_txn - Get an LMDB write transaction
 * @param ctx LMDB context
 * @retval num LMDB return code, e.g. MDB_SUCCESS
 */
static int mdb_get_w_txn(struct StoreLmdbCtx *ctx)
{
  int rc;

  if (ctx->txn)
  {
    if (ctx->txn_mode == TXN_WRITE)
      return MDB_SUCCESS;

    /* Free up the memory for readonly or reset transactions */
    mdb_txn_abort(ctx->txn);
  }

  rc = mdb_txn_begin(ctx->env, NULL, 0, &ctx->txn);
  if (rc == MDB_SUCCESS)
    ctx->txn_mode = TXN_WRITE;
  else
    mutt_debug(LL_DEBUG2, "mdb_txn_begin: %s\n", mdb_strerror(rc));

  return rc;
}

/**
 * store_lmdb_open - Implements StoreOps::open()
 */
static void *store_lmdb_open(const char *path)
{
  if (!path)
    return NULL;

  int rc;

  struct StoreLmdbCtx *ctx = mutt_mem_calloc(1, sizeof(struct StoreLmdbCtx));

  rc = mdb_env_create(&ctx->env);
  if (rc != MDB_SUCCESS)
  {
    mutt_debug(LL_DEBUG2, "mdb_env_create: %s\n", mdb_strerror(rc));
    FREE(&ctx);
    return NULL;
  }

  mdb_env_set_mapsize(ctx->env, LMDB_DB_SIZE);

  rc = mdb_env_open(ctx->env, path, MDB_NOSUBDIR, 0644);
  if (rc != MDB_SUCCESS)
  {
    mutt_debug(LL_DEBUG2, "mdb_env_open: %s\n", mdb_strerror(rc));
    goto fail_env;
  }

  rc = mdb_get_r_txn(ctx);
  if (rc != MDB_SUCCESS)
  {
    mutt_debug(LL_DEBUG2, "mdb_txn_begin: %s\n", mdb_strerror(rc));
    goto fail_env;
  }

  rc = mdb_dbi_open(ctx->txn, NULL, MDB_CREATE, &ctx->db);
  if (rc != MDB_SUCCESS)
  {
    mutt_debug(LL_DEBUG2, "mdb_dbi_open: %s\n", mdb_strerror(rc));
    goto fail_dbi;
  }

  mdb_txn_reset(ctx->txn);
  ctx->txn_mode = TXN_UNINITIALIZED;
  return ctx;

fail_dbi:
  mdb_txn_abort(ctx->txn);
  ctx->txn_mode = TXN_UNINITIALIZED;
  ctx->txn = NULL;

fail_env:
  mdb_env_close(ctx->env);
  FREE(&ctx);
  return NULL;
}

/**
 * store_lmdb_fetch - Implements StoreOps::fetch()
 */
static void *store_lmdb_fetch(void *store, const char *key, size_t klen, size_t *vlen)
{
  if (!store)
    return NULL;

  MDB_val dkey;
  MDB_val data;

  struct StoreLmdbCtx *ctx = store;

  dkey.mv_data = (void *) key;
  dkey.mv_size = klen;
  data.mv_data = NULL;
  data.mv_size = 0;
  int rc = mdb_get_r_txn(ctx);
  if (rc != MDB_SUCCESS)
  {
    ctx->txn = NULL;
    mutt_debug(LL_DEBUG2, "txn_renew: %s\n", mdb_strerror(rc));
    return NULL;
  }
  rc = mdb_get(ctx->txn, ctx->db, &dkey, &data);
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
 * store_lmdb_free - Implements StoreOps::free()
 */
static void store_lmdb_free(void *store, void **ptr)
{
  /* LMDB data is owned by the database */
}

/**
 * store_lmdb_store - Implements StoreOps::store()
 */
static int store_lmdb_store(void *store, const char *key, size_t klen, void *value, size_t vlen)
{
  if (!store)
    return -1;

  MDB_val dkey;
  MDB_val databuf;

  struct StoreLmdbCtx *ctx = store;

  dkey.mv_data = (void *) key;
  dkey.mv_size = klen;
  databuf.mv_data = value;
  databuf.mv_size = vlen;
  int rc = mdb_get_w_txn(ctx);
  if (rc != MDB_SUCCESS)
  {
    mutt_debug(LL_DEBUG2, "mdb_get_w_txn: %s\n", mdb_strerror(rc));
    return rc;
  }
  rc = mdb_put(ctx->txn, ctx->db, &dkey, &databuf, 0);
  if (rc != MDB_SUCCESS)
  {
    mutt_debug(LL_DEBUG2, "mdb_put: %s\n", mdb_strerror(rc));
    mdb_txn_abort(ctx->txn);
    ctx->txn_mode = TXN_UNINITIALIZED;
    ctx->txn = NULL;
  }
  return rc;
}

/**
 * store_lmdb_delete_record - Implements StoreOps::delete_record()
 */
static int store_lmdb_delete_record(void *store, const char *key, size_t klen)
{
  if (!store)
    return -1;

  MDB_val dkey;

  struct StoreLmdbCtx *ctx = store;

  dkey.mv_data = (void *) key;
  dkey.mv_size = klen;
  int rc = mdb_get_w_txn(ctx);
  if (rc != MDB_SUCCESS)
  {
    mutt_debug(LL_DEBUG2, "mdb_get_w_txn: %s\n", mdb_strerror(rc));
    return rc;
  }
  rc = mdb_del(ctx->txn, ctx->db, &dkey, NULL);
  if ((rc != MDB_SUCCESS) && (rc != MDB_NOTFOUND))
  {
    mutt_debug(LL_DEBUG2, "mdb_del: %s\n", mdb_strerror(rc));
    mdb_txn_abort(ctx->txn);
    ctx->txn_mode = TXN_UNINITIALIZED;
    ctx->txn = NULL;
  }

  return rc;
}

/**
 * store_lmdb_close - Implements StoreOps::close()
 */
static void store_lmdb_close(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct StoreLmdbCtx *db = *ptr;

  if (db->txn)
  {
    if (db->txn_mode == TXN_WRITE)
      mdb_txn_commit(db->txn);
    else
      mdb_txn_abort(db->txn);

    db->txn_mode = TXN_UNINITIALIZED;
    db->txn = NULL;
  }

  mdb_env_close(db->env);
  FREE(ptr);
}

/**
 * store_lmdb_version - Implements StoreOps::version()
 */
static const char *store_lmdb_version(void)
{
  return "lmdb " MDB_VERSION_STRING;
}

STORE_BACKEND_OPS(lmdb)
