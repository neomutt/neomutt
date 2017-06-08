/**
 * Copyright (C) 2004 Thomas Glanzmann <sithglan@stud.uni-erlangen.de>
 * Copyright (C) 2004 Tobias Werth <sitowert@stud.uni-erlangen.de>
 * Copyright (C) 2004 Brian Fundakowski Feldman <green@FreeBSD.org>
 * Copyright (C) 2016 Pietro Cerutti <gahr@gahr.ch>
 *
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

#include "config.h"
#include <stddef.h>
#include <errno.h>
#include <lmdb.h>
#include "backend.h"
#include "lib.h"

/* The maximum size of the database file (2GiB).
 * The file is mmap(2)'d into memory. */
static size_t LMDB_DB_SIZE = 2147483648;

enum mdb_txn_mode
{
  txn_uninitialized,
  txn_read,
  txn_write
};

struct HcacheLmdbCtx
{
  MDB_env *env;
  MDB_txn *txn;
  MDB_dbi db;
  enum mdb_txn_mode txn_mode;
};

static int mdb_get_r_txn(struct HcacheLmdbCtx *ctx)
{
  int rc;

  if (ctx->txn && (ctx->txn_mode == txn_read || ctx->txn_mode == txn_write))
    return MDB_SUCCESS;

  if (ctx->txn)
    rc = mdb_txn_renew(ctx->txn);
  else
    rc = mdb_txn_begin(ctx->env, NULL, MDB_RDONLY, &ctx->txn);

  if (rc == MDB_SUCCESS)
    ctx->txn_mode = txn_read;
  else
    mutt_debug(2, "mdb_get_r_txn: %s: %s\n",
               ctx->txn ? "mdb_txn_renew" : "mdb_txn_begin", mdb_strerror(rc));

  return rc;
}

static int mdb_get_w_txn(struct HcacheLmdbCtx *ctx)
{
  int rc;

  if (ctx->txn)
  {
    if (ctx->txn_mode == txn_write)
      return MDB_SUCCESS;

    /* Free up the memory for readonly or reset transactions */
    mdb_txn_abort(ctx->txn);
  }

  rc = mdb_txn_begin(ctx->env, NULL, 0, &ctx->txn);
  if (rc == MDB_SUCCESS)
    ctx->txn_mode = txn_write;
  else
    mutt_debug(2, "mdb_get_w_txn: mdb_txn_begin: %s\n", mdb_strerror(rc));

  return rc;
}

static void *hcache_lmdb_open(const char *path)
{
  int rc;

  struct HcacheLmdbCtx *ctx = safe_malloc(sizeof(struct HcacheLmdbCtx));
  ctx->txn = NULL;
  ctx->db = 0;

  rc = mdb_env_create(&ctx->env);
  if (rc != MDB_SUCCESS)
  {
    mutt_debug(2, "hcache_open_lmdb: mdb_env_create: %s\n", mdb_strerror(rc));
    FREE(&ctx);
    return NULL;
  }

  rc = ENOMEM;
  while (LMDB_DB_SIZE)
  {
    mdb_env_set_mapsize(ctx->env, LMDB_DB_SIZE);
    rc = mdb_env_open(ctx->env, path, MDB_NOSUBDIR, 0644);
    if (rc != ENOMEM)
      break;

    LMDB_DB_SIZE >>= 1;
    mutt_debug(2, "hcache_open_lmdb: reducing dbsize to %zu", LMDB_DB_SIZE);
  }

  if (rc != MDB_SUCCESS)
  {
    mutt_debug(2, "hcache_open_lmdb: mdb_env_open: %s\n", mdb_strerror(rc));
    goto fail_env;
  }

  rc = mdb_get_r_txn(ctx);
  if (rc != MDB_SUCCESS)
  {
    mutt_debug(2, "hcache_open_lmdb: mdb_txn_begin: %s\n", mdb_strerror(rc));
    goto fail_env;
  }

  rc = mdb_dbi_open(ctx->txn, NULL, MDB_CREATE, &ctx->db);
  if (rc != MDB_SUCCESS)
  {
    mutt_debug(2, "hcache_open_lmdb: mdb_dbi_open: %s\n", mdb_strerror(rc));
    goto fail_dbi;
  }

  mdb_txn_reset(ctx->txn);
  ctx->txn_mode = txn_uninitialized;
  return ctx;

fail_dbi:
  mdb_txn_abort(ctx->txn);
  ctx->txn_mode = txn_uninitialized;
  ctx->txn = NULL;

fail_env:
  mdb_env_close(ctx->env);
  FREE(&ctx);
  return NULL;
}

