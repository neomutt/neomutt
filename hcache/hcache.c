/**
 * @file
 * Header cache multiplexor
 *
 * @authors
 * Copyright (C) 2004 Thomas Glanzmann <sithglan@stud.uni-erlangen.de>
 * Copyright (C) 2004 Tobias Werth <sitowert@stud.uni-erlangen.de>
 * Copyright (C) 2004 Brian Fundakowski Feldman <green@FreeBSD.org>
 * Copyright (C) 2016 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2019-2020 Tino Reichardt <milky-neomutt@mcmilk.de>
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
 * @page hc_hcache Header cache multiplexor
 *
 * This module implements the gateway between the user visible part of the
 * header cache API and the backend specific API. Also, this module implements
 * the serialization/deserialization routines for the Header structure.
 */

#include "config.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "lib.h"
#include "compress/lib.h"
#include "store/lib.h"
#include "hcache/hcversion.h" // path needed by out-of-tree build
#include "muttlib.h"
#include "serialize.h"

#if !(defined(HAVE_BDB) || defined(HAVE_GDBM) || defined(HAVE_KC) ||           \
      defined(HAVE_LMDB) || defined(HAVE_QDBM) || defined(HAVE_ROCKSDB) ||     \
      defined(HAVE_TC) || defined(HAVE_TDB))
#error "No hcache backend defined"
#endif

/// Header Cache version
static unsigned int HcacheVer = 0x0;

/**
 * hcache_free - Free a header cache
 * @param ptr header cache to free
 */
static void hcache_free(struct HeaderCache **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct HeaderCache *hc = *ptr;
  FREE(&hc->folder);

  FREE(ptr);
}

/**
 * hcache_new - Create a new header cache
 * @retval ptr Newly created header cache
 */
static struct HeaderCache *hcache_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct HeaderCache));
}

/**
 * header_size - Compute the size of the header with uuid validity and crc
 * @retval num Size of the header
 */
static size_t header_size(void)
{
  return sizeof(int) + sizeof(uint32_t);
}

/**
 * email_pack_flags - Pack the Email flags into a uint32_t
 * @param e Email to pack
 * @retval num uint32_t of packed flags
 *
 * @note Order of packing must match email_unpack_flags()
 */
static inline uint32_t email_pack_flags(const struct Email *e)
{
  if (!e)
    return 0;

  // clang-format off
  return e->security +
        (e->expired    << 16) +
        (e->flagged    << 17) +
        (e->mime       << 18) +
        (e->old        << 19) +
        (e->read       << 20) +
        (e->replied    << 21) +
        (e->superseded << 22) +
        (e->trash      << 23);
  // clang-format on
}

/**
 * email_unpack_flags - Unpack the Email flags from a uint32_t
 * @param e      Email to unpack into
 * @param packed Packed flags
 *
 * @note Order of packing must match email_pack_flags()
 */
static inline void email_unpack_flags(struct Email *e, uint32_t packed)
{
  if (!e)
    return;

  // clang-format off
  e->security   = (packed & ((1 << 16) - 1)); // bits 0-15
  e->expired    = (packed & (1 << 16));
  e->flagged    = (packed & (1 << 17));
  e->mime       = (packed & (1 << 18));
  e->old        = (packed & (1 << 19));
  e->read       = (packed & (1 << 20));
  e->replied    = (packed & (1 << 21));
  e->superseded = (packed & (1 << 22));
  e->trash      = (packed & (1 << 23));
  // clang-format on
}

/**
 * email_pack_timezone - Pack the Email timezone into a uint32_t
 * @param e Email to pack
 * @retval num uint32_t of packed timezone
 *
 * @note Order of packing must match email_unpack_timezone()
 */
static inline uint32_t email_pack_timezone(const struct Email *e)
{
  if (!e)
    return 0;

  return e->zhours + (e->zminutes << 5) + (e->zoccident << 11);
}

/**
 * email_unpack_timezone - Unpack the Email timezone from a uint32_t
 * @param e      Email to unpack into
 * @param packed Packed timezone
 *
 * @note Order of packing must match email_pack_timezone()
 */
static inline void email_unpack_timezone(struct Email *e, uint32_t packed)
{
  if (!e)
    return;

  // clang-format off
  e->zhours    =  (packed       & ((1 << 5) - 1)); // bits 0-4 (5)
  e->zminutes  = ((packed >> 5) & ((1 << 6) - 1)); // bits 5-10 (6)
  e->zoccident =  (packed       &  (1 << 11));     // bit  11 (1)
  // clang-format on
}

