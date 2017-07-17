/**
 * @file
 * Kyotocabinet DB backend for the header cache
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
#include <kclangc.h>
#include <limits.h>
#include <stdio.h>
#include "backend.h"
#include "lib.h"
#include "options.h"

static void *hcache_kyotocabinet_open(const char *path)
{
  char kcdbpath[_POSIX_PATH_MAX];
  int printfresult;

  printfresult =
      snprintf(kcdbpath, sizeof(kcdbpath), "%s#type=kct#opts=%s#rcomp=lex",
               path, option(OPTHCACHECOMPRESS) ? "lc" : "l");
  if ((printfresult < 0) || (printfresult >= sizeof(kcdbpath)))
  {
    return NULL;
  }

  KCDB *db = kcdbnew();
  if (!db)
    return NULL;

  if (kcdbopen(db, kcdbpath, KCOWRITER | KCOCREATE))
    return db;
  else
  {
#ifdef DEBUG
    int ecode = kcdbecode(db);
    mutt_debug(2, "kcdbopen failed for %s: %s (ecode %d)\n", kcdbpath, kcdbemsg(db), ecode);
#endif
    kcdbdel(db);
    return NULL;
  }
}

static void *hcache_kyotocabinet_fetch(void *ctx, const char *key, size_t keylen)
{
  size_t sp;

  if (!ctx)
    return NULL;

  KCDB *db = ctx;
  return kcdbget(db, key, keylen, &sp);
}

static void hcache_kyotocabinet_free(void *vctx, void **data)
{
  kcfree(*data);
  *data = NULL;
}

static int hcache_kyotocabinet_store(void *ctx, const char *key, size_t keylen,
                                     void *data, size_t dlen)
{
  if (!ctx)
    return -1;

  KCDB *db = ctx;
  return kcdbset(db, key, keylen, data, dlen);
}

static int hcache_kyotocabinet_delete(void *ctx, const char *key, size_t keylen)
{
  if (!ctx)
    return -1;

  KCDB *db = ctx;
  return kcdbremove(db, key, keylen);
}

static void hcache_kyotocabinet_close(void **ctx)
{
  if (!ctx || !*ctx)
    return;

  KCDB *db = *ctx;
  if (!kcdbclose(db))
  {
#ifdef DEBUG
    int ecode = kcdbecode(db);
    mutt_debug(2, "kcdbclose failed: %s (ecode %d)\n", kcdbemsg(db), ecode);
#endif
  }
  kcdbdel(db);
}

static const char *hcache_kyotocabinet_backend(void)
{
  /* SHORT_STRING(128) should be more than enough for KCVERSION */
  static char version_cache[SHORT_STRING] = "";
  if (!version_cache[0])
    snprintf(version_cache, sizeof(version_cache), "kyotocabinet %s", KCVERSION);

  return version_cache;
}

HCACHE_BACKEND_OPS(kyotocabinet)
