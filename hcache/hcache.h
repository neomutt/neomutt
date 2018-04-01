/**
 * @file
 * Header cache multiplexor
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
 * @page hcache Header cache API
 *
 * This module defines the user-visible header cache API, which is used within
 * neomutt to cache and restore mail header data.
 *
 * @subpage hc_hcache
 *
 * Backends:
 *
 * | File          | Description      |
 * | :------------ | :--------------- |
 * | hcache/bdb.c  | @subpage hc_bdb  |
 * | hcache/gdbm.c | @subpage hc_gdbm |
 * | hcache/kc.c   | @subpage hc_kc   |
 * | hcache/lmdb.c | @subpage hc_lmdb |
 * | hcache/qdbm.c | @subpage hc_qdbm |
 * | hcache/tc.c   | @subpage hc_tc   |
 */

#ifndef _MUTT_HCACHE_H
#define _MUTT_HCACHE_H

#include <stddef.h>

struct Header;
typedef struct HeaderCache header_cache_t;

typedef int (*hcache_namer_t)(const char *path, char *dest, size_t dlen);

/**
 * mutt_hcache_open - open the connection to the header cache
 * @param path   Location of the header cache (often as specified by the user)
 * @param folder Name of the folder containing the messages
 * @param namer  Optional (might be NULL) client-specific function to form the
 *               final name of the hcache database file.
 * @retval Pointer to a header_cache_t struct on success
 * @retval NULL otherwise
 */
header_cache_t *mutt_hcache_open(const char *path, const char *folder, hcache_namer_t namer);

/**
 * mutt_hcache_close - close the connection to the header cache
 * @param h Pointer to the header_cache_t structure got by mutt_hcache_open
 */
void mutt_hcache_close(header_cache_t *h);

/**
 * mutt_hcache_fetch - fetch and validate a  message's header from the cache
 * @param h      Pointer to the header_cache_t structure got by mutt_hcache_open
 * @param key    Message identification string
 * @param keylen Length of the string pointed to by key
 * @retval Pointer to the data if found and valid
 * @retval NULL otherwise
 * @note This function performs a check on the validity of the data found by
 *       comparing it with the crc value of the header_cache_t structure.
 * @note The returned pointer must be freed by calling mutt_hcache_free. This
 *       must be done before closing the header cache with mutt_hcache_close.
 */
void *mutt_hcache_fetch(header_cache_t *h, const char *key, size_t keylen);

/**
 * mutt_hcache_fetch_raw - fetch a message's header from the cache
 * @param h      Pointer to the header_cache_t structure got by mutt_hcache_open
 * @param key    Message identification string
 * @param keylen Length of the string pointed to by key
 * @retval Pointer to the data if found
 * @retval NULL otherwise
 * @note This function does not perform any check on the validity of the data
 *       found.
 * @note The returned pointer must be freed by calling mutt_hcache_free. This
 *       must be done before closing the header cache with mutt_hcache_close.
 */
void *mutt_hcache_fetch_raw(header_cache_t *h, const char *key, size_t keylen);

/**
 * mutt_hcache_free - free previously fetched data
 * @param h    Pointer to the header_cache_t structure got by mutt_hcache_open
 * @param data Pointer to the data got using hcache_fetch or hcache_fetch_raw
 */
void mutt_hcache_free(header_cache_t *h, void **data);

/**
 * mutt_hcache_restore - restore a Header from data retrieved from the cache
 * @param d Data retrieved using mutt_hcache_fetch or mutt_hcache_fetch_raw
 * @retval Pointer to the restored header (cannot be NULL)
 * @note The returned Header must be free'd by caller code with
 *       mutt_header_free().
 */
struct Header *mutt_hcache_restore(const unsigned char *d);

/**
 * mutt_hcache_store - store a Header along with a validity datum
 * @param h           Pointer to the header_cache_t structure got by mutt_hcache_open
 * @param key         Message identification string
 * @param keylen      Length of the string pointed to by key
 * @param header      Message header to store
 * @param uidvalidity IMAP-specific UIDVALIDITY value, or 0 to use the current time
 * @retval 0 on success
 * @return A generic or backend-specific error code otherwise
 */
int mutt_hcache_store(header_cache_t *h, const char *key, size_t keylen,
                      struct Header *header, unsigned int uidvalidity);

/**
 * mutt_hcache_store_raw - store a key / data pair
 * @param h      Pointer to the header_cache_t structure got by mutt_hcache_open
 * @param key    Message identification string
 * @param keylen Length of the string pointed to by key
 * @param data   Payload to associate with key
 * @param dlen   Length of the buffer pointed to by the @a data parameter
 * @retval 0 on success
 * @return A generic or backend-specific error code otherwise
 */
int mutt_hcache_store_raw(header_cache_t *h, const char *key, size_t keylen,
                          void *data, size_t dlen);

/**
 * mutt_hcache_delete - delete a key / data pair
 * @param h      Pointer to the header_cache_t structure got by mutt_hcache_open
 * @param key    Message identification string
 * @param keylen Length of the string pointed to by key
 * @retval 0 on success
 * @return A generic or backend-specific error code otherwise
 */
int mutt_hcache_delete(header_cache_t *h, const char *key, size_t keylen);

/**
 * mutt_hcache_backend_list - get a list of backend identification strings
 * @retval Comma separated string describing the compiled-in backends
 * @note The returned string must be free'd by the caller
 */
const char *mutt_hcache_backend_list(void);

/**
 * mutt_hcache_is_valid_backend - Is the string a valid hcache backend
 * @param s String identifying a backend
 * @retval 1 if s is recognized as a valid backend
 * @retval 0 otherwise
 */
int mutt_hcache_is_valid_backend(const char *s);

#endif /* _MUTT_HCACHE_H */
