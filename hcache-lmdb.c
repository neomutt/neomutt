/*
 * Copyright (C) 2004 Thomas Glanzmann <sithglan@stud.uni-erlangen.de>
 * Copyright (C) 2004 Tobias Werth <sitowert@stud.uni-erlangen.de>
 * Copyright (C) 2004 Brian Fundakowski Feldman <green@FreeBSD.org>
 * Copyright (C) 2016 Pietro Cerutti <gahr@gahr.ch>
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"

#ifdef HAVE_LMDB

#include "hcache-backend.h"
#include "lib.h"

#define LMDB_DB_SIZE (1024 * 1024 * 1024)
#include <lmdb.h>

enum mdb_txn_mode
{
  txn_uninitialized = 0,
  txn_read          = 1 << 0,
  txn_write         = 1 << 1
};

typedef struct
{
  MDB_env *env;
  MDB_txn *txn;
  MDB_dbi db;
  enum mdb_txn_mode txn_mode;
} hcache_lmdb_ctx_t;

static int
mdb_get_r_txn(hcache_lmdb_ctx_t *ctx)
{
  int rc;

  if (ctx->txn && (ctx->txn_mode & (txn_read | txn_write)) > 0)
    return MDB_SUCCESS;

  if (ctx->txn)
    rc = mdb_txn_renew(ctx->txn);
  else
    rc = mdb_txn_begin(ctx->env, NULL, MDB_RDONLY, &ctx->txn);

  if (rc == MDB_SUCCESS)
    ctx->txn_mode = txn_read;

  return rc;
}

static int
mdb_get_w_txn(hcache_lmdb_ctx_t *ctx)
{
  int rc;

  if (ctx->txn && (ctx->txn_mode == txn_write))
    return MDB_SUCCESS;

  if (ctx->txn)
  {
    if (ctx->txn_mode == txn_read)
      mdb_txn_reset(ctx->txn);
    ctx->txn = NULL;
  }

  rc = mdb_txn_begin(ctx->env, NULL, 0, &ctx->txn);
  if (rc == MDB_SUCCESS)
    ctx->txn_mode = txn_write;

  return rc;
}

static void *
hcache_lmdb_open(const char *path)
{
  int rc;

  hcache_lmdb_ctx_t *ctx = safe_malloc(sizeof(hcache_lmdb_ctx_t));
  ctx->txn = NULL;
  ctx->db  = 0;

  rc = mdb_env_create(&ctx->env);
  if (rc != MDB_SUCCESS)
  {
    fprintf(stderr, "hcache_open_lmdb: mdb_env_create: %s", mdb_strerror(rc));
    return NULL;
  }

  mdb_env_set_mapsize(ctx->env, LMDB_DB_SIZE);

  rc = mdb_env_open(ctx->env, path, MDB_NOSUBDIR, 0644);
  if (rc != MDB_SUCCESS)
  {
    fprintf(stderr, "hcache_open_lmdb: mdb_env_open: %s", mdb_strerror(rc));
    goto fail_env;
  }

  rc = mdb_get_r_txn(ctx);
  if (rc != MDB_SUCCESS)
  {
      fprintf(stderr, "hcache_open_lmdb: mdb_txn_begin: %s", mdb_strerror(rc));
      goto fail_env;
  }

  rc = mdb_dbi_open(ctx->txn, NULL, MDB_CREATE, &ctx->db);
  if (rc != MDB_SUCCESS)
  {
    fprintf(stderr, "hcache_open_lmdb: mdb_dbi_open: %s", mdb_strerror(rc));
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

static void *
hcache_lmdb_fetch(void *vctx, const char *key, size_t keylen)
{
  MDB_val dkey;
  MDB_val data;
  int rc;

  if (!vctx)
      return NULL;

  hcache_lmdb_ctx_t *ctx = vctx;

  dkey.mv_data = (void *)key;
  dkey.mv_size = keylen;
  data.mv_data = NULL;
  data.mv_size = 0;
  rc = mdb_get_r_txn(ctx);
  if (rc != MDB_SUCCESS)
  {
    ctx->txn = NULL;
    fprintf(stderr, "txn_renew: %s\n", mdb_strerror(rc));
    return NULL;
  }
  rc = mdb_get(ctx->txn, ctx->db, &dkey, &data);
  if (rc == MDB_NOTFOUND)
  {
    return NULL;
  }
  if (rc != MDB_SUCCESS)
  {
    fprintf(stderr, "mdb_get: %s\n", mdb_strerror(rc));
    return NULL;
  }

  return data.mv_data;
}

static void
hcache_lmdb_free(void *vctx, void **data)
{
  /* LMDB data is owned by the database */
}

static int
hcache_lmdb_store(void *vctx, const char *key, size_t keylen, void *data, size_t dlen)
{
  MDB_val dkey;
  MDB_val databuf;
  int rc;

  if (!vctx)
    return -1;

  hcache_lmdb_ctx_t *ctx = vctx;

  dkey.mv_data = (void *)key;
  dkey.mv_size = keylen;
  databuf.mv_data = data;
  databuf.mv_size = dlen;
  rc = mdb_get_w_txn(ctx);
  if (rc != MDB_SUCCESS)
  {
    fprintf(stderr, "txn_begin: %s\n", mdb_strerror(rc));
    return rc;
  }
  rc = mdb_put(ctx->txn, ctx->db, &dkey, &databuf, 0);
  if (rc != MDB_SUCCESS)
  {
    fprintf(stderr, "mdb_put: %s\n", mdb_strerror(rc));
    mdb_txn_abort(ctx->txn);
    ctx->txn_mode = txn_uninitialized;
    ctx->txn = NULL;
    return rc;
  }
  return rc;
}

static int
hcache_lmdb_delete(void *vctx, const char *key, size_t keylen)
{
  MDB_val dkey;
  int rc;

  if (!vctx)
    return -1;

  hcache_lmdb_ctx_t *ctx = vctx;

  dkey.mv_data = (void *)key;
  dkey.mv_size = keylen;
  rc = mdb_get_w_txn(ctx);
  if (rc != MDB_SUCCESS)
  {
    fprintf(stderr, "txn_begin: %s\n", mdb_strerror(rc));
    return rc;
  }
  rc = mdb_del(ctx->txn, ctx->db, &dkey, NULL);
  if (rc != MDB_SUCCESS)
  {
    if (rc != MDB_NOTFOUND)
    {
      fprintf(stderr, "mdb_del: %s\n", mdb_strerror(rc));
      mdb_txn_abort(ctx->txn);
      ctx->txn_mode = txn_uninitialized;
      ctx->txn = NULL;
    }
    return rc;
  }

  return rc;
}

static void
hcache_lmdb_close(void **vctx)
{
  if (!vctx || !*vctx)
    return;

  hcache_lmdb_ctx_t *ctx = *vctx;

  if (ctx->txn && ctx->txn_mode == txn_write)
  {
    mdb_txn_commit(ctx->txn);
    ctx->txn_mode = txn_uninitialized;
    ctx->txn = NULL;
  }

  mdb_env_close(ctx->env);
  FREE(vctx); /* __FREE_CHECKED__ */
}

static const char *
hcache_lmdb_backend(void)
{
  return "lmdb " MDB_VERSION_STRING;
}

HCACHE_BACKEND_OPS(lmdb)

#endif /* HAVE_LMDB */
