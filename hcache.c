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

#if HAVE_QDBM
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
  restore_int((unsigned int *) &(*b)->dsize, d, off);
  restore_int((unsigned int *) &(*b)->destroy, d, off);
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

static unsigned int
crc32(unsigned int crc, unsigned char const *p, size_t len)
{
  int i;
  while (len--)
  {
    crc ^= *p++;
    for (i = 0; i < 8; i++)
      crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
  }
  return crc;
}

static int
generate_crc32()
{
  int crc = 0;
  SPAM_LIST *sp = SpamList;
  RX_LIST *rx = NoSpamList;

  crc = crc32(crc, (unsigned char const *) "$Id$", mutt_strlen("$Id$"));

#if HAVE_LANGINFO_CODESET
  crc = crc32(crc, (unsigned char const *) Charset, mutt_strlen(Charset));
  crc = crc32(crc, (unsigned char const *) "HAVE_LANGINFO_CODESET",
	mutt_strlen("HAVE_LANGINFO_CODESET"));
#endif

#if EXACT_ADDRESS
  crc = crc32(crc, (unsigned char const *) "EXACT_ADDRESS",
	mutt_strlen("EXACT_ADDRESS"));
#endif

#ifdef USE_POP
  crc = crc32(crc, (unsigned char const *) "USE_POP", mutt_strlen("USE_POP"));
#endif

#ifdef MIXMASTER
  crc = crc32(crc, (unsigned char const *) "MIXMASTER",
        mutt_strlen("MIXMASTER"));
#endif

#ifdef USE_IMAP
  crc = crc32(crc, (unsigned char const *) "USE_IMAP", mutt_strlen("USE_IMAP"));
  crc = crc32(crc, (unsigned char const *) ImapHeaders,
        mutt_strlen(ImapHeaders));
#endif
  while (sp)
  {
    crc = crc32(crc, (unsigned char const *) sp->rx->pattern,
	  mutt_strlen(sp->rx->pattern));
    sp = sp->next;
  }

  crc = crc32(crc, (unsigned char const *) "SPAM_SEPERATOR",
	mutt_strlen("SPAM_SEPERATOR"));

  while (rx)
  {
    crc = crc32(crc, (unsigned char const *) rx->rx->pattern,
	  mutt_strlen(rx->rx->pattern));
    rx = rx->next;
  }

  return crc;
}

static int
crc32_matches(const char *d, unsigned int crc)
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
mutt_hcache_per_folder(const char *path, const char *folder)
{
  static char mutt_hcache_per_folder_path[_POSIX_PATH_MAX];
  struct stat path_stat;
  MD5_CTX md5;
  unsigned char md5sum[16];
  int ret;

  ret = stat(path, &path_stat);
  if (ret < 0)
    return path;

  if (!S_ISDIR(path_stat.st_mode))
    return path;

  MD5Init(&md5);
  MD5Update(&md5, (unsigned char *) folder, strlen(folder));
  MD5Final(md5sum, &md5);

  ret = snprintf(mutt_hcache_per_folder_path, _POSIX_PATH_MAX,
		 "%s/%02x%02x%02x%02x%02x%02x%02x%02x"
		 "%02x%02x%02x%02x%02x%02x%02x%02x",
		 path, md5sum[0], md5sum[1], md5sum[2], md5sum[3],
		 md5sum[4], md5sum[5], md5sum[6], md5sum[7], md5sum[8],
		 md5sum[9], md5sum[10], md5sum[11], md5sum[12],
		 md5sum[13], md5sum[14], md5sum[15]);

  if (ret <= 0)
    return path;

  return mutt_hcache_per_folder_path;
}

/* This function transforms a header into a char so that it is useable by
 * db_store */
