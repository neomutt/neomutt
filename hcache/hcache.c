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
#include <sys/time.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "backend.h"
#include "body.h"
#include "envelope.h"
#include "globals.h"
#include "hcache.h"
#include "hcache/hcversion.h"
#include "header.h"
#include "protos.h"
#include "tags.h"

static unsigned int hcachever = 0x0;

/**
 * struct HeaderCache - header cache structure
 *
 * This struct holds both the backend-agnostic and the backend-specific parts
 * of the header cache. Backend code MUST initialize the fetch, store,
 * delete and close function pointers in hcache_open, and MAY store
 * backend-specific context in the ctx pointer.
 */
struct HeaderCache
{
  char *folder;
  unsigned int crc;
  void *ctx;
};

/**
 * union Validate - Header cache validity
 */
union Validate {
  struct timeval timeval;
  unsigned int uidvalidity;
};

#define HCACHE_BACKEND(name) extern const struct HcacheOps hcache_##name##_ops;
HCACHE_BACKEND_LIST
#undef HCACHE_BACKEND

/* Keep this list sorted as it is in configure.ac to avoid user surprise if no
 * header_cache_backend is specified. */
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

static const struct HcacheOps *hcache_get_backend_ops(const char *backend)
{
  const struct HcacheOps **ops = hcache_ops;

  if (!backend || !*backend)
  {
    return *ops;
  }

  for (; *ops; ++ops)
    if (strcmp(backend, (*ops)->name) == 0)
      break;

  return *ops;
}

#define hcache_get_ops() hcache_get_backend_ops(HeaderCacheBackend)

static void *lazy_malloc(size_t siz)
{
  if (siz < 4096)
    siz = 4096;

  return mutt_mem_malloc(siz);
}

static void lazy_realloc(void *ptr, size_t siz)
{
  void **p = (void **) ptr;

  if (p != NULL && siz < 4096)
    return;

  mutt_mem_realloc(ptr, siz);
}

static unsigned char *dump_int(unsigned int i, unsigned char *d, int *off)
{
  lazy_realloc(&d, *off + sizeof(int));
  memcpy(d + *off, &i, sizeof(int));
  (*off) += sizeof(int);

  return d;
}

static void restore_int(unsigned int *i, const unsigned char *d, int *off)
{
  memcpy(i, d + *off, sizeof(int));
  (*off) += sizeof(int);
}

static unsigned char *dump_char_size(char *c, unsigned char *d, int *off,
                                     ssize_t size, bool convert)
{
  char *p = c;

  if (!c)
  {
    size = 0;
    d = dump_int(size, d, off);
    return d;
  }

  if (convert && !mutt_str_is_ascii(c, size))
  {
    p = mutt_str_substr_dup(c, c + size);
    if (mutt_ch_convert_string(&p, Charset, "utf-8", 0) == 0)
    {
      c = p;
      size = mutt_str_strlen(c) + 1;
    }
  }

  d = dump_int(size, d, off);
  lazy_realloc(&d, *off + size);
  memcpy(d + *off, p, size);
  *off += size;

  if (p != c)
    FREE(&p);

  return d;
}

static unsigned char *dump_char(char *c, unsigned char *d, int *off, bool convert)
{
  return dump_char_size(c, d, off, mutt_str_strlen(c) + 1, convert);
}

static void restore_char(char **c, const unsigned char *d, int *off, bool convert)
{
  unsigned int size;
  restore_int(&size, d, off);

  if (size == 0)
  {
    *c = NULL;
    return;
  }

  *c = mutt_mem_malloc(size);
  memcpy(*c, d + *off, size);
  if (convert && !mutt_str_is_ascii(*c, size))
  {
    char *tmp = mutt_str_strdup(*c);
    if (mutt_ch_convert_string(&tmp, "utf-8", Charset, 0) == 0)
    {
      mutt_str_replace(c, tmp);
    }
    else
    {
      FREE(&tmp);
    }
  }
  *off += size;
}

static unsigned char *dump_address(struct Address *a, unsigned char *d, int *off, bool convert)
{
  unsigned int counter = 0;
  unsigned int start_off = *off;

  d = dump_int(0xdeadbeef, d, off);

  while (a)
  {
    d = dump_char(a->personal, d, off, convert);
    d = dump_char(a->mailbox, d, off, false);
    d = dump_int(a->group, d, off);
    a = a->next;
    counter++;
  }

  memcpy(d + start_off, &counter, sizeof(int));

  return d;
}

