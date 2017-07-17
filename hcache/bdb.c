/**
 * @file
 * Berkeley DB backend for the header cache
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

#include "config.h"
#include <db.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "backend.h"
#include "globals.h"
#include "lib.h"
#include "mx.h"

/**
 * struct HcacheDbCtx - Berkeley DB context
 */
struct HcacheDbCtx
{
  DB_ENV *env;
  DB *db;
  int fd;
  char lockfile[_POSIX_PATH_MAX];
};

static void dbt_init(DBT *dbt, void *data, size_t len)
{
  dbt->data = data;
  dbt->size = dbt->ulen = len;
  dbt->dlen = dbt->doff = 0;
  dbt->flags = DB_DBT_USERMEM;
}

static void dbt_empty_init(DBT *dbt)
{
  dbt->data = NULL;
  dbt->size = dbt->ulen = dbt->dlen = dbt->doff = 0;
  dbt->flags = 0;
}

static void *hcache_bdb_open(const char *path)
{
  struct stat sb;
  int ret;
  u_int32_t createflags = DB_CREATE;
  int pagesize;

  struct HcacheDbCtx *ctx = safe_malloc(sizeof(struct HcacheDbCtx));

  if (mutt_atoi(HeaderCachePageSize, &pagesize) < 0 || pagesize <= 0)
    pagesize = 16384;

  snprintf(ctx->lockfile, _POSIX_PATH_MAX, "%s-lock-hack", path);

  ctx->fd = open(ctx->lockfile, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
  if (ctx->fd < 0)
  {
    FREE(&ctx);
    return NULL;
  }

  if (mx_lock_file(ctx->lockfile, ctx->fd, 1, 5))
    goto fail_close;

  ret = db_env_create(&ctx->env, 0);
  if (ret)
    goto fail_unlock;

  ret = (*ctx->env->open)(ctx->env, NULL, DB_INIT_MPOOL | DB_CREATE | DB_PRIVATE, 0600);
  if (ret)
    goto fail_env;

  ctx->db = NULL;
  ret = db_create(&ctx->db, ctx->env, 0);
  if (ret)
    goto fail_env;

  if (stat(path, &sb) != 0 && errno == ENOENT)
  {
    createflags |= DB_EXCL;
    ctx->db->set_pagesize(ctx->db, pagesize);
  }

  ret = (*ctx->db->open)(ctx->db, NULL, path, NULL, DB_BTREE, createflags, 0600);
  if (ret)
    goto fail_db;

  return ctx;

fail_db:
  ctx->db->close(ctx->db, 0);
fail_env:
  ctx->env->close(ctx->env, 0);
fail_unlock:
  mx_unlock_file(ctx->lockfile, ctx->fd);
fail_close:
  close(ctx->fd);
  unlink(ctx->lockfile);
  FREE(&ctx);

  return NULL;
}

static void *hcache_bdb_fetch(void *vctx, const char *key, size_t keylen)
{
  DBT dkey;
  DBT data;

  if (!vctx)
    return NULL;

  struct HcacheDbCtx *ctx = vctx;

  dbt_init(&dkey, (void *) key, keylen);
  dbt_empty_init(&data);
  data.flags = DB_DBT_MALLOC;

  ctx->db->get(ctx->db, NULL, &dkey, &data, 0);

  return data.data;
}

static void hcache_bdb_free(void *vctx, void **data)
{
  FREE(data);
}

static int hcache_bdb_store(void *vctx, const char *key, size_t keylen, void *data, size_t dlen)
{
  DBT dkey;
  DBT databuf;

  if (!vctx)
    return -1;

  struct HcacheDbCtx *ctx = vctx;

  dbt_init(&dkey, (void *) key, keylen);
  dbt_empty_init(&databuf);
  databuf.flags = DB_DBT_USERMEM;
  databuf.data = data;
  databuf.size = dlen;
  databuf.ulen = dlen;

  return ctx->db->put(ctx->db, NULL, &dkey, &databuf, 0);
}

static int hcache_bdb_delete(void *vctx, const char *key, size_t keylen)
{
  DBT dkey;

  if (!vctx)
    return -1;

  struct HcacheDbCtx *ctx = vctx;

  dbt_init(&dkey, (void *) key, keylen);
  return ctx->db->del(ctx->db, NULL, &dkey, 0);
}

static void hcache_bdb_close(void **vctx)
{
  if (!vctx || !*vctx)
    return;

  struct HcacheDbCtx *ctx = *vctx;

  ctx->db->close(ctx->db, 0);
  ctx->env->close(ctx->env, 0);
  mx_unlock_file(ctx->lockfile, ctx->fd);
  close(ctx->fd);
  unlink(ctx->lockfile);
  FREE(vctx);
}

static const char *hcache_bdb_backend(void)
{
  return DB_VERSION_STRING;
}

HCACHE_BACKEND_OPS(bdb)
