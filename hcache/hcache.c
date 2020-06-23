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
#include "email/lib.h"
#include "lib.h"
#include "hcache/hcversion.h"
#include "muttlib.h"
#include "serialize.h"
#include "compress/lib.h"
#include "store/lib.h"

#if !(defined(HAVE_BDB) || defined(HAVE_GDBM) || defined(HAVE_KC) ||           \
      defined(HAVE_LMDB) || defined(HAVE_QDBM) || defined(HAVE_ROCKSDB) ||     \
      defined(HAVE_TC) || defined(HAVE_TDB))
#error "No hcache backend defined"
#endif

/* These Config Variables are only used in hcache/hcache.c */
char *C_HeaderCacheBackend; ///< Config: (hcache) Header cache backend to use

static unsigned int hcachever = 0x0;

#define hcache_get_ops() store_get_backend_ops(C_HeaderCacheBackend)

#ifdef USE_HCACHE_COMPRESSION
short C_HeaderCacheCompressLevel; ///< Config: (hcache) Level of compression for method
char *C_HeaderCacheCompressMethod; ///< Config: (hcache) Enable generic hcache database compression

#define compr_get_ops() compress_get_ops(C_HeaderCacheCompressMethod)
#endif

/**
 * header_size - Compute the size of the header with uuid validity
 * and crc.
 */
static size_t header_size(void)
{
  return sizeof(int) + sizeof(uint32_t);
}

/**
 * dump - Serialise an Email object
 * @param hc          Header cache handle
 * @param e           Email to serialise
 * @param off         Size of the binary blob
 * @param uidvalidity IMAP server identifier
 * @retval ptr Binary blob representing the Email
 *
 * This function transforms an Email into a binary string so that it can be
 * saved to a database.
 */
static void *dump(header_cache_t *hc, const struct Email *e, int *off, uint32_t uidvalidity)
{
  struct Email e_dump;
  bool convert = !CharsetIsUtf8;

  *off = 0;
  unsigned char *d = mutt_mem_malloc(4096);

  d = serial_dump_uint32_t((uidvalidity != 0) ? uidvalidity : mutt_date_epoch(), d, off);
  d = serial_dump_int(hc->crc, d, off);

  assert((size_t) *off == header_size());

  lazy_realloc(&d, *off + sizeof(struct Email));
  memcpy(&e_dump, e, sizeof(struct Email));

  /* some fields are not safe to cache */
  e_dump.tagged = false;
  e_dump.changed = false;
  e_dump.threaded = false;
  e_dump.recip_valid = false;
  e_dump.searched = false;
  e_dump.matched = false;
  e_dump.collapsed = false;
  e_dump.limited = false;
  e_dump.num_hidden = 0;
  e_dump.recipient = 0;
  e_dump.pair = 0;
  e_dump.attach_valid = false;
  e_dump.path = NULL;
  e_dump.tree = NULL;
  e_dump.thread = NULL;
  STAILQ_INIT(&e_dump.tags);
#ifdef MIXMASTER
  STAILQ_INIT(&e_dump.chain);
#endif
  e_dump.edata = NULL;

  memcpy(d + *off, &e_dump, sizeof(struct Email));
  *off += sizeof(struct Email);

  d = serial_dump_envelope(e_dump.env, d, off, convert);
  d = serial_dump_body(e_dump.content, d, off, convert);
  d = serial_dump_char(e_dump.maildir_flags, d, off, convert);

  return d;
}

/**
 * restore - Restore an Email from data retrieved from the cache
 * @param d Data retrieved using mutt_hcache_dump
 * @retval ptr Success, the restored header (can't be NULL)
 *
 * @note The returned Email must be free'd by caller code with
 *       email_free()
 */
static struct Email *restore(const unsigned char *d)
{
  int off = 0;
  struct Email *e = email_new();
  bool convert = !CharsetIsUtf8;

  /* skip validate */
  off += sizeof(uint32_t);

  /* skip crc */
  off += sizeof(unsigned int);

  memcpy(e, d + off, sizeof(struct Email));
  off += sizeof(struct Email);

