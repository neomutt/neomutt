/*
 * Copyright (C) 2004 Thomas Glanzmann <sithglan@stud.uni-erlangen.de>
 * Copyright (C) 2004 Tobias Werth <sitowert@stud.uni-erlangen.de>
 * Copyright (C) 2004 Brian Fundakowski Feldman <green@FreeBSD.org>
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

#if HAVE_CONFIG_H
#include "config.h"
#endif				/* HAVE_CONFIG_H */

#if HAVE_VILLA_H
#include <depot.h>
#include <cabin.h>
#include <villa.h>
#elif HAVE_GDBM
#include <gdbm.h>
#elif HAVE_DB4
#include <db.h>
#endif

#include <errno.h>
#include <fcntl.h>
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include "mutt.h"
#include "hcache.h"
#include "hcversion.h"
#ifdef USE_IMAP
#include "message.h"
#endif
#include "mime.h"
#include "mx.h"
#include "lib.h"
#include "md5.h"

#if HAVE_QDBM
static struct header_cache
{
  VILLA *db;
  char *folder;
  unsigned int crc;
} HEADER_CACHE;
#elif HAVE_GDBM
static struct header_cache
{
  GDBM_FILE db;
  char *folder;
  unsigned int crc;
} HEADER_CACHE;
#elif HAVE_DB4
static struct header_cache
{
  DB_ENV *env;
  DB *db;
  unsigned int crc;
  int fd;
  char lockfile[_POSIX_PATH_MAX];
} HEADER_CACHE;

static void mutt_hcache_dbt_init(DBT * dbt, void *data, size_t len);
static void mutt_hcache_dbt_empty_init(DBT * dbt);
#endif

typedef union
{
  struct timeval timeval;
  unsigned long uid_validity;
} validate;

static void *
lazy_malloc(size_t siz)
{
  if (0 < siz && siz < 4096)
    siz = 4096;

  return safe_malloc(siz);
}

static void
lazy_realloc(void *ptr, size_t siz)
{
  void **p = (void **) ptr;

  if (p != NULL && 0 < siz && siz < 4096)
    return;

  safe_realloc(ptr, siz);
}

static unsigned char *
dump_int(unsigned int i, unsigned char *d, int *off)
{
  lazy_realloc(&d, *off + sizeof (int));
  memcpy(d + *off, &i, sizeof (int));
  (*off) += sizeof (int);

  return d;
}

static void
restore_int(unsigned int *i, const unsigned char *d, int *off)
{
  memcpy(i, d + *off, sizeof (int));
  (*off) += sizeof (int);
}

static unsigned char *
dump_char(char *c, unsigned char *d, int *off)
{
  unsigned int size;

  if (c == NULL)
  {
    size = 0;
    d = dump_int(size, d, off);
    return d;
  }

  size = mutt_strlen(c) + 1;
  d = dump_int(size, d, off);
  lazy_realloc(&d, *off + size);
  memcpy(d + *off, c, size);
  *off += size;

  return d;
}

static unsigned char *
dump_char_size(char *c, unsigned char *d, int *off, ssize_t size)
{
  if (c == NULL)
  {
    size = 0;
    d = dump_int(size, d, off);
    return d;
  }

  d = dump_int(size, d, off);
  lazy_realloc(&d, *off + size);
  memcpy(d + *off, c, size);
  *off += size;

  return d;
}

static void
restore_char(char **c, const unsigned char *d, int *off)
{
  unsigned int size;
  restore_int(&size, d, off);

  if (size == 0)
  {
    *c = NULL;
    return;
  }

  *c = safe_malloc(size);
  memcpy(*c, d + *off, size);
  *off += size;
}

static unsigned char *
dump_address(ADDRESS * a, unsigned char *d, int *off)
{
  unsigned int counter = 0;
  unsigned int start_off = *off;

  d = dump_int(0xdeadbeef, d, off);

  while (a)
  {
#ifdef EXACT_ADDRESS
    d = dump_char(a->val, d, off);
#endif
    d = dump_char(a->personal, d, off);
    d = dump_char(a->mailbox, d, off);
    d = dump_int(a->group, d, off);
    a = a->next;
    counter++;
  }

  memcpy(d + start_off, &counter, sizeof (int));

  return d;
}