static void restore_address(struct Address **a, const unsigned char *d, int *off, bool convert)
{
  unsigned int counter;

  restore_int(&counter, d, off);

  while (counter)
  {
    *a = mutt_addr_new();
    restore_char(&(*a)->personal, d, off, convert);
    restore_char(&(*a)->mailbox, d, off, false);
    restore_int((unsigned int *) &(*a)->group, d, off);
    a = &(*a)->next;
    counter--;
  }

  *a = NULL;
}

/**
 * dump_stailq - Pack a STAILQ into a binary blob
 * @param l       List to read from
 * @param d       Binary blob to add to
 * @param off     Offset into the blob
 * @param convert If true, the strings will be converted to utf-8
 * @retval ptr End of the newly packed binary
 */
static unsigned char *dump_stailq(struct ListHead *l, unsigned char *d, int *off, bool convert)
{
  unsigned int counter = 0;
  unsigned int start_off = *off;

  d = dump_int(0xdeadbeef, d, off);

  struct ListNode *np;
  STAILQ_FOREACH(np, l, entries)
  {
    d = dump_char(np->data, d, off, convert);
    counter++;
  }

  memcpy(d + start_off, &counter, sizeof(int));

  return d;
}

/**
 * restore_stailq - Unpack a STAILQ from a binary blob
 * @param l       List to add to
 * @param d       Binary blob to read from
 * @param off     Offset into blob
 * @param convert If true, the strings will be converted from utf-8
 */
static void restore_stailq(struct ListHead *l, const unsigned char *d, int *off, bool convert)
{
  unsigned int counter;

  restore_int(&counter, d, off);

  struct ListNode *np;
  while (counter)
  {
    np = mutt_list_insert_tail(l, NULL);
    restore_char(&np->data, d, off, convert);
    counter--;
  }
}

static unsigned char *dump_buffer(struct Buffer *b, unsigned char *d, int *off, bool convert)
{
  if (!b)
  {
    d = dump_int(0, d, off);
    return d;
  }
  else
    d = dump_int(1, d, off);

  d = dump_char_size(b->data, d, off, b->dsize + 1, convert);
  d = dump_int(b->dptr - b->data, d, off);
  d = dump_int(b->dsize, d, off);
  d = dump_int(b->destroy, d, off);

  return d;
}

static void restore_buffer(struct Buffer **b, const unsigned char *d, int *off, bool convert)
{
  unsigned int used;
  unsigned int offset;
  restore_int(&used, d, off);
  if (!used)
  {
    return;
  }

  *b = mutt_mem_malloc(sizeof(struct Buffer));

  restore_char(&(*b)->data, d, off, convert);
  restore_int(&offset, d, off);
  (*b)->dptr = (*b)->data + offset;
  restore_int(&used, d, off);
  (*b)->dsize = used;
  restore_int(&used, d, off);
  (*b)->destroy = used;
}

static unsigned char *dump_parameter(struct ParameterList *p, unsigned char *d,
                                     int *off, bool convert)
{
  unsigned int counter = 0;
  unsigned int start_off = *off;

  d = dump_int(0xdeadbeef, d, off);

  struct Parameter *np;
  TAILQ_FOREACH(np, p, entries)
  {
    d = dump_char(np->attribute, d, off, false);
    d = dump_char(np->value, d, off, convert);
    counter++;
  }

  memcpy(d + start_off, &counter, sizeof(int));

  return d;
}

static void restore_parameter(struct ParameterList *p, const unsigned char *d,
                              int *off, bool convert)
{
  unsigned int counter;

  restore_int(&counter, d, off);

  struct Parameter *np;
  while (counter)
  {
    np = mutt_param_new();
    restore_char(&np->attribute, d, off, false);
    restore_char(&np->value, d, off, convert);
    TAILQ_INSERT_TAIL(p, np, entries);
    counter--;
  }
}

static unsigned char *dump_body(struct Body *c, unsigned char *d, int *off, bool convert)
{
  struct Body nb;

  memcpy(&nb, c, sizeof(struct Body));

  /* some fields are not safe to cache */
  nb.content = NULL;
  nb.charset = NULL;
  nb.next = NULL;
  nb.parts = NULL;
  nb.hdr = NULL;
  nb.aptr = NULL;

  lazy_realloc(&d, *off + sizeof(struct Body));
  memcpy(d + *off, &nb, sizeof(struct Body));
  *off += sizeof(struct Body);

  d = dump_char(nb.xtype, d, off, false);
  d = dump_char(nb.subtype, d, off, false);

  d = dump_parameter(&nb.parameter, d, off, convert);

  d = dump_char(nb.description, d, off, convert);
  d = dump_char(nb.form_name, d, off, convert);
  d = dump_char(nb.filename, d, off, convert);
  d = dump_char(nb.d_filename, d, off, convert);

  return d;
}