  STAILQ_INIT(&e->tags);
#ifdef MIXMASTER
  STAILQ_INIT(&e->chain);
#endif

  e->env = mutt_env_new();
  serial_restore_envelope(e->env, d, &off, convert);

  e->content = mutt_body_new();
  serial_restore_body(e->content, d, &off, convert);

  serial_restore_char(&e->maildir_flags, d, &off, convert);

  return e;
}

struct RealKey
{
  char key[1024];
  size_t len;
};

/**
 * realkey - Compute the real key used in the backend, taking into account the compression method
 * @param  key    Original key
 * @param  keylen Length of original key
 * @retval ptr Static location holding data and length of the real key
 */
static struct RealKey *realkey(const char *key, size_t keylen)
{
  static struct RealKey rk;
#ifdef USE_HCACHE_COMPRESSION
  if (C_HeaderCacheCompressMethod)
  {
    rk.len = snprintf(rk.key, sizeof(rk.key), "%s-%s", key, compr_get_ops()->name);
  }
  else
#endif
  {
    memcpy(rk.key, key, keylen + 1); // Including NUL byte
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
 * @param hcpath Buffer for the result
 * @param path   Base directory, from $header_cache
 * @param folder Mailbox name (including protocol)
 * @param namer  Callback to generate database filename - Implements ::hcache_namer_t
 * @retval ptr Full pathname to the database (to be generated)
 *             (path must be freed by the caller)
 *
 * Generate the pathname for the hcache database, it will be of the form:
 *     BASE/FOLDER/NAME
 *
 * * BASE:   Base directory (@a path)
 * * FOLDER: Mailbox name (@a folder)
 * * NAME:   Create by @a namer, or md5sum of @a folder
 *
 * This function will create any parent directories needed, so the caller just
 * needs to create the database file.
 *
 * If @a path exists and is a directory, it is used.
 * If @a path has a trailing '/' it is assumed to be a directory.
 * Otherwise @a path is assumed to be a file.
 */
static void hcache_per_folder(struct Buffer *hcpath, const char *path,
                              const char *folder, hcache_namer_t namer)
{
  struct stat sb;

  int plen = mutt_str_len(path);
  int rc = stat(path, &sb);
  bool slash = (path[plen - 1] == '/');

  if (((rc == 0) && !S_ISDIR(sb.st_mode)) || ((rc == -1) && !slash))
  {
    /* An existing file or a non-existing path not ending with a slash */
    mutt_encode_path(hcpath, path);
    return;
  }

  /* We have a directory - no matter whether it exists, or not */
  struct Buffer *hcfile = mutt_buffer_pool_get();
  if (namer)
  {
    namer(folder, hcfile);
    mutt_buffer_concat_path(hcpath, path, mutt_b2s(hcfile));
  }
  else
  {
    unsigned char m[16]; /* binary md5sum */
    struct Buffer *name = mutt_buffer_pool_get();
#ifdef USE_HCACHE_COMPRESSION
    const char *cm = C_HeaderCacheCompressMethod;
    mutt_buffer_printf(name, "%s|%s%s", hcache_get_ops()->name, folder, cm ? cm : "");
#else
    mutt_buffer_printf(name, "%s|%s", hcache_get_ops()->name, folder);
#endif
    mutt_md5(mutt_b2s(name), m);
    mutt_buffer_reset(name);
    mutt_md5_toascii(m, name->data);
    mutt_buffer_printf(hcpath, "%s%s%s", path, slash ? "" : "/", mutt_b2s(name));
    mutt_buffer_pool_release(&name);
  }

  mutt_encode_path(hcpath, mutt_b2s(hcpath));
  create_hcache_dir(mutt_b2s(hcpath));
  mutt_buffer_pool_release(&hcfile);
}

/**
 * get_foldername - Where should the cache be stored?
 * @param folder Path to be canonicalised
 * @retval ptr New string with canonical path
 */
static char *get_foldername(const char *folder)
{
  /* if the folder is local, canonify the path to avoid
   * to ensure equivalent paths share the hcache */
  char *p = mutt_mem_malloc(PATH_MAX + 1);
  if (!realpath(folder, p))
    mutt_str_replace(&p, folder);

  return p;
}

/**
 * mutt_hcache_open - Multiplexor for StoreOps::open
 */
header_cache_t *mutt_hcache_open(const char *path, const char *folder, hcache_namer_t namer)
{
  const struct StoreOps *ops = hcache_get_ops();
  if (!ops)
    return NULL;

  header_cache_t *hc = mutt_mem_calloc(1, sizeof(header_cache_t));

  /* Calculate the current hcache version from dynamic configuration */
  if (hcachever == 0x0)
  {
    union
    {
      unsigned char charval[16];
      unsigned int intval;
    } digest;
    struct Md5Ctx md5ctx;

    hcachever = HCACHEVER;

    mutt_md5_init_ctx(&md5ctx);

    /* Seed with the compiled-in header structure hash */
    mutt_md5_process_bytes(&hcachever, sizeof(hcachever), &md5ctx);

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
    hcachever = digest.intval;
  }

#ifdef USE_HCACHE_COMPRESSION
  if (C_HeaderCacheCompressMethod)
  {
    const struct ComprOps *cops = compr_get_ops();

    hc->cctx = cops->open(C_HeaderCacheCompressLevel);
    if (!hc->cctx)
    {
      FREE(&hc);
      return NULL;
    }

    /* remember the buffer of database backend */
    mutt_debug(LL_DEBUG3, "Header cache will use %s compression\n", cops->name);
  }
#endif

  hc->folder = get_foldername(folder);
  hc->crc = hcachever;

  if (!path || (path[0] == '\0'))
  {
    FREE(&hc->folder);
    FREE(&hc);
    return NULL;
  }

  struct Buffer *hcpath = mutt_buffer_pool_get();
  hcache_per_folder(hcpath, path, hc->folder, namer);

  hc->ctx = ops->open(mutt_b2s(hcpath));
  if (!hc->ctx)
  {
    /* remove a possibly incompatible version */
    if (unlink(mutt_b2s(hcpath)) == 0)
    {
      hc->ctx = ops->open(mutt_b2s(hcpath));
      if (!hc->ctx)
      {
        FREE(&hc->folder);
        FREE(&hc);
      }
    }
  }

  mutt_buffer_pool_release(&hcpath);
  return hc;
}

/**
 * mutt_hcache_close - Multiplexor for StoreOps::close
 */
void mutt_hcache_close(header_cache_t *hc)
{
  const struct StoreOps *ops = hcache_get_ops();
  if (!hc || !ops)
    return;

#ifdef USE_HCACHE_COMPRESSION
  if (C_HeaderCacheCompressMethod)
    compr_get_ops()->close(&hc->cctx);
#endif

  ops->close(&hc->ctx);
  FREE(&hc->folder);
  FREE(&hc);
}

/**
 * mutt_hcache_fetch - Multiplexor for StoreOps::fetch
 */
struct HCacheEntry mutt_hcache_fetch(header_cache_t *hc, const char *key,
                                     size_t keylen, uint32_t uidvalidity)
{
  struct RealKey *rk = realkey(key, keylen);
  struct HCacheEntry entry = { 0 };

  size_t dlen;
  void *data = mutt_hcache_fetch_raw(hc, rk->key, rk->len, &dlen);
  void *to_free = data;
  if (!data)
  {
    goto end;
  }

  /* restore uidvalidity and crc */
  size_t hlen = header_size();
  int off = 0;
  serial_restore_uint32_t(&entry.uidvalidity, data, &off);
  serial_restore_int(&entry.crc, data, &off);
  assert((size_t) off == hlen);
  if (entry.crc != hc->crc || ((uidvalidity != 0) && uidvalidity != entry.uidvalidity))
  {
    goto end;
  }

#ifdef USE_HCACHE_COMPRESSION
  if (C_HeaderCacheCompressMethod)
  {
    const struct ComprOps *cops = compr_get_ops();

    void *dblob = cops->decompress(hc->cctx, (char *) data + hlen, dlen - hlen);
    if (!dblob)
    {
      goto end;
    }
    data = (char *) dblob - hlen; /* restore skips uidvalidity and crc */
  }
#endif

  entry.email = restore(data);

end:
  mutt_hcache_free_raw(hc, &to_free);
  return entry;
}

/**
 * mutt_hcache_fetch_raw - Fetch a message's header from the cache
 * @param[in]  hc     Pointer to the header_cache_t structure got by mutt_hcache_open()
 * @param[in]  key    Message identification string
 * @param[in]  keylen Length of the string pointed to by key
 * @param[out] dlen   Length of the fetched data
 * @retval ptr  Success, the data if found
 * @retval NULL Otherwise
 *
 * @note This function does not perform any check on the validity of the data found.
 * @note The returned data must be free with mutt_hcache_free_raw().
 */
void *mutt_hcache_fetch_raw(header_cache_t *hc, const char *key, size_t keylen, size_t *dlen)
{
  const struct StoreOps *ops = hcache_get_ops();

  if (!hc || !ops)
    return NULL;

  struct Buffer path = mutt_buffer_make(1024);
  keylen = mutt_buffer_printf(&path, "%s%.*s", hc->folder, (int) keylen, key);
  void *blob = ops->fetch(hc->ctx, mutt_b2s(&path), keylen, dlen);
  mutt_buffer_dealloc(&path);
  return blob;
}

/**
 * mutt_hcache_free - Multiplexor for StoreOps::free
 */
void mutt_hcache_free_raw(header_cache_t *hc, void **data)
{
  const struct StoreOps *ops = hcache_get_ops();

  if (!hc || !ops || !data || !*data)
    return;

  ops->free(hc->ctx, data);
}

/**
 * mutt_hcache_store - Multiplexor for StoreOps::store
 */
int mutt_hcache_store(header_cache_t *hc, const char *key, size_t keylen,
                      struct Email *e, uint32_t uidvalidity)
{
  if (!hc)
    return -1;

  int dlen = 0;
  char *data = dump(hc, e, &dlen, uidvalidity);

#ifdef USE_HCACHE_COMPRESSION
  if (C_HeaderCacheCompressMethod)
  {
    /* We don't compress uidvalidity and the crc, so we can check them before
     * decompressing on fetch().  */
    size_t hlen = header_size();

    const struct ComprOps *cops = compr_get_ops();

    /* data / dlen gets ptr to compressed data here */
    size_t clen = dlen;
    void *cdata = cops->compress(hc->cctx, data + hlen, dlen - hlen, &clen);
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
  struct RealKey *rk = realkey(key, keylen);
  int rc = mutt_hcache_store_raw(hc, rk->key, rk->len, data, dlen);

  FREE(&data);

  return rc;
}

/**
 * mutt_hcache_store_raw - store a key / data pair
 * @param hc     Pointer to the header_cache_t structure got by mutt_hcache_open()
 * @param key    Message identification string
 * @param keylen Length of the string pointed to by key
 * @param data   Payload to associate with key
 * @param dlen   Length of the buffer pointed to by the @a data parameter
 * @retval 0   Success
 * @retval num Generic or backend-specific error code otherwise
 */
int mutt_hcache_store_raw(header_cache_t *hc, const char *key, size_t keylen,
                          void *data, size_t dlen)
{
  const struct StoreOps *ops = hcache_get_ops();

  if (!hc || !ops)
    return -1;

  struct Buffer path = mutt_buffer_make(1024);

  keylen = mutt_buffer_printf(&path, "%s%.*s", hc->folder, (int) keylen, key);
  int rc = ops->store(hc->ctx, mutt_b2s(&path), keylen, data, dlen);
  mutt_buffer_dealloc(&path);

  return rc;
}

/**
 * mutt_hcache_delete_header - Multiplexor for StoreOps::delete_header
 */
int mutt_hcache_delete_header(header_cache_t *hc, const char *key, size_t keylen)
{
  const struct StoreOps *ops = hcache_get_ops();
  if (!hc)
    return -1;

  struct Buffer path = mutt_buffer_make(1024);

  keylen = mutt_buffer_printf(&path, "%s%s", hc->folder, key);

  int rc = ops->delete_record(hc->ctx, mutt_b2s(&path), keylen);
  mutt_buffer_dealloc(&path);
  return rc;
}
