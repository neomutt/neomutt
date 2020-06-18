/**
 * @file
 * Tokyo Cabinet backend for the key/value Store
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
 * @page store_tc Tokyo Cabinet
 *
 * Tokyo Cabinet backend for the key/value Store.
 * https://fallabs.com/tokyocabinet/
 */

#include "config.h"
#include <stddef.h>
#include <tcbdb.h>
#include <tcutil.h>
#include "mutt/lib.h"
#include "lib.h"
#include "mutt_globals.h"

/**
 * store_tokyocabinet_open - Implements StoreOps::open()
 */
static void *store_tokyocabinet_open(const char *path)
{
  if (!path)
    return NULL;

  TCBDB *db = tcbdbnew();
  if (!db)
    return NULL;
  if (tcbdbopen(db, path, BDBOWRITER | BDBOCREAT))
    return db;

  int ecode = tcbdbecode(db);
  mutt_debug(LL_DEBUG2, "tcbdbopen failed for %s: %s (ecode %d)\n", path,
             tcbdberrmsg(ecode), ecode);
  tcbdbdel(db);
  return NULL;
}

/**
 * store_tokyocabinet_fetch - Implements StoreOps::fetch()
 */
static void *store_tokyocabinet_fetch(void *store, const char *key, size_t klen, size_t *vlen)
{
  if (!store)
    return NULL;

  int sp = 0;
  TCBDB *db = store;
  void *rv = tcbdbget(db, key, klen, &sp);
  *vlen = sp;
  return rv;
}

/**
 * store_tokyocabinet_free - Implements StoreOps::free()
 */
static void store_tokyocabinet_free(void *store, void **ptr)
{
  FREE(ptr);
}

/**
 * store_tokyocabinet_store - Implements StoreOps::store()
 */
static int store_tokyocabinet_store(void *store, const char *key, size_t klen,
                                    void *value, size_t vlen)
{
  if (!store)
    return -1;

  TCBDB *db = store;
  if (!tcbdbput(db, key, klen, value, vlen))
  {
    int ecode = tcbdbecode(db);
    return ecode ? ecode : -1;
  }
  return 0;
}

/**
 * store_tokyocabinet_delete_record - Implements StoreOps::delete_record()
 */
static int store_tokyocabinet_delete_record(void *store, const char *key, size_t klen)
{
  if (!store)
    return -1;

  TCBDB *db = store;
  if (!tcbdbout(db, key, klen))
  {
    int ecode = tcbdbecode(db);
    return ecode ? ecode : -1;
  }
  return 0;
}

/**
 * store_tokyocabinet_close - Implements StoreOps::close()
 */
static void store_tokyocabinet_close(void **ptr)
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
 * store_tokyocabinet_version - Implements StoreOps::version()
 */
static const char *store_tokyocabinet_version(void)
{
  return "tokyocabinet " _TC_VERSION;
}

STORE_BACKEND_OPS(tokyocabinet)