static void restore_body(struct Body *c, const unsigned char *d, int *off, bool convert)
{
  memcpy(c, d + *off, sizeof(struct Body));
  *off += sizeof(struct Body);

  restore_char(&c->xtype, d, off, false);
  restore_char(&c->subtype, d, off, false);

  TAILQ_INIT(&c->parameter);
  restore_parameter(&c->parameter, d, off, convert);

  restore_char(&c->description, d, off, convert);
  restore_char(&c->form_name, d, off, convert);
  restore_char(&c->filename, d, off, convert);
  restore_char(&c->d_filename, d, off, convert);
}

static unsigned char *dump_envelope(struct Envelope *e, unsigned char *d, int *off, bool convert)
{
  d = dump_address(e->return_path, d, off, convert);
  d = dump_address(e->from, d, off, convert);
  d = dump_address(e->to, d, off, convert);
  d = dump_address(e->cc, d, off, convert);
  d = dump_address(e->bcc, d, off, convert);
  d = dump_address(e->sender, d, off, convert);
  d = dump_address(e->reply_to, d, off, convert);
  d = dump_address(e->mail_followup_to, d, off, convert);

  d = dump_char(e->list_post, d, off, convert);
  d = dump_char(e->subject, d, off, convert);

  if (e->real_subj)
    d = dump_int(e->real_subj - e->subject, d, off);
  else
    d = dump_int(-1, d, off);

  d = dump_char(e->message_id, d, off, false);
  d = dump_char(e->supersedes, d, off, false);
  d = dump_char(e->date, d, off, false);
  d = dump_char(e->x_label, d, off, convert);

  d = dump_buffer(e->spam, d, off, convert);

  d = dump_stailq(&e->references, d, off, false);
  d = dump_stailq(&e->in_reply_to, d, off, false);
  d = dump_stailq(&e->userhdrs, d, off, convert);

#ifdef USE_NNTP
  d = dump_char(e->xref, d, off, false);
  d = dump_char(e->followup_to, d, off, false);
  d = dump_char(e->x_comment_to, d, off, convert);
#endif

  return d;
}

static void restore_envelope(struct Envelope *e, const unsigned char *d, int *off, bool convert)
{
  int real_subj_off;

  restore_address(&e->return_path, d, off, convert);
  restore_address(&e->from, d, off, convert);
  restore_address(&e->to, d, off, convert);
  restore_address(&e->cc, d, off, convert);
  restore_address(&e->bcc, d, off, convert);
  restore_address(&e->sender, d, off, convert);
  restore_address(&e->reply_to, d, off, convert);
  restore_address(&e->mail_followup_to, d, off, convert);

  restore_char(&e->list_post, d, off, convert);
  restore_char(&e->subject, d, off, convert);
  restore_int((unsigned int *) (&real_subj_off), d, off);

  if (real_subj_off >= 0)
    e->real_subj = e->subject + real_subj_off;
  else
    e->real_subj = NULL;

  restore_char(&e->message_id, d, off, false);
  restore_char(&e->supersedes, d, off, false);
  restore_char(&e->date, d, off, false);
  restore_char(&e->x_label, d, off, convert);

  restore_buffer(&e->spam, d, off, convert);

  restore_stailq(&e->references, d, off, false);
  restore_stailq(&e->in_reply_to, d, off, false);
  restore_stailq(&e->userhdrs, d, off, convert);

#ifdef USE_NNTP
  restore_char(&e->xref, d, off, false);
  restore_char(&e->followup_to, d, off, false);
  restore_char(&e->x_comment_to, d, off, convert);
#endif
}

static int crc_matches(const char *d, unsigned int crc)
{
  int off = sizeof(union Validate);
  unsigned int mycrc = 0;

  if (!d)
    return 0;

  restore_int(&mycrc, (unsigned char *) d, &off);

  return (crc == mycrc);
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

  static char dir[_POSIX_PATH_MAX];
  mutt_str_strfcpy(dir, path, sizeof(dir));

  char *p = strrchr(dir, '/');
  if (!p)
    return true;

  *p = '\0';
  if (mutt_file_mkdir(dir, S_IRWXU | S_IRWXG | S_IRWXO) == 0)
    return true;

  mutt_error(_("Can't create %s: %s."), dir, strerror(errno));
  return false;
}

