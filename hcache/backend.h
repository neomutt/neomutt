/**
 * @file
 * API for the header cache
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

#ifndef MUTT_HCACHE_BACKEND_H
#define MUTT_HCACHE_BACKEND_H

#include <stdlib.h>

/**
 * struct HcacheOps - Header Cache API
 */
struct HcacheOps
{
  /**
   * name - Backend name
   */
  const char *name;
  /**
   * open - backend-specific routing to open the header cache database
   * @param path The path to the database file
   * @retval ptr  Success, backend-specific context
   * @retval NULL Otherwise
   *
   * The open function has the purpose of opening a backend-specific
   * connection to the database file specified by the path parameter. Backends
   * MUST return non-NULL specific context information on success. This will be
   * stored in the ctx member of the header_cache_t structure and passed on to
   * all other backend-specific functions (see below).
   */
  void *(*open)(const char *path);
  /**
   * fetch - backend-specific routine to fetch a message's headers
   * @param ctx    The backend-specific context retrieved via open()
   * @param key    A message identification string
   * @param keylen The length of the string pointed to by key
   * @retval ptr  Success, message's headers
   * @retval NULL Otherwise
   */
  void *(*fetch)(void *ctx, const char *key, size_t keylen);
  /**
   * free - backend-specific routine to free fetched data
   * @param[in]  ctx The backend-specific context retrieved via open()
   * @param[out] data A pointer to the data got with fetch() or fetch_raw()
   */
  void (*free)(void *ctx, void **data);
  /**
   * store - backend-specific routine to store a message's headers
   * @param ctx     The backend-specific context retrieved via open()
   * @param key     A message identification string
   * @param keylen  The length of the string pointed to by key
   * @param data    The message headers data
   * @param datalen The length of the string pointed to by data
   * @retval 0   Success
   * @retval num Error, a backend-specific error code
   */
  int (*store)(void *ctx, const char *key, size_t keylen, void *data, size_t datalen);
  /**
   * delete_header - backend-specific routine to delete a message's headers
   * @param ctx    The backend-specific context retrieved via open()
   * @param key    A message identification string
   * @param keylen The length of the string pointed to by key
   * @retval 0   Success
   * @retval num Error, a backend-specific error code
   */
  int (*delete_header)(void *ctx, const char *key, size_t keylen);
  /**
   * close - backend-specific routine to close a context
   * @param[out] ctx The backend-specific context retrieved via open()
   *
   * Backend code is responsible for freeing any resources associated with the
   * @a ctx parameter. For this reason, backend code is passed a pointer-to-pointer
   * to the context, so that FREE can be invoked on it.
   */
  void (*close)(void **ctx);
  /**
   * backend - backend-specific identification string
   * @retval ptr String describing the currently used hcache backend
   */
  const char *(*backend)(void);
};

#define HCACHE_BACKEND_OPS(_name)                                              \
  const struct HcacheOps hcache_##_name##_ops = {                              \
    .name    = #_name,                                                         \
    .open    = hcache_##_name##_open,                                          \
    .fetch   = hcache_##_name##_fetch,                                         \
    .free    = hcache_##_name##_free,                                          \
    .store   = hcache_##_name##_store,                                         \
    .delete_header  = hcache_##_name##_delete_header,                          \
    .close   = hcache_##_name##_close,                                         \
    .backend = hcache_##_name##_backend,                                       \
  };

#endif /* MUTT_HCACHE_BACKEND_H */
