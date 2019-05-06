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
 * @page hc_hcache Header cache multiplexor
 *
 * This module implements the gateway between the user visible part of the
 * header cache API and the backend specific API. Also, this module implements
 * the serialization/deserialization routines for the Header structure.
 */

#include "config.h"
#include "muttlib.h"
#include "serialize.h"

#if !(defined(HAVE_BDB) || defined(HAVE_GDBM) || defined(HAVE_KC) ||           \
      defined(HAVE_LMDB) || defined(HAVE_QDBM) || defined(HAVE_TC))
#error "No hcache backend defined"
#endif

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "email/lib.h"
#include "backend.h"
#include "hcache.h"
#include "hcache/hcversion.h"

/* These Config Variables are only used in hcache/hcache.c */
char *C_HeaderCacheBackend; ///< Config: (hcache) Header cache backend to use

static unsigned int hcachever = 0x0;

#define HCACHE_BACKEND(name) extern const struct HcacheOps hcache_##name##_ops;
HCACHE_BACKEND(bdb)
HCACHE_BACKEND(gdbm)
HCACHE_BACKEND(kyotocabinet)
HCACHE_BACKEND(lmdb)
HCACHE_BACKEND(qdbm)
HCACHE_BACKEND(tokyocabinet)
#undef HCACHE_BACKEND

#define hcache_get_ops() hcache_get_backend_ops(C_HeaderCacheBackend)

/**
 * hcache_ops - Backend implementations
 *
 * Keep this list sorted as it is in configure.ac to avoid user surprise if no
 * header_cache_backend is specified.
 */
const struct HcacheOps *hcache_ops[] = {
#ifdef HAVE_TC
  &hcache_tokyocabinet_ops,
#endif
#ifdef HAVE_KC
  &hcache_kyotocabinet_ops,
#endif
#ifdef HAVE_QDBM
  &hcache_qdbm_ops,
#endif
#ifdef HAVE_GDBM
  &hcache_gdbm_ops,
#endif
#ifdef HAVE_BDB
  &hcache_bdb_ops,
#endif
#ifdef HAVE_LMDB
  &hcache_lmdb_ops,
#endif
  NULL,
};

/**
 * hcache_get_backend_ops - Get the API functions for an hcache backend
 * @param backend Name of the backend
 * @retval ptr Set of function pointers
 */
static const struct HcacheOps *hcache_get_backend_ops(const char *backend)
{
  const struct HcacheOps **ops = hcache_ops;

  if (!backend || !*backend)
  {
    return *ops;
  }

  for (; *ops; ops++)
    if (strcmp(backend, (*ops)->name) == 0)
      break;

  return *ops;
}

/**
 * crc_matches - Is the CRC number correct?
 * @param d   Binary blob to read CRC from
 * @param crc CRC to compare
 * @retval num 1 if true, 0 if not
 */
static bool crc_matches(const char *d, unsigned int crc)
{
  if (!d)
    return false;

  unsigned int mycrc = *(unsigned int *) (d + sizeof(union Validate));

  return crc == mycrc;
}

/**
 * create_hcache_dir - Create parent dirs for the hcache database
 * @param path Database filename
 * @retval true Success
 * @retval false Failure (errno set)
 */
static bool create_hcache_dir(const char *path)
{
  if (!path)
    return false;

  static char dir[PATH_MAX];
  mutt_str_strfcpy(dir, path, sizeof(dir));

  char *p = strrchr(dir, '/');
  if (!p)
    return true;

  *p = '\0';
  if (mutt_file_mkdir(dir, S_IRWXU | S_IRWXG | S_IRWXO) == 0)
    return true;

  mutt_error(_("Can't create %s: %s"), dir, strerror(errno));
  return false;
}

/**
 * hcache_per_folder - Generate the hcache pathname
 * @param path   Base directory, from $header_cache
 * @param folder Mailbox name (including protocol)
 * @param namer  Callback to generate database filename - Implements ::hcache_namer_t
 * @retval ptr Full pathname to the database (to be generated)
 *             (path must be freed by the caller)
 *
 * Generate the pathname for the hcache database, it will be of the form:
 *     BASE/FOLDER/NAME-SUFFIX
 *
 * * BASE:   Base directory (@a path)
 * * FOLDER: Mailbox name (@a folder)
 * * NAME:   Create by @a namer, or md5sum of @a folder
 * * SUFFIX: Character set (if ICONV isn't being used)
 *
 * This function will create any parent directories needed, so the caller just
 * needs to create the database file.
 *
 * If @a path exists and is a directory, it is used.
 * If @a path has a trailing '/' it is assumed to be a directory.
 * If ICONV isn't being used, then a suffix is added to the path, e.g. '-utf-8'.
 * Otherwise @a path is assumed to be a file.
 */