/**
 * hcache_per_folder - Generate the hcache pathname
 * @param path   Base directory, from $header_cache
 * @param folder Mailbox name (including protocol)
 * @param namer  Callback to generate database filename
 * @retval ptr Full pathname to the database (to be generated)
 *             (path must be freed by the caller)
 *
 * Generate the pathname for the hcache database, it will be of the form:
 *     BASE/FOLDER/NAME-SUFFIX
 *
 * * BASE:  Base directory (@a path)
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
  static char hcpath[_POSIX_PATH_MAX];
  char suffix[32] = "";
  struct stat sb;

  int plen = mutt_str_strlen(path);
  int rc = stat(path, &sb);
  int slash = (path[plen - 1] == '/');

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
    char name[_POSIX_PATH_MAX];
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
 * hcache_dump - Serialise a Header object
 *
 * This function transforms a header into a char so that it is useable by
 * db_store.
 */
static void *hcache_dump(header_cache_t *h, struct Header *header, int *off,
                         unsigned int uidvalidity)
{
  struct Header nh;
  bool convert = !CharsetIsUtf8;

  *off = 0;
  unsigned char *d = lazy_malloc(sizeof(union Validate));

  if (uidvalidity == 0)
  {
    struct timeval now;
    gettimeofday(&now, NULL);
    memcpy(d, &now, sizeof(struct timeval));
  }
  else
    memcpy(d, &uidvalidity, sizeof(uidvalidity));
  *off += sizeof(union Validate);

  d = dump_int(h->crc, d, off);

  lazy_realloc(&d, *off + sizeof(struct Header));
  memcpy(&nh, header, sizeof(struct Header));

  /* some fields are not safe to cache */
  nh.tagged = false;
  nh.changed = false;
  nh.threaded = false;
  nh.recip_valid = false;
  nh.searched = false;
  nh.matched = false;
  nh.collapsed = false;
  nh.limited = false;
  nh.num_hidden = 0;
  nh.recipient = 0;
  nh.pair = 0;
  nh.attach_valid = false;
  nh.path = NULL;
  nh.tree = NULL;
  nh.thread = NULL;
  STAILQ_INIT(&nh.tags);
#ifdef MIXMASTER
  STAILQ_INIT(&nh.chain);
#endif
#if defined(USE_POP) || defined(USE_IMAP)
  nh.data = NULL;
#endif

  memcpy(d + *off, &nh, sizeof(struct Header));
  *off += sizeof(struct Header);

  d = dump_envelope(nh.env, d, off, convert);
  d = dump_body(nh.content, d, off, convert);
  d = dump_char(nh.maildir_flags, d, off, convert);

  return d;
}

struct Header *mutt_hcache_restore(const unsigned char *d)
{
  int off = 0;
  struct Header *h = mutt_header_new();
  bool convert = !CharsetIsUtf8;

  /* skip validate */
  off += sizeof(union Validate);

  /* skip crc */
  off += sizeof(unsigned int);

  memcpy(h, d + off, sizeof(struct Header));
  off += sizeof(struct Header);

  h->env = mutt_env_new();
  restore_envelope(h->env, d, &off, convert);

  h->content = mutt_body_new();
  restore_body(h->content, d, &off, convert);

  restore_char(&h->maildir_flags, d, &off, convert);

  return h;
}

static char *get_foldername(const char *folder)
{
  /* if the folder is local, canonify the path to avoid
   * to ensure equivalent paths share the hcache */
  char *p = mutt_mem_malloc(PATH_MAX + 1);
  if (!realpath(folder, p))
    mutt_str_replace(&p, folder);

  return p;
}