/**
 * dump_email - Serialise an Email object
 * @param hc          Header cache handle
 * @param e           Email to serialise
 * @param off         Size of the binary blob
 * @param uidvalidity IMAP server identifier
 * @retval ptr Binary blob representing the Email
 *
 * This function transforms an Email into a binary string so that it can be
 * saved to a database.
 */
static void *dump_email(struct HeaderCache *hc, const struct Email *e, int *off, uint32_t uidvalidity)
{
  bool convert = !CharsetIsUtf8;

  *off = 0;
  unsigned char *d = mutt_mem_malloc(4096);

  d = serial_dump_uint32_t((uidvalidity != 0) ? uidvalidity : mutt_date_now(), d, off);
  d = serial_dump_int(hc->crc, d, off);

  assert((size_t) *off == header_size());

  uint32_t packed = email_pack_flags(e);
  d = serial_dump_uint32_t(packed, d, off);

  packed = email_pack_timezone(e);
  d = serial_dump_uint32_t(packed, d, off);

  uint64_t big = e->date_sent;
  d = serial_dump_uint64_t(big, d, off);
  big = e->received;
  d = serial_dump_uint64_t(big, d, off);

  d = serial_dump_int(e->lines, d, off);

  d = serial_dump_envelope(e->env, d, off, convert);
  d = serial_dump_body(e->body, d, off, convert);
  d = serial_dump_tags(&e->tags, d, off);

  return d;
}

/**
 * restore_email - Restore an Email from data retrieved from the cache
 * @param d Data retrieved using hcache_fetch()
 * @retval ptr Success, the restored header (can't be NULL)
 *
 * @note The returned Email must be free'd by caller code with
 *       email_free()
 */
static struct Email *restore_email(const unsigned char *d)
{
  int off = 0;
  struct Email *e = email_new();
  bool convert = !CharsetIsUtf8;

  off += sizeof(uint32_t);     // skip validate
  off += sizeof(unsigned int); // skip crc

  uint32_t packed = 0;
  serial_restore_uint32_t(&packed, d, &off);
  email_unpack_flags(e, packed);

  packed = 0;
  serial_restore_uint32_t(&packed, d, &off);
  email_unpack_timezone(e, packed);

  uint64_t big = 0;
  serial_restore_uint64_t(&big, d, &off);
  e->date_sent = big;

  big = 0;
  serial_restore_uint64_t(&big, d, &off);
  e->received = big;

  unsigned int num = 0;
  serial_restore_int(&num, d, &off);
  e->lines = num;

  e->env = mutt_env_new();
  serial_restore_envelope(e->env, d, &off, convert);

  e->body = mutt_body_new();
  serial_restore_body(e->body, d, &off, convert);
  serial_restore_tags(&e->tags, d, &off);

  return e;
}

/**
 * struct RealKey - Hcache key name (including compression method)
 */
struct RealKey
{
  char key[1024]; ///< Key name
  size_t len;     ///< Length of key
};

/**
 * realkey - Compute the real key used in the backend, taking into account the compression method
 * @param  hc     Header cache handle
 * @param  key    Original key
 * @param  keylen Length of original key
 * @retval ptr Static location holding data and length of the real key
 */
static struct RealKey *realkey(struct HeaderCache *hc, const char *key, size_t keylen)
{
  static struct RealKey rk;
#ifdef USE_HCACHE_COMPRESSION
  if (hc->compr_ops)
  {
    rk.len = snprintf(rk.key, sizeof(rk.key), "%.*s-%s", (int) keylen, key,
                      hc->compr_ops->name);
  }
  else
#endif
  {
    mutt_strn_copy(rk.key, key, keylen, sizeof(rk.key));
    rk.len = keylen;
  }
  return &rk;
}

/**
 * create_hcache_dir - Create parent dirs for the hcache database
 * @param path Database filename
 * @retval true Success
 * @retval false Failure (errno set)
 */
static bool create_hcache_dir(const char *path)
{
  char *dir = mutt_str_dup(path);
  if (!dir)
    return false;

  char *p = strrchr(dir, '/');
  if (!p)
  {
    FREE(&dir);
    return true;
  }

  *p = '\0';

  int rc = mutt_file_mkdir(dir, S_IRWXU | S_IRWXG | S_IRWXO);
  if (rc != 0)
    mutt_error(_("Can't create %s: %s"), dir, strerror(errno));

  FREE(&dir);
  return (rc == 0);
}