static void
restore_address(ADDRESS ** a, const unsigned char *d, int *off)
{
  unsigned int counter;

  restore_int(&counter, d, off);

  while (counter)
  {
    *a = safe_malloc(sizeof (ADDRESS));
#ifdef EXACT_ADDRESS
    restore_char(&(*a)->val, d, off);
#endif
    restore_char(&(*a)->personal, d, off);
    restore_char(&(*a)->mailbox, d, off);
    restore_int((unsigned int *) &(*a)->group, d, off);
    a = &(*a)->next;
    counter--;
  }

  *a = NULL;
}

static unsigned char *
dump_list(LIST * l, unsigned char *d, int *off)
{
  unsigned int counter = 0;
  unsigned int start_off = *off;

  d = dump_int(0xdeadbeef, d, off);

  while (l)
  {
    d = dump_char(l->data, d, off);
    l = l->next;
    counter++;
  }

  memcpy(d + start_off, &counter, sizeof (int));

  return d;
}

static void
restore_list(LIST ** l, const unsigned char *d, int *off)
{
  unsigned int counter;

  restore_int(&counter, d, off);

  while (counter)
  {
    *l = safe_malloc(sizeof (LIST));
    restore_char(&(*l)->data, d, off);
    l = &(*l)->next;
    counter--;
  }

  *l = NULL;
}

static unsigned char *
dump_buffer(BUFFER * b, unsigned char *d, int *off)
{
  if (!b)
  {
    d = dump_int(0, d, off);
    return d;
  }
  else
    d = dump_int(1, d, off);

  d = dump_char_size(b->data, d, off, b->dsize + 1);
  d = dump_int(b->dptr - b->data, d, off);
  d = dump_int(b->dsize, d, off);
  d = dump_int(b->destroy, d, off);

  return d;
}

static void
restore_buffer(BUFFER ** b, const unsigned char *d, int *off)
{
  unsigned int used;
  unsigned int offset;
  restore_int(&used, d, off);
  if (!used)
  {
    return;
  }

  *b = safe_malloc(sizeof (BUFFER));

  restore_char(&(*b)->data, d, off);
  restore_int(&offset, d, off);
  (*b)->dptr = (*b)->data + offset;
  restore_int (&used, d, off);
  (*b)->dsize = used;
  restore_int (&used, d, off);
  (*b)->destroy = used;
}

static unsigned char *
dump_parameter(PARAMETER * p, unsigned char *d, int *off)
{
  unsigned int counter = 0;
  unsigned int start_off = *off;

  d = dump_int(0xdeadbeef, d, off);

  while (p)
  {
    d = dump_char(p->attribute, d, off);
    d = dump_char(p->value, d, off);
    p = p->next;
    counter++;
  }

  memcpy(d + start_off, &counter, sizeof (int));

  return d;
}

static void
restore_parameter(PARAMETER ** p, const unsigned char *d, int *off)
{
  unsigned int counter;

  restore_int(&counter, d, off);

  while (counter)
  {
    *p = safe_malloc(sizeof (PARAMETER));
    restore_char(&(*p)->attribute, d, off);
    restore_char(&(*p)->value, d, off);
    p = &(*p)->next;
    counter--;
  }

  *p = NULL;
}

static unsigned char *
dump_body(BODY * c, unsigned char *d, int *off)
{
  lazy_realloc(&d, *off + sizeof (BODY));
  memcpy(d + *off, c, sizeof (BODY));
  *off += sizeof (BODY);

  d = dump_char(c->xtype, d, off);
  d = dump_char(c->subtype, d, off);

  d = dump_parameter(c->parameter, d, off);

  d = dump_char(c->description, d, off);
  d = dump_char(c->form_name, d, off);
  d = dump_char(c->filename, d, off);
  d = dump_char(c->d_filename, d, off);

  return d;
}

static void
restore_body(BODY * c, const unsigned char *d, int *off)
{
  memcpy(c, d + *off, sizeof (BODY));
  *off += sizeof (BODY);

  restore_char(&c->xtype, d, off);
  restore_char(&c->subtype, d, off);

  restore_parameter(&c->parameter, d, off);

  restore_char(&c->description, d, off);
  restore_char(&c->form_name, d, off);
  restore_char(&c->filename, d, off);
  restore_char(&c->d_filename, d, off);
}