static void *hcache_lmdb_fetch(void *vctx, const char *key, size_t keylen)
{
  MDB_val dkey;
  MDB_val data;
  int rc;

  if (!vctx)
    return NULL;

  struct HcacheLmdbCtx *ctx = vctx;

  dkey.mv_data = (void *) key;
  dkey.mv_size = keylen;
  data.mv_data = NULL;
  data.mv_size = 0;
  rc = mdb_get_r_txn(ctx);
  if (rc != MDB_SUCCESS)
  {
    ctx->txn = NULL;
    mutt_debug(2, "hcache_lmdb_fetch: txn_renew: %s\n", mdb_strerror(rc));
    return NULL;
  }
  rc = mdb_get(ctx->txn, ctx->db, &dkey, &data);
  if (rc == MDB_NOTFOUND)
  {
    return NULL;
  }
  if (rc != MDB_SUCCESS)
  {
    mutt_debug(2, "hcache_lmdb_fetch: mdb_get: %s\n", mdb_strerror(rc));
    return NULL;
  }

  return data.mv_data;
}

static void hcache_lmdb_free(void *vctx, void **data)
{
  /* LMDB data is owned by the database */
}

static int hcache_lmdb_store(void *vctx, const char *key, size_t keylen, void *data, size_t dlen)
{
  MDB_val dkey;
  MDB_val databuf;
  int rc;

  if (!vctx)
    return -1;

  struct HcacheLmdbCtx *ctx = vctx;

  dkey.mv_data = (void *) key;
  dkey.mv_size = keylen;
  databuf.mv_data = data;
  databuf.mv_size = dlen;
  rc = mdb_get_w_txn(ctx);
  if (rc != MDB_SUCCESS)
  {
    mutt_debug(2, "hcache_lmdb_store: mdb_get_w_txn: %s\n", mdb_strerror(rc));
    return rc;
  }
  rc = mdb_put(ctx->txn, ctx->db, &dkey, &databuf, 0);
  if (rc != MDB_SUCCESS)
  {
    mutt_debug(2, "hcahce_lmdb_store: mdb_put: %s\n", mdb_strerror(rc));
    mdb_txn_abort(ctx->txn);
    ctx->txn_mode = txn_uninitialized;
    ctx->txn = NULL;
    return rc;
  }
  return rc;
}

static int hcache_lmdb_delete(void *vctx, const char *key, size_t keylen)
{
  MDB_val dkey;
  int rc;

  if (!vctx)
    return -1;

  struct HcacheLmdbCtx *ctx = vctx;

  dkey.mv_data = (void *) key;
  dkey.mv_size = keylen;
  rc = mdb_get_w_txn(ctx);
  if (rc != MDB_SUCCESS)
  {
    mutt_debug(2, "hcache_lmdb_delete: mdb_get_w_txn: %s\n", mdb_strerror(rc));
    return rc;
  }
  rc = mdb_del(ctx->txn, ctx->db, &dkey, NULL);
  if (rc != MDB_SUCCESS)
  {
    if (rc != MDB_NOTFOUND)
    {
      mutt_debug(2, "hcache_lmdb_delete: mdb_del: %s\n", mdb_strerror(rc));
      mdb_txn_abort(ctx->txn);
      ctx->txn_mode = txn_uninitialized;
      ctx->txn = NULL;
    }
    return rc;
  }

  return rc;
}

static void hcache_lmdb_close(void **vctx)
{
  if (!vctx || !*vctx)
    return;

  struct HcacheLmdbCtx *ctx = *vctx;

  if (ctx->txn && ctx->txn_mode == txn_write)
  {
    mdb_txn_commit(ctx->txn);
    ctx->txn_mode = txn_uninitialized;
    ctx->txn = NULL;
  }

  mdb_env_close(ctx->env);
  FREE(vctx);
}

static const char *hcache_lmdb_backend(void)
{
  return "lmdb " MDB_VERSION_STRING;
}

HCACHE_BACKEND_OPS(lmdb)