/**
 * hcache_per_folder - Generate the hcache pathname
 * @param hc     Header cache handle
 * @param hcpath Buffer for the result
 * @param path   Base directory, from $header_cache
 * @param namer  Callback to generate database filename - Implements ::hcache_namer_t
 *
 * Generate the pathname for the hcache database, it will be of the form:
 *     BASE/FOLDER/NAME
 *
 * * BASE:   Base directory (@a path)
 * * FOLDER: Mailbox name (hc->folder)
 * * NAME:   Create by @a namer, or md5sum of hc->folder
 *
 * This function will create any parent directories needed, so the caller just
 * needs to create the database file.
 *
 * If @a path exists and is a directory, it is used.
 * If @a path has a trailing '/' it is assumed to be a directory.
 * Otherwise @a path is assumed to be a file.
 */
static void hcache_per_folder(struct HeaderCache *hc, struct Buffer *hcpath,
                              const char *path, hcache_namer_t namer)
{
  struct stat st = { 0 };

  int plen = mutt_str_len(path);
  int rc = stat(path, &st);
  bool slash = (path[plen - 1] == '/');

  if (((rc == 0) && !S_ISDIR(st.st_mode)) || ((rc == -1) && !slash))
  {
    /* An existing file or a non-existing path not ending with a slash */
    mutt_encode_path(hcpath, path);
    create_hcache_dir(buf_string(hcpath));
    return;
  }

  /* We have a directory - no matter whether it exists, or not */
  struct Buffer *hcfile = buf_pool_get();
  if (namer)
  {
    namer(hc->folder, hcfile);
    buf_concat_path(hcpath, path, buf_string(hcfile));
  }
  else
  {
    unsigned char m[16]; /* binary md5sum */
    struct Buffer *name = buf_pool_get();

#ifdef USE_HCACHE_COMPRESSION
    const char *cm = hc->compr_ops ? hc->compr_ops->name : "";
    buf_printf(name, "%s|%s%s", hc->store_ops->name, hc->folder, cm);
#else
    buf_printf(name, "%s|%s", hc->store_ops->name, hc->folder);
#endif
    mutt_md5(buf_string(name), m);
    buf_reset(name);
    mutt_md5_toascii(m, name->data);
    buf_printf(hcpath, "%s%s%s", path, slash ? "" : "/", buf_string(name));
    buf_pool_release(&name);
  }

  mutt_encode_path(hcpath, buf_string(hcpath));
  create_hcache_dir(buf_string(hcpath));
  buf_pool_release(&hcfile);
}

/**
 * get_foldername - Where should the cache be stored?
 * @param folder Path to be canonicalised
 * @retval ptr New string with canonical path
 */
static char *get_foldername(const char *folder)
{
  /* if the folder is local, canonify the path to ensure equivalent paths share
   * the hcache */
  char *p = mutt_mem_malloc(PATH_MAX + 1);
  if (!realpath(folder, p))
    mutt_str_replace(&p, folder);

  return p;
}

/**
 * fetch_raw - Fetch a message's header from the cache
 * @param[in]  hc     Pointer to the struct HeaderCache structure got by hcache_open()
 * @param[in]  key    Message identification string
 * @param[in]  keylen Length of the string pointed to by key
 * @param[out] dlen   Length of the fetched data
 * @retval ptr  Success, the data if found
 * @retval NULL Otherwise
 *
 * @note This function does not perform any check on the validity of the data found.
 * @note The returned data must be free with free_raw().
 */
static void *fetch_raw(struct HeaderCache *hc, const char *key, size_t keylen, size_t *dlen)
{
  if (!hc)
    return NULL;

  struct Buffer *path = buf_pool_get();
  keylen = buf_printf(path, "%s%.*s", hc->folder, (int) keylen, key);
  void *blob = hc->store_ops->fetch(hc->store_handle, buf_string(path), keylen, dlen);
  buf_pool_release(&path);
  return blob;
}

/**
 * free_raw - Multiplexor for StoreOps::free
 */
static void free_raw(struct HeaderCache *hc, void **data)
{
  hc->store_ops->free(hc->store_handle, data);
}

/**
 * generate_hcachever - Calculate hcache version from dynamic configuration
 * @retval num Header cache version
 */
