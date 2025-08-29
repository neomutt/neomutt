/**
 * @file
 * Kyoto Cabinet backend for the key/value Store
 *
 * @authors
 * Copyright (C) 2016-2017 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
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
 * @page store_kc Kyoto Cabinet
 *
 * Kyoto Cabinet backend for the key/value Store.
 * https://dbmx.net/kyotocabinet/
 */

#include "config.h"
#include <kclangc.h>
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "lib.h"

/**
 * store_kyotocabinet_open - Open a connection to a Store - Implements StoreOps::open() - @ingroup store_open
 */
static StoreHandle *store_kyotocabinet_open(const char *path, bool create)
{
  if (!path)
    return NULL;

  KCDB *db = kcdbnew();
  if (!db)
    return NULL;

  struct Buffer *kcdbpath = buf_pool_get();

  buf_printf(kcdbpath, "%s#type=kct#opts=l#rcomp=lex", path);

  if (!kcdbopen(db, buf_string(kcdbpath), KCOWRITER | (create ? KCOCREATE : 0)))
  {
    int ecode = kcdbecode(db);
    mutt_debug(LL_DEBUG2, "kcdbopen failed for %s: %s (ecode %d)\n",
               buf_string(kcdbpath), kcdbemsg(db), ecode);
    kcdbdel(db);
    db = NULL;
  }

  buf_pool_release(&kcdbpath);
  // Return an opaque pointer
  return (StoreHandle *) db;
}

/**
 * store_kyotocabinet_fetch - Fetch a Value from the Store - Implements StoreOps::fetch() - @ingroup store_fetch
 */
static void *store_kyotocabinet_fetch(StoreHandle *store, const char *key,
                                      size_t klen, size_t *vlen)
{
  if (!store)
    return NULL;

  // Decloak an opaque pointer
  KCDB *db = store;
  return kcdbget(db, key, klen, vlen);
}

/**
 * store_kyotocabinet_free - Free a Value returned by fetch() - Implements StoreOps::free() - @ingroup store_free
 */
static void store_kyotocabinet_free(StoreHandle *store, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  kcfree(*ptr);
  *ptr = NULL;
}

/**
 * store_kyotocabinet_store - Write a Value to the Store - Implements StoreOps::store() - @ingroup store_store
 */
static int store_kyotocabinet_store(StoreHandle *store, const char *key,
                                    size_t klen, void *value, size_t vlen)
{
  if (!store)
    return -1;

  // Decloak an opaque pointer
  KCDB *db = store;
  if (!kcdbset(db, key, klen, value, vlen))
  {
    int ecode = kcdbecode(db);
    return ecode ? ecode : -1;
  }
  return 0;
}

/**
 * store_kyotocabinet_delete_record - Delete a record from the Store - Implements StoreOps::delete_record() - @ingroup store_delete_record
 */
static int store_kyotocabinet_delete_record(StoreHandle *store, const char *key, size_t klen)
{
  if (!store)
    return -1;

  // Decloak an opaque pointer
  KCDB *db = store;
  if (!kcdbremove(db, key, klen))
  {
    int ecode = kcdbecode(db);
    return ecode ? ecode : -1;
  }
  return 0;
}

/**
 * store_kyotocabinet_close - Close a Store connection - Implements StoreOps::close() - @ingroup store_close
 */
static void store_kyotocabinet_close(StoreHandle **ptr)
{
  if (!ptr || !*ptr)
    return;

  // Decloak an opaque pointer
  KCDB *db = *ptr;
  if (!kcdbclose(db))
  {
    int ecode = kcdbecode(db);
    mutt_debug(LL_DEBUG2, "kcdbclose failed: %s (ecode %d)\n", kcdbemsg(db), ecode);
  }
  kcdbdel(db);
  *ptr = NULL;
}

/**
 * store_kyotocabinet_version - Get a Store version string - Implements StoreOps::version() - @ingroup store_version
 */
static const char *store_kyotocabinet_version(void)
{
  static char version_cache[128] = { 0 }; ///< should be more than enough for KCVERSION
  if (version_cache[0] == '\0')
    snprintf(version_cache, sizeof(version_cache), "kyotocabinet %s", KCVERSION);

  return version_cache;
}

STORE_BACKEND_OPS(kyotocabinet)