static unsigned char *
dump_envelope(ENVELOPE * e, unsigned char *d, int *off)
{
  d = dump_address(e->return_path, d, off);
  d = dump_address(e->from, d, off);
  d = dump_address(e->to, d, off);
  d = dump_address(e->cc, d, off);
  d = dump_address(e->bcc, d, off);
  d = dump_address(e->sender, d, off);
  d = dump_address(e->reply_to, d, off);
  d = dump_address(e->mail_followup_to, d, off);

  d = dump_char(e->list_post, d, off);
  d = dump_char(e->subject, d, off);

  if (e->real_subj)
    d = dump_int(e->real_subj - e->subject, d, off);
  else
    d = dump_int(-1, d, off);

  d = dump_char(e->message_id, d, off);
  d = dump_char(e->supersedes, d, off);
  d = dump_char(e->date, d, off);
  d = dump_char(e->x_label, d, off);

  d = dump_buffer(e->spam, d, off);

  d = dump_list(e->references, d, off);
  d = dump_list(e->in_reply_to, d, off);
  d = dump_list(e->userhdrs, d, off);

  return d;
}

static void
restore_envelope(ENVELOPE * e, const unsigned char *d, int *off)
{
  int real_subj_off;

  restore_address(&e->return_path, d, off);
  restore_address(&e->from, d, off);
  restore_address(&e->to, d, off);
  restore_address(&e->cc, d, off);
  restore_address(&e->bcc, d, off);
  restore_address(&e->sender, d, off);
  restore_address(&e->reply_to, d, off);
  restore_address(&e->mail_followup_to, d, off);

  restore_char(&e->list_post, d, off);
  restore_char(&e->subject, d, off);
  restore_int((unsigned int *) (&real_subj_off), d, off);

  if (0 <= real_subj_off)
    e->real_subj = e->subject + real_subj_off;
  else
    e->real_subj = NULL;

  restore_char(&e->message_id, d, off);
  restore_char(&e->supersedes, d, off);
  restore_char(&e->date, d, off);
  restore_char(&e->x_label, d, off);

  restore_buffer(&e->spam, d, off);

  restore_list(&e->references, d, off);
  restore_list(&e->in_reply_to, d, off);
  restore_list(&e->userhdrs, d, off);
}

static int
crc_matches(const char *d, unsigned int crc)
{
  int off = sizeof (validate);
  unsigned int mycrc = 0;

  if (!d)
    return 0;

  restore_int(&mycrc, (unsigned char *) d, &off);

  return (crc == mycrc);
}

/* Append md5sumed folder to path if path is a directory. */
static const char *
mutt_hcache_per_folder(const char *path, const char *folder,
                       hcache_namer_t namer)
{
  static char hcpath[_POSIX_PATH_MAX];
  struct stat sb;
  MD5_CTX md5;
  unsigned char md5sum[16];
  char* s;
  int ret, plen;

  plen = mutt_strlen (path);

  ret = stat(path, &sb);
  if (ret < 0 && path[plen-1] != '/')
    return path;

  if (ret >= 0 && !S_ISDIR(sb.st_mode))
    return path;

  if (namer)
  {
    snprintf (hcpath, sizeof (hcpath), "%s%s", path,
              path[plen-1] == '/' ? "" : "/");
    if (path[plen-1] != '/')
      plen++;

    ret = namer (folder, hcpath + plen, sizeof (hcpath) - plen);
  }
  else
  {
    MD5Init(&md5);
    MD5Update(&md5, (unsigned char *) folder, strlen(folder));
    MD5Final(md5sum, &md5);

    ret = snprintf(hcpath, _POSIX_PATH_MAX,
                   "%s/%02x%02x%02x%02x%02x%02x%02x%02x"
                   "%02x%02x%02x%02x%02x%02x%02x%02x",
                   path, md5sum[0], md5sum[1], md5sum[2], md5sum[3],
                   md5sum[4], md5sum[5], md5sum[6], md5sum[7], md5sum[8],
                   md5sum[9], md5sum[10], md5sum[11], md5sum[12],
                   md5sum[13], md5sum[14], md5sum[15]);
  }
  
  if (ret <= 0)
    return path;

  if (stat (hcpath, &sb) >= 0)
    return hcpath;

  s = strchr (hcpath + 1, '/');
  while (s)
  {
    /* create missing path components */
    *s = '\0';
    if (stat (hcpath, &sb) < 0 && (errno != ENOENT || mkdir (hcpath, 0777) < 0))
      return path;
    *s = '/';
    s = strchr (s + 1, '/');
  }

  return hcpath;
}

/* This function transforms a header into a char so that it is useable by
 * db_store */
