/**
 * @file
 * QDBM backend for the header cache
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
 * @page hc_qdbm QDBM
 *
 * Use a Quick DataBase Manager file as a header cache backend.
 */

#include "config.h"
#include <stddef.h>
#include <depot.h>
#include <stdbool.h>
#include <villa.h>
#include "mutt/mutt.h"
#include "backend.h"
#include "globals.h"

/**
 * hcache_qdbm_open - Implements HcacheOps::open()
 */
static void *hcache_qdbm_open(const char *path)
{
  int flags = VL_OWRITER | VL_OCREAT;

  if (C_HeaderCacheCompress)
    flags |= VL_OZCOMP;

  return vlopen(path, flags, VL_CMPLEX);
}

/**
 * hcache_qdbm_fetch - Implements HcacheOps::fetch()
 */
static void *hcache_qdbm_fetch(void *ctx, const char *key, size_t keylen)
{
  if (!ctx)
    return NULL;

  VILLA *db = ctx;
  return vlget(db, key, keylen, NULL);
}

/**
 * hcache_qdbm_free - Implements HcacheOps::free()
 */
static void hcache_qdbm_free(void *ctx, void **data)
{
  FREE(data);
}

/**
 * hcache_qdbm_store - Implements HcacheOps::store()
 */
static int hcache_qdbm_store(void *ctx, const char *key, size_t keylen, void *data, size_t dlen)
{
  if (!ctx)
    return -1;

  VILLA *db = ctx;
  /* Not sure if dbecode is reset on success, so better to explicitly return 0
   * on success */
  bool success = vlput(db, key, keylen, data, dlen, VL_DOVER);
  return success ? 0 : dpecode ? dpecode : -1;
}

/**
 * hcache_qdbm_delete_header - Implements HcacheOps::delete_header()
 */
static int hcache_qdbm_delete_header(void *ctx, const char *key, size_t keylen)
{
  if (!ctx)
    return -1;

  VILLA *db = ctx;
  /* Not sure if dbecode is reset on success, so better to explicitly return 0
   * on success */
  bool success = vlout(db, key, keylen);
  return success ? 0 : dpecode ? dpecode : -1;
}

/**
 * hcache_qdbm_close - Implements HcacheOps::close()
 */
static void hcache_qdbm_close(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  VILLA *db = *ptr;
  vlclose(db);
  *ptr = NULL;
}

/**
 * hcache_qdbm_backend - Implements HcacheOps::backend()
 */
static const char *hcache_qdbm_backend(void)
{
  return "qdbm " _QDBM_VERSION;
}

HCACHE_BACKEND_OPS(qdbm)
