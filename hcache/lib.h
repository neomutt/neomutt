/**
 * @file
 * Header cache multiplexor
 *
 * @authors
 * Copyright (C) 2004 Thomas Glanzmann <sithglan@stud.uni-erlangen.de>
 * Copyright (C) 2004 Tobias Werth <sitowert@stud.uni-erlangen.de>
 * Copyright (C) 2004 Brian Fundakowski Feldman <green@FreeBSD.org>
 * Copyright (C) 2016 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020 Tino Reichardt <milky-neomutt@mcmilk.de>
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
 * @page hcache HCACHE: Header cache API
 *
 * The Header Cache saves data from email headers to a local store in order to
 * speed up network mailboxes.
 *
 * @sa \ref store
 * @sa \ref compress
 *
 * ## Operation
 *
 * When NeoMutt parses an email, it stores the results in a number of structures
 * (listed below).  To save time and network traffic, NeoMutt can save the
 * results into a \ref store (optionally using \ref compress).
 *
 * @sa Address Body Buffer Email Envelope ListNode Parameter
 *
 * To save the data, the Header Cache uses a set of 'dump' functions
 * (\ref hc_serial) to 'serialise' the structures.  The cache also stores a CRC
 * checksum of the C structs that were used.  When retrieving the data, the
 * Header Cache uses a set of 'restore' functions to turn the data back into
 * structs.
 *
 * The CRC checksum is created by `hcache/hcachever.sh` during the build
 * process.  Whenever the definition of any of the structs changes, the CRC will
 * change, invalidating any existing cached data.
 *
 * @note Adding or removing a field from the set of serialised fields will
 * **not** affect the CRC.  In this case, it is vital that you bump the
 * **`BASEVERSION`** variable in `hcache/hcachever.sh`
 *
 * ## Source
 *
 * | File                | Description        |
 * | :------------------ | :----------------- |
 * | hcache/hcache.c     | @subpage hc_hcache |
 * | hcache/serialize.c  | @subpage hc_serial |
 */

#ifndef MUTT_HCACHE_LIB_H
#define MUTT_HCACHE_LIB_H

#include <stddef.h>
#include <stdbool.h>

struct Buffer;
struct Email;

/**
 * struct EmailCache - header cache structure
 *
 * This struct holds both the backend-agnostic and the backend-specific parts
 * of the header cache. Backend code MUST initialize the fetch, store,
 * delete and close function pointers in hcache_open, and MAY store
 * backend-specific context in the ctx pointer.
 */
struct EmailCache
{
  char *folder;
  unsigned int crc;
  void *ctx;
  void *cctx;
};

typedef struct EmailCache header_cache_t;

/**
 * struct HCacheEntry - Wrapper for Email retrieved from the header cache
 */
struct HCacheEntry
{
  uint32_t uidvalidity; ///< IMAP-specific UIDVALIDITY
  unsigned int crc;     ///< CRC of Email/Body/etc structs
  struct Email *email;  ///< Retrieved email
};

/**
 * typedef hcache_namer_t - Prototype for function to compose hcache file names
 * @param path    Path of message
 * @param dest    Buffer for filename
 */
typedef void (*hcache_namer_t)(const char *path, struct Buffer *dest);

/* These Config Variables are only used in hcache/hcache.c */
extern char *C_HeaderCacheBackend;
extern short C_HeaderCacheCompressLevel;
extern char *C_HeaderCacheCompressMethod;

/**
 * mutt_hcache_open - open the connection to the header cache
 * @param path   Location of the header cache (often as specified by the user)
 * @param folder Name of the folder containing the messages
 * @param namer  Optional (might be NULL) client-specific function to form the
 *               final name of the hcache database file.
 * @retval ptr  Success, header_cache_t struct
 * @retval NULL Otherwise
 */
header_cache_t *mutt_hcache_open(const char *path, const char *folder, hcache_namer_t namer);

/**
 * mutt_hcache_close - close the connection to the header cache
 * @param hc Pointer to the header_cache_t structure got by mutt_hcache_open()
 */
void mutt_hcache_close(header_cache_t *hc);

/**
 * mutt_hcache_store - store a Header along with a validity datum
 * @param hc          Pointer to the header_cache_t structure got by mutt_hcache_open()
 * @param key         Message identification string
 * @param keylen      Length of the key string
 * @param e           Email to store
 * @param uidvalidity IMAP-specific UIDVALIDITY value, or 0 to use the current time
 * @retval 0   Success
 * @retval num Generic or backend-specific error code otherwise
 */
int mutt_hcache_store(header_cache_t *hc, const char *key, size_t keylen,
                      struct Email *e, uint32_t uidvalidity);

/**
 * mutt_hcache_fetch - fetch and validate a  message's header from the cache
 * @param hc     Pointer to the header_cache_t structure got by mutt_hcache_open()
 * @param key    Message identification string
 * @param keylen Length of the string pointed to by key
 * @param uidvalidity Only restore if it matches the stored uidvalidity
 * @retval obj Success, HCacheEntry containing an Email
 * @retval obj Failure, empty HCacheEntry
 *
 * @note This function performs a check on the validity of the data found by
 *       comparing it with the crc value of the header_cache_t structure.
 */
struct HCacheEntry mutt_hcache_fetch(header_cache_t *hc, const char *key, size_t keylen, uint32_t uidvalidity);

int mutt_hcache_store_raw(header_cache_t *hc, const char *key, size_t keylen,
                          void *data, size_t dlen);

void *mutt_hcache_fetch_raw(header_cache_t *hc, const char *key, size_t keylen, size_t *dlen);

/**
 * mutt_hcache_free_raw - free data fetched with mutt_hcache_fetch_raw()
 * @param hc   Pointer to the header_cache_t structure got by mutt_hcache_open()
 * @param data Pointer to the data got using mutt_hcache_fetch_raw
 */
void mutt_hcache_free_raw(header_cache_t *hc, void **data);

/**
 * mutt_hcache_delete_header - delete a key / data pair
 * @param hc     Pointer to the header_cache_t structure got by mutt_hcache_open()
 * @param key    Message identification string
 * @param keylen Length of the string pointed to by key
 * @retval 0   Success
 * @retval num Generic or backend-specific error code otherwise
 */
int mutt_hcache_delete_header(header_cache_t *hc, const char *key, size_t keylen);

#endif /* MUTT_HCACHE_LIB_H */