header_cache_t *mutt_hcache_open(const char *path, const char *folder, hcache_namer_t namer)
{
  const struct HcacheOps *ops = hcache_get_ops();
  if (!ops)
    return NULL;

  header_cache_t *h = mutt_mem_calloc(1, sizeof(header_cache_t));

  /* Calculate the current hcache version from dynamic configuration */
  if (hcachever == 0x0)
  {
    union {
      unsigned char charval[16];
      unsigned int intval;
    } digest;
    struct Md5Ctx ctx;
    struct ReplaceList *spam = NULL;
    struct RegexList *nospam = NULL;

    hcachever = HCACHEVER;

    mutt_md5_init_ctx(&ctx);

    /* Seed with the compiled-in header structure hash */
    mutt_md5_process_bytes(&hcachever, sizeof(hcachever), &ctx);

    /* Mix in user's spam list */
    for (spam = SpamList; spam; spam = spam->next)
    {
      mutt_md5_process(spam->regex->pattern, &ctx);
      mutt_md5_process(spam->template, &ctx);
    }

    /* Mix in user's nospam list */
    for (nospam = NoSpamList; nospam; nospam = nospam->next)
    {
      mutt_md5_process(nospam->regex->pattern, &ctx);
    }

    /* Get a hash and take its bytes as an (unsigned int) hash version */
    mutt_md5_finish_ctx(&ctx, digest.charval);
    hcachever = digest.intval;
  }

  h->folder = get_foldername(folder);
  h->crc = hcachever;

  if (!path || path[0] == '\0')
  {
    FREE(&h->folder);
    FREE(&h);
    return NULL;
  }

  path = hcache_per_folder(path, h->folder, namer);

  h->ctx = ops->open(path);
  if (h->ctx)
    return h;
  else
  {
    /* remove a possibly incompatible version */
    if (unlink(path) == 0)
    {
      h->ctx = ops->open(path);
      if (h->ctx)
        return h;
    }
    FREE(&h->folder);
    FREE(&h);

    return NULL;
  }
}

void mutt_hcache_close(header_cache_t *h)
{
  const struct HcacheOps *ops = hcache_get_ops();
  if (!h || !ops)
    return;

  ops->close(&h->ctx);
  FREE(&h->folder);
  FREE(&h);
}

void *mutt_hcache_fetch(header_cache_t *h, const char *key, size_t keylen)
{
  void *data = mutt_hcache_fetch_raw(h, key, keylen);
  if (!data)
  {
    return NULL;
  }

  if (!crc_matches(data, h->crc))
  {
    mutt_hcache_free(h, &data);
    return NULL;
  }

  return data;
}

void *mutt_hcache_fetch_raw(header_cache_t *h, const char *key, size_t keylen)
{
  char path[_POSIX_PATH_MAX];
  const struct HcacheOps *ops = hcache_get_ops();

  if (!h || !ops)
    return NULL;

  keylen = snprintf(path, sizeof(path), "%s%s", h->folder, key);

  return ops->fetch(h->ctx, path, keylen);
}

void mutt_hcache_free(header_cache_t *h, void **data)
{
  const struct HcacheOps *ops = hcache_get_ops();

  if (!h || !ops)
    return;

  ops->free(h->ctx, data);
}

int mutt_hcache_store(header_cache_t *h, const char *key, size_t keylen,
                      struct Header *header, unsigned int uidvalidity)
{
  char *data = NULL;
  int dlen;
  int ret;

  if (!h)
    return -1;

  data = hcache_dump(h, header, &dlen, uidvalidity);
  ret = mutt_hcache_store_raw(h, key, keylen, data, dlen);

  FREE(&data);

  return ret;
}

int mutt_hcache_store_raw(header_cache_t *h, const char *key, size_t keylen,
                          void *data, size_t dlen)
{
  char path[_POSIX_PATH_MAX];
  const struct HcacheOps *ops = hcache_get_ops();

  if (!h || !ops)
    return -1;

  keylen = snprintf(path, sizeof(path), "%s%s", h->folder, key);

  return ops->store(h->ctx, path, keylen, data, dlen);
}

int mutt_hcache_delete(header_cache_t *h, const char *key, size_t keylen)
{
  char path[_POSIX_PATH_MAX];
  const struct HcacheOps *ops = hcache_get_ops();

  if (!h)
    return -1;

  keylen = snprintf(path, sizeof(path), "%s%s", h->folder, key);

  return ops->delete (h->ctx, path, keylen);
}

const char *mutt_hcache_backend_list(void)
{
  char tmp[STRING] = { 0 };
  const struct HcacheOps **ops = hcache_ops;
  size_t len = 0;

  for (; *ops; ++ops)
  {
    if (len != 0)
    {
      len += snprintf(tmp + len, STRING - len, ", ");
    }
    len += snprintf(tmp + len, STRING - len, "%s", (*ops)->name);
  }

  return mutt_str_strdup(tmp);
}

int mutt_hcache_is_valid_backend(const char *s)
{
  return hcache_get_backend_ops(s) != NULL;
}
