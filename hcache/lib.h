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
 * @page lib_hcache Email Header Cache
 *
 * Cache of Email headers
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
 * | hcache/config.c     | @subpage hc_config |
 * | hcache/hcache.c     | @subpage hc_hcache |
 * | hcache/serialize.c  | @subpage hc_serial |
 */

#ifndef MUTT_HCACHE_LIB_H
#define MUTT_HCACHE_LIB_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "compress/lib.h"
#include "store/lib.h"

struct Buffer;
struct Email;

/**
 * struct HeaderCache - Header Cache
 *
 * This is the interface to the local cache of Email headers.
 * The data is kept in a Store (database) which can be optionally Compressed.
 */
struct HeaderCache
{
  char *folder;                       ///< Folder name
  unsigned int crc;                   ///< CRC of the cache entry
  const struct StoreOps *store_ops;   ///< Store backend
  StoreHandle *store_handle;          ///< Store handle
  const struct ComprOps *compr_ops;   ///< Compression backend
  ComprHandle *compr_handle;          ///< Compression handle
};

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
 * @defgroup hcache_namer_api Header Cache Naming API
 *
 * Prototype for function to compose hcache file names
 *
 * @param path    Path of message
 * @param dest    Buffer for filename
 */
typedef void (*hcache_namer_t)(const char *path, struct Buffer *dest);

/**
 * hcache_open - Open the connection to the header cache
 * @param path   Location of the header cache (often as specified by the user)
 * @param folder Name of the folder containing the messages
 * @param namer  Optional (might be NULL) client-specific function to form the
 *               final name of the hcache database file.
 * @retval ptr  Success, struct HeaderCache struct
 * @retval NULL Otherwise
 */
struct HeaderCache *hcache_open(const char *path, const char *folder, hcache_namer_t namer);

/**
 * hcache_close - Close the connection to the header cache
 * @param ptr Pointer to the struct HeaderCache structure got by hcache_open()
 *
 * @note The pointer will be set to NULL
 */
void hcache_close(struct HeaderCache **ptr);

/**
 * hcache_store - Store a Header along with a validity datum
 * @param hc          Pointer to the struct HeaderCache structure got by hcache_open()
 * @param key         Message identification string
 * @param keylen      Length of the key string
 * @param e           Email to store
 * @param uidvalidity IMAP-specific UIDVALIDITY value, or 0 to use the current time
 * @retval 0   Success
 * @retval num Generic or backend-specific error code otherwise
 */
int hcache_store(struct HeaderCache *hc, const char *key, size_t keylen,
                      struct Email *e, uint32_t uidvalidity);

/**
 * hcache_fetch - Fetch and validate a  message's header from the cache
 * @param hc     Pointer to the struct HeaderCache structure got by hcache_open()
 * @param key    Message identification string
 * @param keylen Length of the string pointed to by key
 * @param uidvalidity Only restore if it matches the stored uidvalidity
 * @retval obj HCacheEntry containing an Email, empty on failure
 *
 * @note This function performs a check on the validity of the data found by
 *       comparing it with the crc value of the struct HeaderCache structure.
 */
struct HCacheEntry hcache_fetch(struct HeaderCache *hc, const char *key, size_t keylen, uint32_t uidvalidity);

char *hcache_fetch_str(struct HeaderCache *hc, const char *key, size_t keylen);
bool  hcache_fetch_obj_(struct HeaderCache *hc, const char *key, size_t keylen, void *dst, size_t dstlen);
#define hcache_fetch_obj(hc, key, keylen, dst) hcache_fetch_obj_(hc, key, keylen, dst, sizeof(*dst))

int hcache_store_raw(struct HeaderCache *hc, const char *key, size_t keylen,
                          void *data, size_t dlen);

/**
 * hcache_delete_record - Delete a key / data pair
 * @param hc     Pointer to the struct HeaderCache structure got by hcache_open()
 * @param key    Message identification string
 * @param keylen Length of the string pointed to by key
 * @retval 0   Success
 * @retval num Generic or backend-specific error code otherwise
 */
int hcache_delete_record(struct HeaderCache *hc, const char *key, size_t keylen);

#endif /* MUTT_HCACHE_LIB_H */
