/**
 * @file
 * Key value store
 *
 * @authors
 * Copyright (C) 2016 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page store STORE: Key value store
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
 * @subpage store_store
 *
 * | Name                   | File            | Home Page                                 |
 * | :--------------------- | :-------------- | :---------------------------------------- |
 * | @subpage store_bdb     | store/bdb.c     | https://en.wikipedia.org/wiki/Berkeley_DB |
 * | @subpage store_gdbm    | store/gdbm.c    | https://www.gnu.org.ua/software/gdbm/     |
 * | @subpage store_kc      | store/kc.c      | https://fallabs.com/kyotocabinet/         |
 * | @subpage store_lmdb    | store/lmdb.c    | https://symas.com/lmdb/                   |
 * | @subpage store_qdbm    | store/qdbm.c    | https://fallabs.com/qdbm/                 |
 * | @subpage store_rocksdb | store/rocksdb.c | https://rocksdb.org/                      |
 * | @subpage store_tc      | store/tc.c      | https://tdb.samba.org/                    |
 * | @subpage store_tdb     | store/tdb.c     | https://fallabs.com/tokyocabinet/         |
 */

#ifndef MUTT_STORE_LIB_H
#define MUTT_STORE_LIB_H

#include <stdbool.h>
#include <stdlib.h>

/**
 * struct StoreOps - Key Value Store API
 */
struct StoreOps
{
  const char *name; ///< Store name

  /**
   * open - Open a connection to a Store
   * @param[in] path Path to the database file
   * @retval ptr  Success, Store pointer
   * @retval NULL Failure
   *
   * The open function has the purpose of opening a backend-specific
   * connection to the database file specified by the path parameter. Backends
   * MUST return non-NULL specific context information on success.
   */
  void *(*open)(const char *path);

  /**
   * fetch - Fetch a Value from the Store
   * @param[in]  store Store retrieved via open()
   * @param[in]  key   Key identifying the record
   * @param[in]  klen  Length of the Key string
   * @param[out] vlen  Length of the Value
   * @retval ptr  Success, Value associated with the Key
   * @retval NULL Error, or Key not found
   */
  void *(*fetch)(void *store, const char *key, size_t klen, size_t *vlen);

  /**
   * free - Free a Value returned by fetch()
   * @param[in]  store Store retrieved via open()
   * @param[out] ptr   Value to be freed
   */
  void (*free)(void *store, void **ptr);

  /**
   * store - Write a Value to the Store
   * @param[in] store Store retrieved via open()
   * @param[in] key   Key identifying the record
   * @param[in] klen  Length of the Key string
   * @param[in] value Value to save
   * @param[in] vlen  Length of the Value
   * @retval 0   Success
   * @retval num Error, a backend-specific error code
   */
  int (*store)(void *store, const char *key, size_t klen, void *value, size_t vlen);

  /**
   * delete_record - Delete a record from the Store
   * @param[in] store Store retrieved via open()
   * @param[in] key   Key identifying the record
   * @param[in] klen  Length of the Key string
   * @retval 0   Success
   * @retval num Error, a backend-specific error code
   */
  int (*delete_record)(void *store, const char *key, size_t klen);

  /**
   * close - Close a Store connection
   * @param[in,out] ptr Store retrieved via open()
   */
  void (*close)(void **ptr);

  /**
   * version - Get a Store version string
   * @retval ptr String describing the currently used Store
   */
  const char *(*version)(void);
};

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
