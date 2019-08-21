/**
 * @file
 * Tokyocabinet DB backend for the header cache
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
 * @page hc_tc Tokyo Cabinet
 *
 * Use a a Tokyo Cabinet file as a header cache backend.
 */

#include "config.h"
#include <stddef.h>
#include <tcbdb.h>
#include <tcutil.h>
#include "mutt/mutt.h"
#include "backend.h"
#include "globals.h"

/**
 * hcache_tokyocabinet_open - Implements HcacheOps::open()
 */
static void *hcache_tokyocabinet_open(const char *path)
{
  TCBDB *db = tcbdbnew();
  if (!db)
    return NULL;
  if (C_HeaderCacheCompress)
    tcbdbtune(db, 0, 0, 0, -1, -1, BDBTDEFLATE);
  if (tcbdbopen(db, path, BDBOWRITER | BDBOCREAT))
    return db;
  else
  {
    int ecode = tcbdbecode(db);
    mutt_debug(LL_DEBUG2, "tcbdbopen failed for %s: %s (ecode %d)\n", path,
               tcbdberrmsg(ecode), ecode);
    tcbdbdel(db);
    return NULL;
  }
}

/**
 * hcache_tokyocabinet_fetch - Implements HcacheOps::fetch()
 */
static void *hcache_tokyocabinet_fetch(void *ctx, const char *key, size_t keylen)
{
  if (!ctx)
    return NULL;

  int sp = 0;
  TCBDB *db = ctx;
  return tcbdbget(db, key, keylen, &sp);
}

/**
 * hcache_tokyocabinet_free - Implements HcacheOps::free()
 */
static void hcache_tokyocabinet_free(void *ctx, void **data)
{
  FREE(data);
}

/**
 * hcache_tokyocabinet_store - Implements HcacheOps::store()
 */
static int hcache_tokyocabinet_store(void *ctx, const char *key, size_t keylen,
                                     void *data, size_t dlen)
{
  if (!ctx)
    return -1;

  TCBDB *db = ctx;
  if (!tcbdbput(db, key, keylen, data, dlen))
  {
    int ecode = tcbdbecode(db);
    return ecode ? ecode : -1;
  }
  return 0;
}

/**
 * hcache_tokyocabinet_delete_header - Implements HcacheOps::delete_header()
 */
static int hcache_tokyocabinet_delete_header(void *ctx, const char *key, size_t keylen)
{
  if (!ctx)
    return -1;

  TCBDB *db = ctx;
  if (!tcbdbout(db, key, keylen))
  {
    int ecode = tcbdbecode(db);
    return ecode ? ecode : -1;
  }
  return 0;
}

/**
 * hcache_tokyocabinet_close - Implements HcacheOps::close()
 */
static void hcache_tokyocabinet_close(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  TCBDB *db = *ptr;
  if (!tcbdbclose(db))
  {
    int ecode = tcbdbecode(db);
    mutt_debug(LL_DEBUG2, "tcbdbclose failed: %s (ecode %d)\n", tcbdberrmsg(ecode), ecode);
  }
  tcbdbdel(db);
  *ptr = NULL;
}

/**
 * hcache_tokyocabinet_backend - Implements HcacheOps::backend()
 */
static const char *hcache_tokyocabinet_backend(void)
{
  return "tokyocabinet " _TC_VERSION;
}

HCACHE_BACKEND_OPS(tokyocabinet)
