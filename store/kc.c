/**
 * @file
 * Kyoto Cabinet backend for the key/value Store
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
 * @page store_kc Kyoto Cabinet
 *
 * Kyoto Cabinet backend for the key/value Store.
 * https://fallabs.com/kyotocabinet/
 */

#include "config.h"
#include <kclangc.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "lib.h"
#include "mutt_globals.h"

/**
 * store_kyotocabinet_open - Implements StoreOps::open()
 */
static void *store_kyotocabinet_open(const char *path)
{
  if (!path)
    return NULL;

  KCDB *db = kcdbnew();
  if (!db)
    return NULL;

  struct Buffer kcdbpath = mutt_buffer_make(1024);

  mutt_buffer_printf(&kcdbpath, "%s#type=kct#opts=l#rcomp=lex", path);

  if (!kcdbopen(db, mutt_b2s(&kcdbpath), KCOWRITER | KCOCREATE))
  {
    int ecode = kcdbecode(db);
    mutt_debug(LL_DEBUG2, "kcdbopen failed for %s: %s (ecode %d)\n",
               mutt_b2s(&kcdbpath), kcdbemsg(db), ecode);
    kcdbdel(db);
    db = NULL;
  }

  mutt_buffer_dealloc(&kcdbpath);
  return db;
}

/**
 * store_kyotocabinet_fetch - Implements StoreOps::fetch()
 */
static void *store_kyotocabinet_fetch(void *store, const char *key, size_t klen, size_t *vlen)
{
  if (!store)
    return NULL;

  KCDB *db = store;
  return kcdbget(db, key, klen, vlen);
}

/**
 * store_kyotocabinet_free - Implements StoreOps::free()
 */
static void store_kyotocabinet_free(void *store, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  kcfree(*ptr);
  *ptr = NULL;
}

/**
 * store_kyotocabinet_store - Implements StoreOps::store()
 */
static int store_kyotocabinet_store(void *store, const char *key, size_t klen,
                                    void *value, size_t vlen)
{
  if (!store)
    return -1;

  KCDB *db = store;
  if (!kcdbset(db, key, klen, value, vlen))
  {
    int ecode = kcdbecode(db);
    return ecode ? ecode : -1;
  }
  return 0;
}

/**
 * store_kyotocabinet_delete_record - Implements StoreOps::delete_record()
 */
static int store_kyotocabinet_delete_record(void *store, const char *key, size_t klen)
{
  if (!store)
    return -1;

  KCDB *db = store;
  if (!kcdbremove(db, key, klen))
  {
    int ecode = kcdbecode(db);
    return ecode ? ecode : -1;
  }
  return 0;
}

/**
 * store_kyotocabinet_close - Implements StoreOps::close()
 */
static void store_kyotocabinet_close(void **ptr)
{
  if (!ptr || !*ptr)
    return;

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
 * store_kyotocabinet_version - Implements StoreOps::version()
 */
static const char *store_kyotocabinet_version(void)
{
  static char version_cache[128] = { 0 }; ///< should be more than enough for KCVERSION
  if (version_cache[0] == '\0')
    snprintf(version_cache, sizeof(version_cache), "kyotocabinet %s", KCVERSION);

  return version_cache;
}

STORE_BACKEND_OPS(kyotocabinet)
