/**
 * @file
 * Tokyo Cabinet backend for the key/value Store
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
 * @page store_tc Tokyo Cabinet
 *
 * Tokyo Cabinet backend for the key/value Store.
 * https://dbmx.net/tokyocabinet/
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include <tcbdb.h>
#include <tcutil.h>
#include "mutt/lib.h"
#include "lib.h"

/**
 * store_tokyocabinet_open - Open a connection to a Store - Implements StoreOps::open() - @ingroup store_open
 */
static StoreHandle *store_tokyocabinet_open(const char *path, bool create)
{
  if (!path)
    return NULL;

  TCBDB *db = tcbdbnew();
  if (!db)
    return NULL;
  if (!tcbdbopen(db, path, BDBOWRITER | (create ? BDBOCREAT : 0)))
  {
    int ecode = tcbdbecode(db);
    mutt_debug(LL_DEBUG2, "tcbdbopen failed for %s: %s (ecode %d)\n", path,
               tcbdberrmsg(ecode), ecode);
    tcbdbdel(db);
    return NULL;
  }

  // Return an opaque pointer
  return (StoreHandle *) db;
}

/**
 * store_tokyocabinet_fetch - Fetch a Value from the Store - Implements StoreOps::fetch() - @ingroup store_fetch
 */
static void *store_tokyocabinet_fetch(StoreHandle *store, const char *key,
                                      size_t klen, size_t *vlen)
{
  if (!store)
    return NULL;

  // Decloak an opaque pointer
  TCBDB *db = store;
  int sp = 0;
  void *rv = tcbdbget(db, key, klen, &sp);
  *vlen = sp;
  return rv;
}

/**
 * store_tokyocabinet_free - Free a Value returned by fetch() - Implements StoreOps::free() - @ingroup store_free
 */
static void store_tokyocabinet_free(StoreHandle *store, void **ptr)
{
  FREE(ptr);
}

/**
 * store_tokyocabinet_store - Write a Value to the Store - Implements StoreOps::store() - @ingroup store_store
 */
static int store_tokyocabinet_store(StoreHandle *store, const char *key,
                                    size_t klen, void *value, size_t vlen)
{
  if (!store)
    return -1;

  // Decloak an opaque pointer
  TCBDB *db = store;
  if (!tcbdbput(db, key, klen, value, vlen))
  {
    int ecode = tcbdbecode(db);
    return ecode ? ecode : -1;
  }
  return 0;
}

/**
 * store_tokyocabinet_delete_record - Delete a record from the Store - Implements StoreOps::delete_record() - @ingroup store_delete_record
 */
static int store_tokyocabinet_delete_record(StoreHandle *store, const char *key, size_t klen)
{
  if (!store)
    return -1;

  // Decloak an opaque pointer
  TCBDB *db = store;
  if (!tcbdbout(db, key, klen))
  {
    int ecode = tcbdbecode(db);
    return ecode ? ecode : -1;
  }
  return 0;
}

/**
 * store_tokyocabinet_close - Close a Store connection - Implements StoreOps::close() - @ingroup store_close
 */
static void store_tokyocabinet_close(StoreHandle **ptr)
{
  if (!ptr || !*ptr)
    return;

  // Decloak an opaque pointer
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
 * store_tokyocabinet_version - Get a Store version string - Implements StoreOps::version() - @ingroup store_version
 */
static const char *store_tokyocabinet_version(void)
{
  return "tokyocabinet " _TC_VERSION;
}

STORE_BACKEND_OPS(tokyocabinet)
