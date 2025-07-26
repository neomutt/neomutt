/**
 * @file
 * Key value store
 *
 * @authors
 * Copyright (C) 2016 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020-2023 Richard Russon <rich@flatcap.org>
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
 * @page lib_store Store
 *
 * Key value store
 *
 * These databases provide Key/Value storage for NeoMutt.
 * They are used by the \ref hcache.
 *
 * @sa https://en.wikipedia.org/wiki/Key-value_database
 *
 * ## Interface
 *
 * Each Store backend implements the StoreOps API.
 *
 * ## Source
 *
 * @subpage store_module
 * @subpage store_store
 *
 * | File            | Name                   | Home Page                                 |
 * | :-------------- | :--------------------- | :---------------------------------------- |
 * | store/bdb.c     | @subpage store_bdb     | https://en.wikipedia.org/wiki/Berkeley_DB |
 * | store/gdbm.c    | @subpage store_gdbm    | https://www.gnu.org.ua/software/gdbm/     |
 * | store/kc.c      | @subpage store_kc      | https://dbmx.net/kyotocabinet/            |
 * | store/lmdb.c    | @subpage store_lmdb    | https://symas.com/lmdb/                   |
 * | store/qdbm.c    | @subpage store_qdbm    | https://dbmx.net/qdbm/                    |
 * | store/rocksdb.c | @subpage store_rocksdb | https://rocksdb.org/                      |
 * | store/tc.c      | @subpage store_tc      | https://dbmx.net/tokyocabinet/            |
 * | store/tdb.c     | @subpage store_tdb     | https://tdb.samba.org/                    |
 */

#ifndef MUTT_STORE_LIB_H
#define MUTT_STORE_LIB_H

#include <stdbool.h>
#include <stdlib.h>

/// Opaque type for store backend
typedef void StoreHandle;

/**
 * @defgroup store_api Key Value Store API
 *
 * The Key Value Store API
 */
struct StoreOps
{
  const char *name; ///< Store name

  /**
   * @defgroup store_open open()
   * @ingroup store_api
   *
   * open - Open a connection to a Store
   * @param[in] path Path to the database file
   * @param[in] create Create the file if it's not there?
   * @retval ptr  Success, Store pointer
   * @retval NULL Failure
   *
   * The open function has the purpose of opening a backend-specific
   * connection to the database file specified by the path parameter. Backends
   * MUST return non-NULL specific handle information on success.
   */
  StoreHandle *(*open)(const char *path, bool create);

  /**
   * @defgroup store_fetch fetch()
   * @ingroup store_api
   *
   * fetch - Fetch a Value from the Store
   * @param[in]  store Store retrieved via open()
   * @param[in]  key   Key identifying the record
   * @param[in]  klen  Length of the Key string
   * @param[out] vlen  Length of the Value
   * @retval ptr  Success, Value associated with the Key
   * @retval NULL Error, or Key not found
   */
  void *(*fetch)(StoreHandle *store, const char *key, size_t klen, size_t *vlen);

  /**
   * @defgroup store_free free()
   * @ingroup store_api
   *
   * free - Free a Value returned by fetch()
   * @param[in]  store Store retrieved via open()
   * @param[out] ptr   Value to be freed
   */
  void (*free)(StoreHandle *store, void **ptr);

  /**
   * @defgroup store_store store()
   * @ingroup store_api
   *
   * store - Write a Value to the Store
   * @param[in] store Store retrieved via open()
   * @param[in] key   Key identifying the record
   * @param[in] klen  Length of the Key string
   * @param[in] value Value to save
   * @param[in] vlen  Length of the Value
   * @retval 0   Success
   * @retval num Error, a backend-specific error code
   */
  int (*store)(StoreHandle *store, const char *key, size_t klen, void *value, size_t vlen);

  /**
   * @defgroup store_delete_record delete_record()
   * @ingroup store_api
   *
   * delete_record - Delete a record from the Store
   * @param[in] store Store retrieved via open()
   * @param[in] key   Key identifying the record
   * @param[in] klen  Length of the Key string
   * @retval 0   Success
   * @retval num Error, a backend-specific error code
   */
  int (*delete_record)(StoreHandle *store, const char *key, size_t klen);

  /**
   * @defgroup store_close close()
   * @ingroup store_api
   *
   * close - Close a Store connection
   * @param[in,out] ptr Store retrieved via open()
   */
  void (*close)(StoreHandle **ptr);

  /**
   * @defgroup store_version version()
   * @ingroup store_api
   *
   * version - Get a Store version string
   * @retval ptr String describing the currently used Store
   */
  const char *(*version)(void);
};

struct Slist *         store_backend_list(void);
const struct StoreOps *store_get_backend_ops(const char *str);
bool                   store_is_valid_backend(const char *str);

#define STORE_BACKEND_OPS(_name)                                               \
  const struct StoreOps store_##_name##_ops = {                                \
    .name           = #_name,                                                  \
    .open           = store_##_name##_open,                                    \
    .fetch          = store_##_name##_fetch,                                   \
    .free           = store_##_name##_free,                                    \
    .store          = store_##_name##_store,                                   \
    .delete_record  = store_##_name##_delete_record,                           \
    .close          = store_##_name##_close,                                   \
    .version        = store_##_name##_version,                                 \
  };

#endif /* MUTT_STORE_LIB_H */