static unsigned int generate_hcachever(void)
{
  union
  {
    unsigned char charval[16]; ///< MD5 digest as a string
    unsigned int intval;       ///< MD5 digest as an integer
  } digest;
  struct Md5Ctx md5ctx = { 0 };

  mutt_md5_init_ctx(&md5ctx);

  /* Seed with the compiled-in header structure hash */
  unsigned int ver = HCACHEVER;
  mutt_md5_process_bytes(&ver, sizeof(ver), &md5ctx);

  /* Mix in user's spam list */
  struct Replace *sp = NULL;
  STAILQ_FOREACH(sp, &SpamList, entries)
  {
    mutt_md5_process(sp->regex->pattern, &md5ctx);
    mutt_md5_process(sp->templ, &md5ctx);
  }

  /* Mix in user's nospam list */
  struct RegexNode *np = NULL;
  STAILQ_FOREACH(np, &NoSpamList, entries)
  {
    mutt_md5_process(np->regex->pattern, &md5ctx);
  }

  /* Get a hash and take its bytes as an (unsigned int) hash version */
  mutt_md5_finish_ctx(&md5ctx, digest.charval);

  return digest.intval;
}

/**
 * hcache_open - Multiplexor for StoreOps::open
 */
struct HeaderCache *hcache_open(const char *path, const char *folder, hcache_namer_t namer)
{
  if (!path || (path[0] == '\0'))
    return NULL;

  if (HcacheVer == 0x0)
    HcacheVer = generate_hcachever();

  struct HeaderCache *hc = hcache_new();

  hc->folder = get_foldername(folder);
  hc->crc = HcacheVer;

  const char *const c_header_cache_backend = cs_subset_string(NeoMutt->sub, "header_cache_backend");
  hc->store_ops = store_get_backend_ops(c_header_cache_backend);
  if (!hc->store_ops)
  {
    hcache_free(&hc);
    return NULL;
  }

#ifdef USE_HCACHE_COMPRESSION
  const char *const c_header_cache_compress_method = cs_subset_string(NeoMutt->sub, "header_cache_compress_method");
  if (c_header_cache_compress_method)
  {
    hc->compr_ops = compress_get_ops(c_header_cache_compress_method);

    const short c_header_cache_compress_level = cs_subset_number(NeoMutt->sub, "header_cache_compress_level");
    hc->compr_handle = hc->compr_ops->open(c_header_cache_compress_level);
    if (!hc->compr_handle)
    {
      hcache_free(&hc);
      return NULL;
    }

    /* remember the buffer of database backend */
    mutt_debug(LL_DEBUG3, "Header cache will use %s compression\n",
               hc->compr_ops->name);
  }
#endif

  struct Buffer *hcpath = buf_pool_get();
  hcache_per_folder(hc, hcpath, path, namer);

  hc->store_handle = hc->store_ops->open(buf_string(hcpath));
  if (!hc->store_handle)
  {
    /* remove a possibly incompatible version */
    if (unlink(buf_string(hcpath)) == 0)
    {
      hc->store_handle = hc->store_ops->open(buf_string(hcpath));
      if (!hc->store_handle)
      {
        if (hc->compr_ops)
        {
          hc->compr_ops->close(&hc->compr_handle);
        }
        hcache_free(&hc);
      }
    }
  }

  buf_pool_release(&hcpath);
  return hc;
}

/**
 * hcache_close - Multiplexor for StoreOps::close
 */
void hcache_close(struct HeaderCache **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct HeaderCache *hc = *ptr;

#ifdef USE_HCACHE_COMPRESSION
  if (hc->compr_ops)
    hc->compr_ops->close(&hc->compr_handle);
#endif

  hc->store_ops->close(&hc->store_handle);

  hcache_free(ptr);
}

/**
 * hcache_fetch - Multiplexor for StoreOps::fetch
 */
struct HCacheEntry hcache_fetch(struct HeaderCache *hc, const char *key,
                                size_t keylen, uint32_t uidvalidity)
{
  struct HCacheEntry hce = { 0 };
  if (!hc)
    return hce;

  size_t dlen = 0;
  struct RealKey *rk = realkey(hc, key, keylen);
  void *data = fetch_raw(hc, rk->key, rk->len, &dlen);
  void *to_free = data;
  if (!data)
  {
    goto end;
  }

  /* restore uidvalidity and crc */
  size_t hlen = header_size();
  if (hlen > dlen)
  {
    goto end;
  }
  int off = 0;
  serial_restore_uint32_t(&hce.uidvalidity, data, &off);
  serial_restore_int(&hce.crc, data, &off);
  assert((size_t) off == hlen);
  if ((hce.crc != hc->crc) || ((uidvalidity != 0) && (uidvalidity != hce.uidvalidity)))
  {
    goto end;
  }

#ifdef USE_HCACHE_COMPRESSION
  if (hc->compr_ops)
  {
    void *dblob = hc->compr_ops->decompress(hc->compr_handle,
                                            (char *) data + hlen, dlen - hlen);
    if (!dblob)
    {
      goto end;
    }
    data = (char *) dblob - hlen; /* restore skips uidvalidity and crc */
  }
#endif

  hce.email = restore_email(data);

end:
  free_raw(hc, &to_free);
  return hce;
}

