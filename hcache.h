/*
 * Copyright (C) 2004 Thomas Glanzmann <sithglan@stud.uni-erlangen.de>
 * Copyright (C) 2004 Tobias Werth <sitowert@stud.uni-erlangen.de>
 * Copyright (C) 2004 Brian Fundakowski Feldman <green@FreeBSD.org>
 * Copyright (C) 2016 Pietro Cerutti <gahr@gahr.ch>
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _HCACHE_H_
#define _HCACHE_H_ 1

#include "mutt.h"

struct header_cache;
typedef struct header_cache header_cache_t;

/**
 * hcache_open_t - backend-specific routing to open the header cache database.
 *
 * @param path The path to the database file.
 * @return Pointer to backend-specific context on success, NULL otherwise.
 *
 * The hcache_open function has the purpose of opening a backend-specific
 * connection to the database file specified by the path parameter. Backends
 * MUST return non-NULL specific context informations on success. This will be
 * stored in the ctx member of the header_cache_t structure and passed on to
 * all other backend-specific functions (see below).
 */
typedef void * (*hcache_open_t)(const char *path);

/**
 * hcache_fetch_t -  backend-specific routine to fetch a message's headers.
 *
 * @param ctx The backend-specific context retrieved via hcache_open.
 * @param key A message identification string.
 * @param keylen The length of the string pointed to by key.
 * @return Pointer to the message's headers on success, NULL otherwise.
 */
typedef void * (*hcache_fetch_t)(void *ctx, const char *key, size_t keylen);

/**
 * hcache_store_t - backend-specific routine to store a message's headers.
 *
 * @param ctx The backend-specific context retrieved via hcache_open.
 * @param key A message identification string.
 * @param keylen The length of the string pointed to by key.
 * @param data The message headers data.
 * @param datalen The length of the string pointed to by data.
 * @return 0 on success, a backend-specific error code otherwise.
 */
typedef int (*hcache_store_t)(void *ctx, const char *key, size_t keylen,
                                  void *data, size_t datalen);

/**
 * hcache_delete_t - backend-specific routine to delete a message's headers.
 *
 * @param ctx The backend-specific context retrieved via hcache_open.
 * @param key A message identification string.
 * @param keylen The length of the string pointed to by key.
 * @return 0 on success, a backend-specific error code otherwise.
 */
typedef int (*hcache_delete_t)(void *ctx, const char *key, size_t keylen);

/**
 * hcache_close_t - backend-specific routine to close a context.
 *
 * @param ctx The backend-specific context retrieved via hcache_open.
 *
 * Backend code is responsible for freeing any resources associated with the
 * @ctx parameter. For this reason, backend code is passed a pointer-to-pointer
 * to the context, so that FREE can be invoked on it.
 */
typedef void (*hcache_close_t)(void **ctx);

/**
 * hcache_backend_t - backend-specific identification string.
 *
 * @return String describing the currently used hcache backend.
 */
typedef const char *(*hcache_backend_t)(void);

typedef struct
{
    hcache_open_t    open;
    hcache_fetch_t   fetch;
    hcache_store_t   store;
    hcache_delete_t  delete;
    hcache_close_t   close;
    hcache_backend_t backend;
} hcache_ops_t;

typedef int (*hcache_namer_t)(const char* path, char* dest, size_t dlen);

/**
 * mutt_hcache_open - open the connection to the header cache.
 *
 * @param path Location of the header cache (often as specified by the user).
 * @param folder Name of the folder containing the messages.
 * @param namer Optional (might be NULL) client-specific function to form the
 * final name of the hcache database file.
 * @return Pointer to a header_cache_t struct on success, NULL otherwise.
 */
header_cache_t *
mutt_hcache_open(const char *path, const char *folder, hcache_namer_t namer);

/**
 * mutt_hcache_close - close the connection to the header cache.
 *
 * @param h Pointer to the header_cache_t structure got by mutt_hcache_open.
 */
