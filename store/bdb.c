/**
 * @file
 * Berkeley DB backend for the key/value Store
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
 * @page store_bdb Berkeley DB (BDB)
 *
 * Berkeley DB backend for the key/value Store.
 * https://en.wikipedia.org/wiki/Berkeley_DB
 */

#include "config.h"
#include <db.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "lib.h"

/**
 * struct BdbStoreData - Berkeley DB Store
 */
struct BdbStoreData
{
  DB_ENV *env;
  DB *db;
  int fd;
  struct Buffer lockfile;
};

/**
 * bdb_sdata_free - Free Bdb Store Data
 * @param ptr Bdb Store Data to free
 */
static void bdb_sdata_free(struct BdbStoreData **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct BdbStoreData *sdata = *ptr;
  buf_dealloc(&sdata->lockfile);

  FREE(ptr);
}

/**
 * bdb_sdata_new - Create new Bdb Store Data
 * @retval ptr New Bdb Store Data
 */
static struct BdbStoreData *bdb_sdata_new(void)
{
  struct BdbStoreData *sdata = mutt_mem_calloc(1, sizeof(struct BdbStoreData));

  sdata->lockfile = buf_make(128);

  return sdata;
}

/**
 * dbt_init - Initialise a BDB thing
 * @param dbt  Thing to initialise
 * @param data ID string to associate
 * @param len  Length of ID string
 */
static void dbt_init(DBT *dbt, void *data, size_t len)
{
  dbt->data = data;
  dbt->size = len;
  dbt->ulen = len;
  dbt->dlen = 0;
  dbt->doff = 0;
  dbt->flags = DB_DBT_USERMEM;
}

/**
 * dbt_empty_init - Initialise an empty BDB thing
 * @param dbt  Thing to initialise
 */
static void dbt_empty_init(DBT *dbt)
{
  dbt->data = NULL;
  dbt->size = 0;
  dbt->ulen = 0;
  dbt->dlen = 0;
  dbt->doff = 0;
  dbt->flags = 0;
}

/**
 * store_bdb_open - Open a connection to a Store - Implements StoreOps::open() - @ingroup store_open
 */
static StoreHandle *store_bdb_open(const char *path)
{
  if (!path)
    return NULL;

  struct BdbStoreData *sdata = bdb_sdata_new();

  const int pagesize = 512;

  buf_printf(&sdata->lockfile, "%s-lock-hack", path);

  sdata->fd = open(buf_string(&sdata->lockfile), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
  if (sdata->fd < 0)
  {
    FREE(&sdata);
    return NULL;
  }

  if (mutt_file_lock(sdata->fd, true, true))
    goto fail_close;

  int rc = db_env_create(&sdata->env, 0);
  if (rc)
    goto fail_unlock;

  rc = (*sdata->env->open)(sdata->env, NULL, DB_INIT_MPOOL | DB_CREATE | DB_PRIVATE, 0600);
  if (rc)
    goto fail_env;

  sdata->db = NULL;
  rc = db_create(&sdata->db, sdata->env, 0);
  if (rc)
    goto fail_env;

  uint32_t createflags = DB_CREATE;
  struct stat st = { 0 };

  if ((stat(path, &st) != 0) && (errno == ENOENT))
  {
    createflags |= DB_EXCL;
    sdata->db->set_pagesize(sdata->db, pagesize);
  }

  rc = (*sdata->db->open)(sdata->db, NULL, path, NULL, DB_BTREE, createflags, 0600);
  if (rc)
    goto fail_db;

  // Return an opaque pointer
  return (StoreHandle *) sdata;

fail_db:
  sdata->db->close(sdata->db, 0);
fail_env:
  sdata->env->close(sdata->env, 0);
fail_unlock:
  mutt_file_unlock(sdata->fd);
fail_close:
  close(sdata->fd);
  unlink(buf_string(&sdata->lockfile));
  bdb_sdata_free(&sdata);

  return NULL;
}

/**
 * store_bdb_fetch - Fetch a Value from the Store - Implements StoreOps::fetch() - @ingroup store_fetch
 */
static void *store_bdb_fetch(StoreHandle *store, const char *key, size_t klen, size_t *vlen)
{
  if (!store)
    return NULL;

  // Decloak an opaque pointer
  struct BdbStoreData *sdata = store;

  DBT dkey = { 0 };
  DBT data = { 0 };

  dbt_init(&dkey, (void *) key, klen);
  dbt_empty_init(&data);
  data.flags = DB_DBT_MALLOC;

  sdata->db->get(sdata->db, NULL, &dkey, &data, 0);

  *vlen = data.size;
  return data.data;
}

/**
 * store_bdb_free - Free a Value returned by fetch() - Implements StoreOps::free() - @ingroup store_free
 */
static void store_bdb_free(StoreHandle *store, void **ptr)
{
  FREE(ptr);
}

/**
 * store_bdb_store - Write a Value to the Store - Implements StoreOps::store() - @ingroup store_store
 */
static int store_bdb_store(StoreHandle *store, const char *key, size_t klen,
                           void *value, size_t vlen)
{
  if (!store)
    return -1;

  // Decloak an opaque pointer
  struct BdbStoreData *sdata = store;

  DBT dkey = { 0 };
  DBT databuf = { 0 };

  dbt_init(&dkey, (void *) key, klen);
  dbt_empty_init(&databuf);
  databuf.flags = DB_DBT_USERMEM;
  databuf.data = value;
  databuf.size = vlen;
  databuf.ulen = vlen;

  return sdata->db->put(sdata->db, NULL, &dkey, &databuf, 0);
}

/**
 * store_bdb_delete_record - Delete a record from the Store - Implements StoreOps::delete_record() - @ingroup store_delete_record
 */
static int store_bdb_delete_record(StoreHandle *store, const char *key, size_t klen)
{
  if (!store)
    return -1;

  // Decloak an opaque pointer
  struct BdbStoreData *sdata = store;

  DBT dkey = { 0 };
  dbt_init(&dkey, (void *) key, klen);
  return sdata->db->del(sdata->db, NULL, &dkey, 0);
}

/**
 * store_bdb_close - Close a Store connection - Implements StoreOps::close() - @ingroup store_close
 */
static void store_bdb_close(StoreHandle **ptr)
{
  if (!ptr || !*ptr)
    return;

  // Decloak an opaque pointer
  struct BdbStoreData *sdata = *ptr;

  sdata->db->close(sdata->db, 0);
  sdata->env->close(sdata->env, 0);
  mutt_file_unlock(sdata->fd);
  close(sdata->fd);
  unlink(buf_string(&sdata->lockfile));

  bdb_sdata_free((struct BdbStoreData **) ptr);
}

/**
 * store_bdb_version - Get a Store version string - Implements StoreOps::version() - @ingroup store_version
 */
static const char *store_bdb_version(void)
{
  return DB_VERSION_STRING;
}

STORE_BACKEND_OPS(bdb)
