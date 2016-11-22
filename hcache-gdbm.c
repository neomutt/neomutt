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

#ifdef HAVE_GDBM

#include "hcache-backend.h"
#include "mutt.h"
#include <gdbm.h>

static void *
hcache_gdbm_open(const char *path)
{
  int pagesize;

  if (mutt_atoi (HeaderCachePageSize, &pagesize) < 0 || pagesize <= 0)
    pagesize = 16384;

  GDBM_FILE db = gdbm_open((char *) path, pagesize, GDBM_WRCREAT, 00600, NULL);
  if (db)
    return db;

  /* if rw failed try ro */
  return gdbm_open((char *) path, pagesize, GDBM_READER, 00600, NULL);
}

static void *
hcache_gdbm_fetch(void *ctx, const char *key, size_t keylen)
{
  datum dkey;
  datum data;

  if (!ctx)
    return NULL;

  GDBM_FILE db = ctx;

  dkey.dptr = (char *)key;
  dkey.dsize = keylen;
  data = gdbm_fetch(db, dkey);
  return data.dptr;
}

static void
hcache_gdbm_free(void *vctx, void **data)
{
    FREE(data);
}

static int
hcache_gdbm_store(void *ctx, const char *key, size_t keylen, void *data, size_t dlen)
{
  datum dkey;
  datum databuf;

  if (!ctx)
    return -1;

  GDBM_FILE db = ctx;

  dkey.dptr = (char *)key;
  dkey.dsize = keylen;

  databuf.dsize = dlen;
  databuf.dptr = data;

  return gdbm_store(db, dkey, databuf, GDBM_REPLACE);
}

static int
hcache_gdbm_delete(void *ctx, const char *key, size_t keylen)
{
  datum dkey;

  if (!ctx)
    return -1;

  GDBM_FILE db = ctx;

  dkey.dptr = (char *)key;
  dkey.dsize = keylen;

  return gdbm_delete(db, dkey);
}

static void
hcache_gdbm_close(void **ctx)
{
  if (!ctx)
    return;

  GDBM_FILE db = *ctx;
  gdbm_close(db);
}

static const char *
hcache_gdbm_backend(void)
{
  return gdbm_version;
}

HCACHE_BACKEND_OPS(gdbm)

#endif /* HAVE_GDBM */