static void *
mutt_hcache_dump(header_cache_t *h, HEADER * header, int *off,
		 unsigned long uid_validity)
{
  unsigned char *d = NULL;
  HEADER nh;
  *off = 0;

  d = lazy_malloc(sizeof (validate));

  if (uid_validity)
    memcpy(d, &uid_validity, sizeof (unsigned long));
  else
  {
    struct timeval now;
    gettimeofday(&now, NULL);
    memcpy(d, &now, sizeof (struct timeval));
  }
  *off += sizeof (validate);

  d = dump_int(h->crc, d, off);

  lazy_realloc(&d, *off + sizeof (HEADER));
  memcpy(&nh, header, sizeof (HEADER));

  /* some fields are not safe to cache */
  nh.tagged = 0;
  nh.changed = 0;
  nh.threaded = 0;
  nh.recip_valid = 0;
  nh.searched = 0;
  nh.matched = 0;
  nh.collapsed = 0;
  nh.limited = 0;
  nh.num_hidden = 0;
  nh.recipient = 0;
  nh.attach_valid = 0;
  nh.path = NULL;
  nh.tree = NULL;
  nh.thread = NULL;
#ifdef MIXMASTER
  nh.chain = NULL;
#endif
#if defined USE_POP || defined USE_IMAP
  nh.data = NULL;
#endif

  memcpy(d + *off, &nh, sizeof (HEADER));
  *off += sizeof (HEADER);

  d = dump_envelope(header->env, d, off);
  d = dump_body(header->content, d, off);
  d = dump_char(header->maildir_flags, d, off);

  return d;
}

HEADER *
mutt_hcache_restore(const unsigned char *d, HEADER ** oh)
{
  int off = 0;
  HEADER *h = mutt_new_header();

  /* skip validate */
  off += sizeof (validate);

  /* skip crc */
  off += sizeof (unsigned int);

  memcpy(h, d + off, sizeof (HEADER));
  off += sizeof (HEADER);

  h->env = mutt_new_envelope();
  restore_envelope(h->env, d, &off);

  h->content = mutt_new_body();
  restore_body(h->content, d, &off);

  restore_char(&h->maildir_flags, d, &off);

  /* this is needed for maildir style mailboxes */
  if (oh)
  {
    h->old = (*oh)->old;
    h->path = safe_strdup((*oh)->path);
    mutt_free_header(oh);
  }

  return h;
}

void *
mutt_hcache_fetch(header_cache_t *h, const char *filename,
		  size_t(*keylen) (const char *fn))
{
  void* data;

  data = mutt_hcache_fetch_raw (h, filename, keylen);

  if (!data || !crc_matches(data, h->crc))
  {
    FREE(&data);
    return NULL;
  }
  
  return data;
}

void *
mutt_hcache_fetch_raw (header_cache_t *h, const char *filename,
                       size_t(*keylen) (const char *fn))
{
#ifndef HAVE_DB4
  char path[_POSIX_PATH_MAX];
  int ksize;
#endif
#ifdef HAVE_QDBM
  char *data = NULL;
#elif HAVE_GDBM
  datum key;
  datum data;
#elif HAVE_DB4
  DBT key;
  DBT data;
#endif
  
  if (!h)
    return NULL;
  
#ifdef HAVE_DB4
  if (filename[0] == '/')
    filename++;

  mutt_hcache_dbt_init(&key, (void *) filename, keylen(filename));
  mutt_hcache_dbt_empty_init(&data);
  data.flags = DB_DBT_MALLOC;
  
  h->db->get(h->db, NULL, &key, &data, 0);
  
  return data.data;
#else
  strncpy(path, h->folder, sizeof (path));
  safe_strcat(path, sizeof (path), filename);

  ksize = strlen (h->folder) + keylen (path + strlen (h->folder));  
#endif
#ifdef HAVE_QDBM
  data = vlget(h->db, path, ksize, NULL);
  
  return data;
#elif HAVE_GDBM
  key.dptr = path;
  key.dsize = ksize;
  
  data = gdbm_fetch(h->db, key);
  
  return data.dptr;
#endif
}

int
mutt_hcache_store(header_cache_t *h, const char *filename, HEADER * header,
		  unsigned long uid_validity,
		  size_t(*keylen) (const char *fn))
{
  char* data;
  int dlen;
  int ret;
  
  if (!h)
    return -1;
  
  data = mutt_hcache_dump(h, header, &dlen, uid_validity);
  ret = mutt_hcache_store_raw (h, filename, data, dlen, keylen);
  
  FREE(&data);
  
  return ret;
}