/**
 * hcache_fetch_obj_ - Fetch a message's header from the cache into a destination object
 * @param[in]  hc     Pointer to the struct HeaderCache structure got by hcache_open()
 * @param[in]  key    Message identification string
 * @param[in]  keylen Length of the string pointed to by key
 * @param[out] dst    Pointer to the destination object
 * @param[in]  dstlen Size of the destination object
 * @retval true Success, the data was found and the length matches
 * @retval false Otherwise
 */
bool hcache_fetch_obj_(struct HeaderCache *hc, const char *key, size_t keylen,
                       void *dst, size_t dstlen)
{
  bool rc = true;
  size_t srclen = 0;
  void *src = fetch_raw(hc, key, keylen, &srclen);
  if (src && (srclen == dstlen))
  {
    memcpy(dst, src, dstlen);
  }
  else
  {
    rc = false;
  }
  free_raw(hc, &src);
  return rc;
}

/**
 * hcache_fetch_str - Fetch a string from the cache
 * @param[in]  hc     Pointer to the struct HeaderCache structure got by hcache_open()
 * @param[in]  key    Message identification string
 * @param[in]  keylen Length of the string pointed to by key
 * @retval ptr  Success, the data if found
 * @retval NULL Otherwise
 */
char *hcache_fetch_str(struct HeaderCache *hc, const char *key, size_t keylen)
{
  char *res = NULL;
  size_t dlen = 0;
  void *data = fetch_raw(hc, key, keylen, &dlen);
  if (data)
  {
    res = mutt_strn_dup(data, dlen);
    free_raw(hc, &data);
  }
  return res;
}

/**
 * hcache_store - Multiplexor for StoreOps::store
 */
int hcache_store(struct HeaderCache *hc, const char *key, size_t keylen,
                 struct Email *e, uint32_t uidvalidity)
{
  if (!hc)
    return -1;

  int dlen = 0;
  char *data = dump_email(hc, e, &dlen, uidvalidity);

#ifdef USE_HCACHE_COMPRESSION
  if (hc->compr_ops)
  {
    /* We don't compress uidvalidity and the crc, so we can check them before
     * decompressing on fetch().  */
    size_t hlen = header_size();

    /* data / dlen gets ptr to compressed data here */
    size_t clen = dlen;
    void *cdata = hc->compr_ops->compress(hc->compr_handle, data + hlen, dlen - hlen, &clen);
    if (!cdata)
    {
      FREE(&data);
      return -1;
    }

    char *whole = mutt_mem_malloc(hlen + clen);
    memcpy(whole, data, hlen);
    memcpy(whole + hlen, cdata, clen);

    FREE(&data);

    data = whole;
    dlen = hlen + clen;
  }
#endif

  /* store uncompressed data */
  struct RealKey *rk = realkey(hc, key, keylen);
  int rc = hcache_store_raw(hc, rk->key, rk->len, data, dlen);

  FREE(&data);

  return rc;
}

/**
 * hcache_store_raw - Store a key / data pair
 * @param hc     Pointer to the struct HeaderCache structure got by hcache_open()
 * @param key    Message identification string
 * @param keylen Length of the string pointed to by key
 * @param data   Payload to associate with key
 * @param dlen   Length of the buffer pointed to by the @a data parameter
 * @retval 0   Success
 * @retval num Generic or backend-specific error code otherwise
 */
int hcache_store_raw(struct HeaderCache *hc, const char *key, size_t keylen,
                     void *data, size_t dlen)
{
  if (!hc)
    return -1;

  struct Buffer *path = buf_pool_get();

  keylen = buf_printf(path, "%s%.*s", hc->folder, (int) keylen, key);
  int rc = hc->store_ops->store(hc->store_handle, buf_string(path), keylen, data, dlen);
  buf_pool_release(&path);

  return rc;
}

/**
 * hcache_delete_record - Multiplexor for StoreOps::delete_record
 */
int hcache_delete_record(struct HeaderCache *hc, const char *key, size_t keylen)
{
  if (!hc)
    return -1;

  struct Buffer *path = buf_pool_get();

  keylen = buf_printf(path, "%s%s", hc->folder, key);
  int rc = hc->store_ops->delete_record(hc->store_handle, buf_string(path), keylen);
  buf_pool_release(&path);
  return rc;
}
