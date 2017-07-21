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

#ifndef _MUTT_HCACHE_BACKEND_H
#define _MUTT_HCACHE_BACKEND_H

#include <stdlib.h>

/**
 * hcache_open_t - backend-specific routing to open the header cache database
 * @param path The path to the database file
 * @retval Pointer to backend-specific context on success
 * @retval NULL otherwise
 *
 * The hcache_open function has the purpose of opening a backend-specific
 * connection to the database file specified by the path parameter. Backends
 * MUST return non-NULL specific context informations on success. This will be
 * stored in the ctx member of the header_cache_t structure and passed on to
 * all other backend-specific functions (see below).
 */
typedef void *(*hcache_open_t)(const char *path);

/**
 * hcache_fetch_t - backend-specific routine to fetch a message's headers
 * @param ctx    The backend-specific context retrieved via hcache_open
 * @param key    A message identification string
 * @param keylen The length of the string pointed to by key
 * @retval Pointer to the message's headers on success
 * @retval NULL otherwise
 */
typedef void *(*hcache_fetch_t)(void *ctx, const char *key, size_t keylen);

/**
 * hcache_free_t - backend-specific routine to free fetched data
 * @param ctx The backend-specific context retrieved via hcache_open
 * @param data A pointer to the data got with hcache_fetch or hcache_fetch_raw
 */
typedef void (*hcache_free_t)(void *ctx, void **data);

/**
 * hcache_store_t - backend-specific routine to store a message's headers
 * @param ctx     The backend-specific context retrieved via hcache_open
 * @param key     A message identification string
 * @param keylen  The length of the string pointed to by key
 * @param data    The message headers data
 * @param datalen The length of the string pointed to by data
 * @retval 0 on success
 * @retval a backend-specific error code otherwise
 */
typedef int (*hcache_store_t)(void *ctx, const char *key, size_t keylen,
                              void *data, size_t datalen);

/**
 * hcache_delete_t - backend-specific routine to delete a message's headers
 * @param ctx    The backend-specific context retrieved via hcache_open
 * @param key    A message identification string
 * @param keylen The length of the string pointed to by key
 * @retval 0 on success
 * @retval a backend-specific error code otherwise
 */
typedef int (*hcache_delete_t)(void *ctx, const char *key, size_t keylen);

/**
 * hcache_close_t - backend-specific routine to close a context
 * @param ctx The backend-specific context retrieved via hcache_open
 *
 * Backend code is responsible for freeing any resources associated with the
 * @a ctx parameter. For this reason, backend code is passed a pointer-to-pointer
 * to the context, so that FREE can be invoked on it.
 */
typedef void (*hcache_close_t)(void **ctx);

/**
 * hcache_backend_t - backend-specific identification string
 *
 * @retval String describing the currently used hcache backend
 */
typedef const char *(*hcache_backend_t)(void);

/**
 * struct HcacheOps - Header Cache API
 */
struct HcacheOps
{
  const char       *name;
  hcache_open_t    open;
  hcache_fetch_t   fetch;
  hcache_free_t    free;
  hcache_store_t   store;
  hcache_delete_t  delete;
  hcache_close_t   close;
  hcache_backend_t backend;
};

#define HCACHE_BACKEND_LIST                                                    \
  HCACHE_BACKEND(bdb)                                                          \
  HCACHE_BACKEND(gdbm)                                                         \
  HCACHE_BACKEND(kyotocabinet)                                                 \
  HCACHE_BACKEND(lmdb)                                                         \
  HCACHE_BACKEND(qdbm)                                                         \
  HCACHE_BACKEND(tokyocabinet)

#define HCACHE_BACKEND_OPS(_name)                                              \
  const struct HcacheOps hcache_##_name##_ops = {                              \
    .name = #_name,                                                            \
    .open = hcache_##_name##_open,                                             \
    .fetch = hcache_##_name##_fetch,                                           \
    .free = hcache_##_name##_free,                                             \
    .store = hcache_##_name##_store,                                           \
    .delete = hcache_##_name##_delete,                                         \
    .close = hcache_##_name##_close,                                           \
    .backend = hcache_##_name##_backend,                                       \
  };

#endif /* _MUTT_HCACHE_BACKEND_H */