int
mutt_hcache_store_raw (header_cache_t* h, const char* filename, void* data,
                       size_t dlen, size_t(*keylen) (const char* fn))
{
#ifndef HAVE_DB4
  char path[_POSIX_PATH_MAX];
  int ksize;
#endif
#if HAVE_GDBM
  datum key;
  datum databuf;
#elif HAVE_DB4
  DBT key;
  DBT databuf;
#endif
  
  if (!h)
    return -1;

#if HAVE_DB4
  if (filename[0] == '/')
    filename++;
  
  mutt_hcache_dbt_init(&key, (void *) filename, keylen(filename));
  
  mutt_hcache_dbt_empty_init(&databuf);
  databuf.flags = DB_DBT_USERMEM;
  databuf.data = data;
  databuf.size = dlen;
  databuf.ulen = dlen;
  
  return h->db->put(h->db, NULL, &key, &databuf, 0);
#else
  strncpy(path, h->folder, sizeof (path));
  safe_strcat(path, sizeof (path), filename);

  ksize = strlen(h->folder) + keylen(path + strlen(h->folder));
#endif
#if HAVE_QDBM
  return vlput(h->db, path, ksize, data, dlen, VL_DOVER);
#elif HAVE_GDBM
  key.dptr = path;
  key.dsize = ksize;
  
  databuf.dsize = dlen;
  databuf.dptr = data;
  
  return gdbm_store(h->db, key, databuf, GDBM_REPLACE);
#endif
}

static char* get_foldername(const char *folder) {
  char *p = NULL;
  struct stat st;

  /* if the folder is local, canonify the path to avoid
   * to ensure equivalent paths share the hcache */
  if (stat (folder, &st) == 0)
  {
    p = safe_malloc (_POSIX_PATH_MAX+1);
    if (!realpath (folder, p))
      mutt_str_replace (&p, folder);
  } else
    p = safe_strdup (folder);

  return p;
}

#if HAVE_QDBM
header_cache_t *
mutt_hcache_open(const char *path, const char *folder, hcache_namer_t namer)
{
  struct header_cache *h = safe_calloc(1, sizeof (HEADER_CACHE));
  int    flags = VL_OWRITER | VL_OCREAT;

  h->db = NULL;
  h->folder = get_foldername(folder);
  h->crc = HCACHEVER;

  if (!path || path[0] == '\0')
  {
    FREE(&h->folder);
    FREE(&h);
    return NULL;
  }

  path = mutt_hcache_per_folder(path, h->folder, namer);

  if (option(OPTHCACHECOMPRESS))
    flags |= VL_OZCOMP;

  h->db = vlopen(path, flags, VL_CMPLEX);
  if (h->db)
    return h;
  else
  {
    FREE(&h->folder);
    FREE(&h);

    return NULL;
  }
}

void
mutt_hcache_close(header_cache_t *h)
{
  if (!h)
    return;

  vlclose(h->db);
  FREE(&h->folder);
  FREE(&h);
}

int
mutt_hcache_delete(header_cache_t *h, const char *filename,
		   size_t(*keylen) (const char *fn))
{
  char path[_POSIX_PATH_MAX];
  int ksize;

  if (!h)
    return -1;

  strncpy(path, h->folder, sizeof (path));
  safe_strcat(path, sizeof (path), filename);

  ksize = strlen(h->folder) + keylen(path + strlen(h->folder));

  return vlout(h->db, path, ksize);
}

#elif HAVE_GDBM

header_cache_t *
mutt_hcache_open(const char *path, const char *folder, hcache_namer_t namer)
{
  struct header_cache *h = safe_calloc(1, sizeof (HEADER_CACHE));
  int pagesize = atoi(HeaderCachePageSize) ? atoi(HeaderCachePageSize) : 16384;

  h->db = NULL;
  h->folder = get_foldername(folder);
  h->crc = HCACHEVER;

  if (!path || path[0] == '\0')
  {
    FREE(&h->folder);
    FREE(&h);
    return NULL;
  }

  path = mutt_hcache_per_folder(path, h->folder, namer);

  h->db = gdbm_open((char *) path, pagesize, GDBM_WRCREAT, 00600, NULL);
  if (h->db)
    return h;

  /* if rw failed try ro */
  h->db = gdbm_open((char *) path, pagesize, GDBM_READER, 00600, NULL);
  if (h->db)
    return h;
  else
  {
    FREE(&h->folder);
    FREE(&h);

    return NULL;
  }
}