void
mutt_hcache_close(header_cache_t *h);

/**
 * mutt_hcache_fetch - fetch and validate a  message's header from the cache.
 *
 * @param h Pointer to the header_cache_t structure got by mutt_hcache_open.
 * @param key Message identification string.
 * @param keylen Length of the string pointed to by key.
 * @return Pointer to the data if found and valid, NULL otherwise.
 * @note This function performs a check on the validity of the data found by
 * comparing it with the crc value of the header_cache_t structure.
 */
void *
mutt_hcache_fetch(header_cache_t *h, const char *key, size_t keylen);

/**
 * mutt_hcache_fetch_raw - fetch a message's header from the cache.
 *
 * @param h Pointer to the header_cache_t structure got by mutt_hcache_open.
 * @param key Message identification string.
 * @param keylen Length of the string pointed to by key.
 * @return Pointer to the data if found, NULL otherwise.
 * @note This function does not perform any check on the validity of the data
 * found.
 */
void *
mutt_hcache_fetch_raw(header_cache_t *h, const char *key, size_t keylen);

/**
 * mutt_hcache_restore - restore a HEADER from data retrieved from the cache.
 *
 * @param d Data retrieved using mutt_hcache_fetch or mutt_hcache_fetch_raw.
 * @return Pointer to the restored header (cannot be NULL).
 * @note The returned HEADER must be free'd by caller code with
 * mutt_free_header.
 */
HEADER *
mutt_hcache_restore(const unsigned char *d);

/**
 * mutt_hcache_store - store a HEADER along with a validity datum.
 *
 * @param h Pointer to the header_cache_t structure got by mutt_hcache_open.
 * @param key Message identification string.
 * @param keylen Length of the string pointed to by key.
 * @param header Message header to store.
 * @param uidvalidity IMAP-specific UIDVALIDITY value, or 0 to use the current
 * time.
 * @return 0 on success, -1 otherwise.
 */
int
mutt_hcache_store(header_cache_t *h, const char *key, size_t keylen,
                  HEADER *header, unsigned int uidvalidity);

/**
 * mutt_hcache_store_raw - store a key / data pair.
 *
 * @param h Pointer to the header_cache_t structure got by mutt_hcache_open.
 * @param key Message identification string.
 * @param keylen Length of the string pointed to by key.
 * @param data Payload to associate with key.
 * @param dlen Length of the buffer pointed to by the @data parameter.
 * @return 0 on success, -1 otherwise.
 */
int
mutt_hcache_store_raw(header_cache_t *h, const char* key, size_t keylen, 
                      void* data, size_t dlen);

/**
 * mutt_hcache_delete - delete a key / data pair.
 *
 * @param h Pointer to the header_cache_t structure got by mutt_hcache_open.
 * @param key Message identification string.
 * @param keylen Length of the string pointed to by key.
 * @return 0 on success, -1 otherwise.
 */
int
mutt_hcache_delete(header_cache_t *h, const char *key, size_t keylen);

/**
 * mutt_hcache_backend - get a backend-specific identification string.
 *
 * @return String describing the currently used hcache backend.
 */
const char *
mutt_hcache_backend(void);

#define HCACHE_BACKEND_LIST \
  HCACHE_BACKEND(bdb) \
  HCACHE_BACKEND(gdbm) \
  HCACHE_BACKEND(kc) \
  HCACHE_BACKEND(lmdb) \
  HCACHE_BACKEND(qdbm) \
  HCACHE_BACKEND(tc)

#define HCACHE_BACKEND_OPS(name)       \
  hcache_ops_t hcache_##name##_ops = { \
    .open    = hcache_##name##_open,   \
    .fetch   = hcache_##name##_fetch,  \
    .store   = hcache_##name##_store,  \
    .delete  = hcache_##name##_delete, \
    .close   = hcache_##name##_close,  \
    .backend = hcache_##name##_backend \
  };

#endif /* _HCACHE_H_ */