static const char *hcache_per_folder(const char *path, const char *folder, hcache_namer_t namer)
{
  static char hcpath[PATH_MAX];
  char suffix[32] = "";
  struct stat sb;

  int plen = mutt_str_strlen(path);
  int rc = stat(path, &sb);
  bool slash = (path[plen - 1] == '/');

  if (((rc == 0) && !S_ISDIR(sb.st_mode)) || ((rc == -1) && !slash))
  {
    /* An existing file or a non-existing path not ending with a slash */
    snprintf(hcpath, sizeof(hcpath), "%s%s", path, suffix);
    mutt_encode_path(hcpath, sizeof(hcpath), hcpath);
    return hcpath;
  }

  /* We have a directory - no matter whether it exists, or not */

  if (namer)
  {
    /* We have a mailbox-specific namer function */
    snprintf(hcpath, sizeof(hcpath), "%s%s", path, slash ? "" : "/");
    if (!slash)
      plen++;

    rc = namer(folder, hcpath + plen, sizeof(hcpath) - plen);
  }
  else
  {
    unsigned char m[16]; /* binary md5sum */
    char name[PATH_MAX];
    snprintf(name, sizeof(name), "%s|%s", hcache_get_ops()->name, folder);
    mutt_md5(name, m);
    mutt_md5_toascii(m, name);
    rc = snprintf(hcpath, sizeof(hcpath), "%s%s%s%s", path, slash ? "" : "/", name, suffix);
  }

  mutt_encode_path(hcpath, sizeof(hcpath), hcpath);

  if (rc < 0) /* namer or fprintf failed.. should not happen */
    return path;

  create_hcache_dir(hcpath);
  return hcpath;
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
 * mutt_hcache_open - Multiplexor for HcacheOps::open
 */
header_cache_t *mutt_hcache_open(const char *path, const char *folder, hcache_namer_t namer)
{
  const struct HcacheOps *ops = hcache_get_ops();
  if (!ops)
    return NULL;

  header_cache_t *hc = mutt_mem_calloc(1, sizeof(header_cache_t));

  /* Calculate the current hcache version from dynamic configuration */
  if (hcachever == 0x0)
  {
    union {
      unsigned char charval[16];
      unsigned int intval;
    } digest;
    struct Md5Ctx md5ctx;

    hcachever = HCACHEVER;

    mutt_md5_init_ctx(&md5ctx);

    /* Seed with the compiled-in header structure hash */
    mutt_md5_process_bytes(&hcachever, sizeof(hcachever), &md5ctx);

    /* Mix in user's spam list */
    struct ReplaceListNode *sp = NULL;
    STAILQ_FOREACH(sp, &SpamList, entries)
    {
      mutt_md5_process(sp->regex->pattern, &md5ctx);
      mutt_md5_process(sp->template, &md5ctx);
    }

    /* Mix in user's nospam list */
    struct RegexListNode *np = NULL;
    STAILQ_FOREACH(np, &NoSpamList, entries)
    {
      mutt_md5_process(np->regex->pattern, &md5ctx);
    }

    /* Get a hash and take its bytes as an (unsigned int) hash version */
    mutt_md5_finish_ctx(&md5ctx, digest.charval);
    hcachever = digest.intval;
  }

  hc->folder = get_foldername(folder);
  hc->crc = hcachever;

  if (!path || (path[0] == '\0'))
  {
    FREE(&hc->folder);
    FREE(&hc);
    return NULL;
  }

  path = hcache_per_folder(path, hc->folder, namer);

  hc->ctx = ops->open(path);
  if (hc->ctx)
    return hc;
  else
  {
    /* remove a possibly incompatible version */
    if (unlink(path) == 0)
    {
      hc->ctx = ops->open(path);
      if (hc->ctx)
        return hc;
    }
    FREE(&hc->folder);
    FREE(&hc);

    return NULL;
  }
}

/**
 * mutt_hcache_close - Multiplexor for HcacheOps::close
 */
void mutt_hcache_close(header_cache_t *hc)
{
  const struct HcacheOps *ops = hcache_get_ops();
  if (!hc || !ops)
    return;

  ops->close(&hc->ctx);
  FREE(&hc->folder);
  FREE(&hc);
}

/**
 * mutt_hcache_fetch - Multiplexor for HcacheOps::fetch
 */
void *mutt_hcache_fetch(header_cache_t *hc, const char *key, size_t keylen)
{
  void *data = mutt_hcache_fetch_raw(hc, key, keylen);
  if (!data)
  {
    return NULL;
  }

  if (!crc_matches(data, hc->crc))
  {
    mutt_hcache_free(hc, &data);
    return NULL;
  }

  return data;
}

/**
 * mutt_hcache_fetch_raw - Fetch a message's header from the cache
 * @param hc     Pointer to the header_cache_t structure got by mutt_hcache_open
 * @param key    Message identification string
 * @param keylen Length of the string pointed to by key
 * @retval ptr  Success, the data if found
 * @retval NULL Otherwise
 *
 * @note This function does not perform any check on the validity of the data
 *       found.
 * @note The returned pointer must be freed by calling mutt_hcache_free. This
 *       must be done before closing the header cache with mutt_hcache_close.
 */
void *mutt_hcache_fetch_raw(header_cache_t *hc, const char *key, size_t keylen)
{
  char path[PATH_MAX];
  const struct HcacheOps *ops = hcache_get_ops();

  if (!hc || !ops)
    return NULL;

  keylen = snprintf(path, sizeof(path), "%s%s", hc->folder, key);

  return ops->fetch(hc->ctx, path, keylen);
}

/**
 * mutt_hcache_free - Multiplexor for HcacheOps::free
 */
void mutt_hcache_free(header_cache_t *hc, void **data)
{
  const struct HcacheOps *ops = hcache_get_ops();

  if (!hc || !ops)
    return;

  ops->free(hc->ctx, data);
}

/**
 * mutt_hcache_store - Multiplexor for HcacheOps::store
 */
int mutt_hcache_store(header_cache_t *hc, const char *key, size_t keylen,
                      struct Email *e, unsigned int uidvalidity)
{
  if (!hc)
    return -1;

  int dlen = 0;

  char *data = mutt_hcache_dump(hc, e, &dlen, uidvalidity);
  int rc = mutt_hcache_store_raw(hc, key, keylen, data, dlen);

  FREE(&data);

  return rc;
}

/**
 * mutt_hcache_store_raw - store a key / data pair
 * @param hc     Pointer to the header_cache_t structure got by mutt_hcache_open
 * @param key    Message identification string
 * @param keylen Length of the string pointed to by key
 * @param data   Payload to associate with key
 * @param dlen   Length of the buffer pointed to by the @a data parameter
 * @retval 0   success
 * @retval num Generic or backend-specific error code otherwise
 */
int mutt_hcache_store_raw(header_cache_t *hc, const char *key, size_t keylen,
                          void *data, size_t dlen)
{
  char path[PATH_MAX];
  const struct HcacheOps *ops = hcache_get_ops();

  if (!hc || !ops)
    return -1;

  keylen = snprintf(path, sizeof(path), "%s%s", hc->folder, key);

  return ops->store(hc->ctx, path, keylen, data, dlen);
}

/**
 * mutt_hcache_delete - Multiplexor for HcacheOps::delete
 */
int mutt_hcache_delete(header_cache_t *hc, const char *key, size_t keylen)
{
  char path[PATH_MAX];
  const struct HcacheOps *ops = hcache_get_ops();

  if (!hc)
    return -1;

  keylen = snprintf(path, sizeof(path), "%s%s", hc->folder, key);

  return ops->delete (hc->ctx, path, keylen);
}

/**
 * mutt_hcache_backend_list - Get a list of backend names
 * @retval ptr Comma-space-separated list of names
 *
 * The caller should free the string.
 */
const char *mutt_hcache_backend_list(void)
{
  char tmp[256] = { 0 };
  const struct HcacheOps **ops = hcache_ops;
  size_t len = 0;

  for (; *ops; ops++)
  {
    if (len != 0)
    {
      len += snprintf(tmp + len, sizeof(tmp) - len, ", ");
    }
    len += snprintf(tmp + len, sizeof(tmp) - len, "%s", (*ops)->name);
  }

  return mutt_str_strdup(tmp);
}

/**
 * mutt_hcache_is_valid_backend - Is the string a valid hcache backend
 * @param s String identifying a backend
 * @retval true  s is recognized as a valid backend
 * @retval false otherwise
 */
bool mutt_hcache_is_valid_backend(const char *s)
{
  return hcache_get_backend_ops(s);
}