void
mutt_hcache_close(header_cache_t *h)
{
  if (!h)
    return;

  gdbm_close(h->db);
  FREE(&h->folder);
  FREE(&h);
}

int
mutt_hcache_delete(header_cache_t *h, const char *filename,
		   size_t(*keylen) (const char *fn))
{
  datum key;
  char path[_POSIX_PATH_MAX];

  if (!h)
    return -1;

  strncpy(path, h->folder, sizeof (path));
  safe_strcat(path, sizeof (path), filename);

  key.dptr = path;
  key.dsize = strlen(h->folder) + keylen(path + strlen(h->folder));

  return gdbm_delete(h->db, key);
}
#elif HAVE_DB4

static void
mutt_hcache_dbt_init(DBT * dbt, void *data, size_t len)
{
  dbt->data = data;
  dbt->size = dbt->ulen = len;
  dbt->dlen = dbt->doff = 0;
  dbt->flags = DB_DBT_USERMEM;
}

static void
mutt_hcache_dbt_empty_init(DBT * dbt)
{
  dbt->data = NULL;
  dbt->size = dbt->ulen = dbt->dlen = dbt->doff = 0;
  dbt->flags = 0;
}

header_cache_t *
mutt_hcache_open(const char *path, const char *folder, hcache_namer_t namer)
{
  struct stat sb;
  u_int32_t createflags = DB_CREATE;
  int ret;
  struct header_cache *h = calloc(1, sizeof (HEADER_CACHE));
  int pagesize = atoi(HeaderCachePageSize);
  char* tmp;

  h->crc = HCACHEVER;

  if (!path || path[0] == '\0')
  {
    FREE(&h);
    return NULL;
  }

  tmp = get_foldername (folder);
  path = mutt_hcache_per_folder(path, tmp, namer);
  snprintf(h->lockfile, _POSIX_PATH_MAX, "%s-lock-hack", path);
  FREE(&tmp);

  h->fd = open(h->lockfile, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
  if (h->fd < 0)
  {
    FREE(&h);
    return NULL;
  }

  if (mx_lock_file(h->lockfile, h->fd, 1, 0, 5))
  {
    close(h->fd);
    FREE(&h);
    return NULL;
  }

  ret = db_env_create(&h->env, 0);
  if (ret)
  {
    mx_unlock_file(h->lockfile, h->fd, 0);
    close(h->fd);
    FREE(&h);
    return NULL;
  }

  ret = (*h->env->open)(h->env, NULL, DB_INIT_MPOOL | DB_CREATE | DB_PRIVATE,
	0600);
  if (ret)
  {
    h->env->close(h->env, 0);
    mx_unlock_file(h->lockfile, h->fd, 0);
    close(h->fd);
    FREE(&h);
    return NULL;
  }
  ret = db_create (&h->db, h->env, 0);
  if (ret)
  {
    h->env->close (h->env, 0);
    mx_unlock_file (h->lockfile, h->fd, 0);
    close (h->fd);
    FREE (&h);
    return NULL;
  }

  if (stat(path, &sb) != 0 && errno == ENOENT)
  {
    createflags |= DB_EXCL;
    h->db->set_pagesize(h->db, pagesize);
  }

  ret = (*h->db->open)(h->db, NULL, path, folder, DB_BTREE, createflags, 0600);
  if (ret)
  {
    h->db->close(h->db, 0);
    h->env->close(h->env, 0);
    mx_unlock_file(h->lockfile, h->fd, 0);
    close(h->fd);
    FREE(&h);
    return NULL;
  }

  return h;
}

void
mutt_hcache_close(header_cache_t *h)
{
  if (!h)
    return;

  h->db->close(h->db, 0);
  h->env->close(h->env, 0);
  mx_unlock_file(h->lockfile, h->fd, 0);
  close(h->fd);
  FREE(&h);
}

int
mutt_hcache_delete(header_cache_t *h, const char *filename,
		   size_t(*keylen) (const char *fn))
{
  DBT key;

  if (!h)
    return -1;

  if (filename[0] == '/')
    filename++;

  mutt_hcache_dbt_init(&key, (void *) filename, keylen(filename));
  return h->db->del(h->db, NULL, &key, 0);
}
#endif