static void *
mutt_hcache_dump(void *_db, HEADER * h, int *off,
		 unsigned long uid_validity)
{
  struct header_cache *db = _db;
  unsigned char *d = NULL;
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

  d = dump_int(db->crc, d, off);

  lazy_realloc(&d, *off + sizeof (HEADER));
  memcpy(d + *off, h, sizeof (HEADER));
  *off += sizeof (HEADER);

  d = dump_envelope(h->env, d, off);
  d = dump_body(h->content, d, off);
  d = dump_char(h->maildir_flags, d, off);

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

#if HAVE_QDBM
void *
mutt_hcache_open(const char *path, const char *folder)
{
  struct header_cache *h = safe_calloc(1, sizeof (HEADER_CACHE));
  int    flags = VL_OWRITER | VL_OCREAT;
  h->db = NULL;
  h->folder = safe_strdup(folder);
  h->crc = generate_crc32();

  if (!path || path[0] == '\0')
  {
    FREE(&h->folder);
    FREE(&h);
    return NULL;
  }

  path = mutt_hcache_per_folder(path, folder);

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
mutt_hcache_close(void *db)
{
  struct header_cache *h = db;

  if (!h)
    return;

  vlclose(h->db);
  FREE(&h->folder);
  FREE(&h);
}

void *
mutt_hcache_fetch(void *db, const char *filename,
		  size_t(*keylen) (const char *fn))
{
  struct header_cache *h = db;
  char path[_POSIX_PATH_MAX];
  int ksize;
  char *data = NULL;

  if (!h)
    return NULL;

  strncpy(path, h->folder, sizeof (path));
  safe_strcat(path, sizeof (path), filename);

  ksize = strlen(h->folder) + keylen(path + strlen(h->folder));

  data = vlget(h->db, path, ksize, NULL);

  if (! crc32_matches(data, h->crc))
  {
    FREE(&data);
    return NULL;
  }

  return data;
}

int
mutt_hcache_store(void *db, const char *filename, HEADER * header,
		  unsigned long uid_validity,
		  size_t(*keylen) (const char *fn))
{
  struct header_cache *h = db;
  char path[_POSIX_PATH_MAX];
  int ret;
  int ksize, dsize;
  char *data = NULL;

  if (!h)
    return -1;

  strncpy(path, h->folder, sizeof (path));
  safe_strcat(path, sizeof (path), filename);

  ksize = strlen(h->folder) + keylen(path + strlen(h->folder));

  data  = mutt_hcache_dump(db, header, &dsize, uid_validity);

  ret = vlput(h->db, path, ksize, data, dsize, VL_DOVER);

  FREE(&data);

  return ret;
}

int
mutt_hcache_delete(void *db, const char *filename,
		   size_t(*keylen) (const char *fn))
{
  struct header_cache *h = db;
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

void *
mutt_hcache_open(const char *path, const char *folder)
{
  struct header_cache *h = safe_calloc(1, sizeof (HEADER_CACHE));
  int pagesize = atoi(HeaderCachePageSize) ? atoi(HeaderCachePageSize) : 16384;
  h->db = NULL;
  h->folder = safe_strdup(folder);
  h->crc = generate_crc32();

  if (!path || path[0] == '\0')
  {
    FREE(&h->folder);
    FREE(&h);
    return NULL;
  }

  path = mutt_hcache_per_folder(path, folder);

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
mutt_hcache_close(void *db)
{
  struct header_cache *h = db;

  if (!h)
    return;

  gdbm_close(h->db);
  FREE(&h->folder);
  FREE(&h);
}

void *
mutt_hcache_fetch(void *db, const char *filename,
		  size_t(*keylen) (const char *fn))
{
  struct header_cache *h = db;
  datum key;
  datum data;
  char path[_POSIX_PATH_MAX];

  if (!h)
    return NULL;

  strncpy(path, h->folder, sizeof (path));
  safe_strcat(path, sizeof (path), filename);

  key.dptr = path;
  key.dsize = strlen(h->folder) + keylen(path + strlen(h->folder));

  data = gdbm_fetch(h->db, key);

  if (!crc32_matches(data.dptr, h->crc))
  {
    FREE(&data.dptr);
    return NULL;
  }

  return data.dptr;
}

int
mutt_hcache_store(void *db, const char *filename, HEADER * header,
		  unsigned long uid_validity,
		  size_t(*keylen) (const char *fn))
{
  struct header_cache *h = db;
  datum key;
  datum data;
  char path[_POSIX_PATH_MAX];
  int ret;

  if (!h)
    return -1;

  strncpy(path, h->folder, sizeof (path));
  safe_strcat(path, sizeof (path), filename);

  key.dptr = path;
  key.dsize = strlen(h->folder) + keylen(path + strlen(h->folder));

  data.dptr = mutt_hcache_dump(db, header, &data.dsize, uid_validity);

  ret = gdbm_store(h->db, key, data, GDBM_REPLACE);

  FREE(&data.dptr);

  return ret;
}

int
mutt_hcache_delete(void *db, const char *filename,
		   size_t(*keylen) (const char *fn))
{
  datum key;
  struct header_cache *h = db;
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

void *
mutt_hcache_open(const char *path, const char *folder)
{
  struct stat sb;
  u_int32_t createflags = DB_CREATE;
  int ret;
  struct header_cache *h = calloc(1, sizeof (HEADER_CACHE));
  int pagesize = atoi(HeaderCachePageSize);

  h->crc = generate_crc32();

  if (!path || path[0] == '\0')
  {
    FREE(&h);
    return NULL;
  }

  path = mutt_hcache_per_folder(path, folder);

  snprintf(h->lockfile, _POSIX_PATH_MAX, "%s-lock-hack", path);

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
  if (!ret)
  {
    ret = db_create(&h->db, h->env, 0);
    if (ret)
    {
      h->env->close(h->env, 0);
      mx_unlock_file(h->lockfile, h->fd, 0);
      close(h->fd);
      FREE(&h);
      return NULL;
    }
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
mutt_hcache_close(void *db)
{
  struct header_cache *h = db;

  if (!h)
    return;

  h->db->close(h->db, 0);
  h->env->close(h->env, 0);
  mx_unlock_file(h->lockfile, h->fd, 0);
  close(h->fd);
  FREE(&h);
}

void *
mutt_hcache_fetch(void *db, const char *filename,
		  size_t(*keylen) (const char *fn))
{
  DBT key;
  DBT data;
  struct header_cache *h = db;

  if (!h)
    return NULL;

  filename++;			/* skip '/' */

  mutt_hcache_dbt_init(&key, (void *) filename, keylen(filename));
  mutt_hcache_dbt_empty_init(&data);
  data.flags = DB_DBT_MALLOC;

  h->db->get(h->db, NULL, &key, &data, 0);

  if (!crc32_matches(data.data, h->crc))
  {
    FREE(&data.data);
    return NULL;
  }

  return data.data;
}

int
mutt_hcache_store(void *db, const char *filename, HEADER * header,
		  unsigned long uid_validity,
		  size_t(*keylen) (const char *fn))
{
  DBT key;
  DBT data;
  int ret;
  struct header_cache *h = db;

  if (!h)
    return -1;

  filename++;			/* skip '/' */

  mutt_hcache_dbt_init(&key, (void *) filename, keylen(filename));

  mutt_hcache_dbt_empty_init(&data);
  data.flags = DB_DBT_USERMEM;
  data.data = mutt_hcache_dump(db, header, (signed int *) &data.size,
	      uid_validity);
  data.ulen = data.size;

  ret = h->db->put(h->db, NULL, &key, &data, 0);

  FREE(&data.data);

  return ret;
}

int
mutt_hcache_delete(void *db, const char *filename,
		   size_t(*keylen) (const char *fn))
{
  DBT key;
  struct header_cache *h = db;

  if (!h)
    return -1;

  filename++;			/* skip '/' */

  mutt_hcache_dbt_init(&key, (void *) filename, keylen(filename));
  return h->db->del(h->db, NULL, &key, 0);
}
#endif
