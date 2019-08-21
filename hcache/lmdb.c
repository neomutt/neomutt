/**
 * @file
 * LMDB backend for the header cache
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
 * @page hc_lmdb LMDB
 *
 * Use a Lightning Memory-Mapped Database file as a header cache backend.
 */

#include "config.h"
#include <stddef.h>
#include <lmdb.h>
#include "mutt/mutt.h"
#include "backend.h"

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
 * struct HcacheLmdbCtx - LMDB context
 */
struct HcacheLmdbCtx
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
static int mdb_get_r_txn(struct HcacheLmdbCtx *ctx)
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
static int mdb_get_w_txn(struct HcacheLmdbCtx *ctx)
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
 * hcache_lmdb_open - Implements HcacheOps::open()
 */
static void *hcache_lmdb_open(const char *path)
{
  int rc;

  struct HcacheLmdbCtx *ctx = mutt_mem_calloc(1, sizeof(struct HcacheLmdbCtx));

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
 * hcache_lmdb_fetch - Implements HcacheOps::fetch()
 */
static void *hcache_lmdb_fetch(void *vctx, const char *key, size_t keylen)
{
  if (!vctx)
    return NULL;

  MDB_val dkey;
  MDB_val data;

  struct HcacheLmdbCtx *ctx = vctx;

  dkey.mv_data = (void *) key;
  dkey.mv_size = keylen;
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

  return data.mv_data;
}

/**
 * hcache_lmdb_free - Implements HcacheOps::free()
 */
static void hcache_lmdb_free(void *vctx, void **data)
{
  /* LMDB data is owned by the database */
}

/**
 * hcache_lmdb_store - Implements HcacheOps::store()
 */
static int hcache_lmdb_store(void *vctx, const char *key, size_t keylen, void *data, size_t dlen)
{
  if (!vctx)
    return -1;

  MDB_val dkey;
  MDB_val databuf;

  struct HcacheLmdbCtx *ctx = vctx;

  dkey.mv_data = (void *) key;
  dkey.mv_size = keylen;
  databuf.mv_data = data;
  databuf.mv_size = dlen;
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
 * hcache_lmdb_delete_header - Implements HcacheOps::delete_header()
 */
static int hcache_lmdb_delete_header(void *vctx, const char *key, size_t keylen)
{
  if (!vctx)
    return -1;

  MDB_val dkey;

  struct HcacheLmdbCtx *ctx = vctx;

  dkey.mv_data = (void *) key;
  dkey.mv_size = keylen;
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
 * hcache_lmdb_close - Implements HcacheOps::close()
 */
static void hcache_lmdb_close(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct HcacheLmdbCtx *db = *ptr;

  if (db->txn && (db->txn_mode == TXN_WRITE))
  {
    mdb_txn_commit(db->txn);
    db->txn_mode = TXN_UNINITIALIZED;
    db->txn = NULL;
  }

  mdb_env_close(db->env);
  FREE(ptr);
}

/**
 * hcache_lmdb_backend - Implements HcacheOps::backend()
 */
static const char *hcache_lmdb_backend(void)
{
  return "lmdb " MDB_VERSION_STRING;
}

HCACHE_BACKEND_OPS(lmdb)
