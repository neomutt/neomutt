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
#elif HAVE_TC
#include <tcbdb.h>
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
#include "mx.h"
#include "lib.h"
#include "md5.h"
#include "rfc822.h"

unsigned int hcachever = 0x0;

#if HAVE_QDBM
struct header_cache
{
  VILLA *db;
  char *folder;
  unsigned int crc;
};
#elif HAVE_TC
struct header_cache
{
  TCBDB *db;
  char *folder;
  unsigned int crc;
};
#elif HAVE_GDBM
struct header_cache
{
  GDBM_FILE db;
  char *folder;
  unsigned int crc;
};
#elif HAVE_DB4
struct header_cache
{
  DB_ENV *env;
  DB *db;
  char *folder;
  unsigned int crc;
  int fd;
  char lockfile[_POSIX_PATH_MAX];
};

static void mutt_hcache_dbt_init(DBT * dbt, void *data, size_t len);
static void mutt_hcache_dbt_empty_init(DBT * dbt);
#endif

typedef union
{
  struct timeval timeval;
  unsigned int uidvalidity;
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

static inline int is_ascii (const char *p, size_t len) {
  register const char *s = p;
  while (s && (unsigned) (s - p) < len) {
    if ((*s & 0x80) != 0)
      return 0;
    s++;
  }
  return 1;
}

static unsigned char *
dump_char_size(char *c, unsigned char *d, int *off, ssize_t size, int convert)
{
  char *p = c;

  if (c == NULL)
  {
    size = 0;
    d = dump_int(size, d, off);
    return d;
  }

  if (convert && !is_ascii (c, size)) {
    p = mutt_substrdup (c, c + size);
    if (mutt_convert_string (&p, Charset, "utf-8", 0) == 0) {
      c = p;
      size = mutt_strlen (c) + 1;
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

static unsigned char *
dump_char(char *c, unsigned char *d, int *off, int convert)
{
  return dump_char_size (c, d, off, mutt_strlen (c) + 1, convert);
}

static void
restore_char(char **c, const unsigned char *d, int *off, int convert)
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
  if (convert && !is_ascii (*c, size)) {
    char *tmp = safe_strdup (*c);
    if (mutt_convert_string (&tmp, "utf-8", Charset, 0) == 0) {
      mutt_str_replace (c, tmp);
    } else {
      FREE(&tmp);
    }
  }
  *off += size;
}

static unsigned char *
dump_address(ADDRESS * a, unsigned char *d, int *off, int convert)
{
  unsigned int counter = 0;
  unsigned int start_off = *off;

  d = dump_int(0xdeadbeef, d, off);

  while (a)
  {
#ifdef EXACT_ADDRESS
    d = dump_char(a->val, d, off, convert);
#endif
    d = dump_char(a->personal, d, off, convert);
    d = dump_char(a->mailbox, d, off, 0);
    d = dump_int(a->group, d, off);
    a = a->next;
    counter++;
  }

  memcpy(d + start_off, &counter, sizeof (int));

  return d;
}

static void
restore_address(ADDRESS ** a, const unsigned char *d, int *off, int convert)
{
  unsigned int counter;

  restore_int(&counter, d, off);

  while (counter)
  {
    *a = rfc822_new_address();
#ifdef EXACT_ADDRESS
    restore_char(&(*a)->val, d, off, convert);
#endif
    restore_char(&(*a)->personal, d, off, convert);
    restore_char(&(*a)->mailbox, d, off, 0);
    restore_int((unsigned int *) &(*a)->group, d, off);
    a = &(*a)->next;
    counter--;
  }

  *a = NULL;
}

static unsigned char *
dump_list(LIST * l, unsigned char *d, int *off, int convert)
{
  unsigned int counter = 0;
  unsigned int start_off = *off;

  d = dump_int(0xdeadbeef, d, off);

  while (l)
  {
    d = dump_char(l->data, d, off, convert);
    l = l->next;
    counter++;
  }

  memcpy(d + start_off, &counter, sizeof (int));

  return d;
}

static void
restore_list(LIST ** l, const unsigned char *d, int *off, int convert)
{
  unsigned int counter;

  restore_int(&counter, d, off);

  while (counter)
  {
    *l = safe_malloc(sizeof (LIST));
    restore_char(&(*l)->data, d, off, convert);
    l = &(*l)->next;
    counter--;
  }

  *l = NULL;
}

static unsigned char *
dump_buffer(BUFFER * b, unsigned char *d, int *off, int convert)
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

static void
restore_buffer(BUFFER ** b, const unsigned char *d, int *off, int convert)
{
  unsigned int used;
  unsigned int offset;
  restore_int(&used, d, off);
  if (!used)
  {
    return;
  }

  *b = safe_malloc(sizeof (BUFFER));

  restore_char(&(*b)->data, d, off, convert);
  restore_int(&offset, d, off);
  (*b)->dptr = (*b)->data + offset;
  restore_int (&used, d, off);
  (*b)->dsize = used;
  restore_int (&used, d, off);
  (*b)->destroy = used;
}

static unsigned char *
dump_parameter(PARAMETER * p, unsigned char *d, int *off, int convert)
{
  unsigned int counter = 0;
  unsigned int start_off = *off;

  d = dump_int(0xdeadbeef, d, off);

  while (p)
  {
    d = dump_char(p->attribute, d, off, 0);
    d = dump_char(p->value, d, off, convert);
    p = p->next;
    counter++;
  }

  memcpy(d + start_off, &counter, sizeof (int));

  return d;
}

static void
restore_parameter(PARAMETER ** p, const unsigned char *d, int *off, int convert)
{
  unsigned int counter;

  restore_int(&counter, d, off);

  while (counter)
  {
    *p = safe_malloc(sizeof (PARAMETER));
    restore_char(&(*p)->attribute, d, off, 0);
    restore_char(&(*p)->value, d, off, convert);
    p = &(*p)->next;
    counter--;
  }

  *p = NULL;
}

static unsigned char *
dump_body(BODY * c, unsigned char *d, int *off, int convert)
{
  BODY nb;

  memcpy (&nb, c, sizeof (BODY));

  /* some fields are not safe to cache */
  nb.content = NULL;
  nb.charset = NULL;
  nb.next = NULL;
  nb.parts = NULL;
  nb.hdr = NULL;
  nb.aptr = NULL;

  lazy_realloc(&d, *off + sizeof (BODY));
  memcpy(d + *off, &nb, sizeof (BODY));
  *off += sizeof (BODY);

  d = dump_char(nb.xtype, d, off, 0);
  d = dump_char(nb.subtype, d, off, 0);

  d = dump_parameter(nb.parameter, d, off, convert);

  d = dump_char(nb.description, d, off, convert);
  d = dump_char(nb.form_name, d, off, convert);
  d = dump_char(nb.filename, d, off, convert);
  d = dump_char(nb.d_filename, d, off, convert);

  return d;
}

static void
restore_body(BODY * c, const unsigned char *d, int *off, int convert)
{
  memcpy(c, d + *off, sizeof (BODY));
  *off += sizeof (BODY);

  restore_char(&c->xtype, d, off, 0);
  restore_char(&c->subtype, d, off, 0);

  restore_parameter(&c->parameter, d, off, convert);

  restore_char(&c->description, d, off, convert);
  restore_char(&c->form_name, d, off, convert);
  restore_char(&c->filename, d, off, convert);
  restore_char(&c->d_filename, d, off, convert);
}

static unsigned char *
dump_envelope(ENVELOPE * e, unsigned char *d, int *off, int convert)
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

  d = dump_char(e->message_id, d, off, 0);
  d = dump_char(e->supersedes, d, off, 0);
  d = dump_char(e->date, d, off, 0);
  d = dump_char(e->x_label, d, off, convert);

  d = dump_buffer(e->spam, d, off, convert);

  d = dump_list(e->references, d, off, 0);
  d = dump_list(e->in_reply_to, d, off, 0);
  d = dump_list(e->userhdrs, d, off, convert);

  return d;
}

static void
restore_envelope(ENVELOPE * e, const unsigned char *d, int *off, int convert)
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

  if (0 <= real_subj_off)
    e->real_subj = e->subject + real_subj_off;
  else
    e->real_subj = NULL;

  restore_char(&e->message_id, d, off, 0);
  restore_char(&e->supersedes, d, off, 0);
  restore_char(&e->date, d, off, 0);
  restore_char(&e->x_label, d, off, convert);

  restore_buffer(&e->spam, d, off, convert);

  restore_list(&e->references, d, off, 0);
  restore_list(&e->in_reply_to, d, off, 0);
  restore_list(&e->userhdrs, d, off, convert);
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
  unsigned char md5sum[16];
  char* s;
  int ret, plen;
#ifndef HAVE_ICONV
  const char *chs = Charset && *Charset ? Charset : 
		    mutt_get_default_charset ();
#endif

  plen = mutt_strlen (path);

  ret = stat(path, &sb);
  if (ret < 0 && path[plen-1] != '/')
  {
#ifdef HAVE_ICONV
    return path;
#else
    snprintf (hcpath, _POSIX_PATH_MAX, "%s-%s", path, chs);
    return hcpath;
#endif
  }

  if (ret >= 0 && !S_ISDIR(sb.st_mode))
  {
#ifdef HAVE_ICONV
    return path;
#else
    snprintf (hcpath, _POSIX_PATH_MAX, "%s-%s", path, chs);
    return hcpath;
#endif
  }

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
    md5_buffer (folder, strlen (folder), &md5sum);

    /* On some systems (e.g. OS X), snprintf is defined as a macro.
     * Embedding directives inside macros is undefined, so we have to duplicate
     * the whole call:
     */
#ifndef HAVE_ICONV
    ret = snprintf(hcpath, _POSIX_PATH_MAX,
                   "%s/%02x%02x%02x%02x%02x%02x%02x%02x"
                   "%02x%02x%02x%02x%02x%02x%02x%02x"
		   "-%s"
		   ,
		   path, md5sum[0], md5sum[1], md5sum[2], md5sum[3],
                   md5sum[4], md5sum[5], md5sum[6], md5sum[7], md5sum[8],
                   md5sum[9], md5sum[10], md5sum[11], md5sum[12],
                   md5sum[13], md5sum[14], md5sum[15]
		   ,chs
		   );
#else
    ret = snprintf(hcpath, _POSIX_PATH_MAX,
                   "%s/%02x%02x%02x%02x%02x%02x%02x%02x"
                   "%02x%02x%02x%02x%02x%02x%02x%02x"
		   ,
		   path, md5sum[0], md5sum[1], md5sum[2], md5sum[3],
                   md5sum[4], md5sum[5], md5sum[6], md5sum[7], md5sum[8],
                   md5sum[9], md5sum[10], md5sum[11], md5sum[12],
                   md5sum[13], md5sum[14], md5sum[15]
		   );
#endif
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
 * db_store.
 */
static void *
mutt_hcache_dump(header_cache_t *h, HEADER * header, int *off,
		 unsigned int uidvalidity, mutt_hcache_store_flags_t flags)
{
  unsigned char *d = NULL;
  HEADER nh;
  int convert = !Charset_is_utf8;

  *off = 0;
  d = lazy_malloc(sizeof (validate));

  if (flags & M_GENERATE_UIDVALIDITY)
  {
    struct timeval now;
    gettimeofday(&now, NULL);
    memcpy(d, &now, sizeof (struct timeval));
  }
  else
    memcpy(d, &uidvalidity, sizeof (uidvalidity));
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
  nh.pair = 0;
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

  d = dump_envelope(nh.env, d, off, convert);
  d = dump_body(nh.content, d, off, convert);
  d = dump_char(nh.maildir_flags, d, off, convert);

  return d;
}

HEADER *
mutt_hcache_restore(const unsigned char *d, HEADER ** oh)
{
  int off = 0;
  HEADER *h = mutt_new_header();
  int convert = !Charset_is_utf8;

  /* skip validate */
  off += sizeof (validate);

  /* skip crc */
  off += sizeof (unsigned int);

  memcpy(h, d + off, sizeof (HEADER));
  off += sizeof (HEADER);

  h->env = mutt_new_envelope();
  restore_envelope(h->env, d, &off, convert);

  h->content = mutt_new_body();
  restore_body(h->content, d, &off, convert);

  restore_char(&h->maildir_flags, d, &off, convert);

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
#elif HAVE_TC
  void *data;
  int sp;
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
#elif HAVE_TC
  data = tcbdbget(h->db, path, ksize, &sp);

  return data;
#elif HAVE_GDBM
  key.dptr = path;
  key.dsize = ksize;
  
  data = gdbm_fetch(h->db, key);
  
  return data.dptr;
#endif
}

/*
 * flags
 *
 * M_GENERATE_UIDVALIDITY
 * ignore uidvalidity param and store gettimeofday() as the value
 */
int
mutt_hcache_store(header_cache_t *h, const char *filename, HEADER * header,
		  unsigned int uidvalidity,
		  size_t(*keylen) (const char *fn),
		  mutt_hcache_store_flags_t flags)
{
  char* data;
  int dlen;
  int ret;
  
  if (!h)
    return -1;
  
  data = mutt_hcache_dump(h, header, &dlen, uidvalidity, flags);
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
#elif HAVE_TC
  return tcbdbput(h->db, path, ksize, data, dlen);
#elif HAVE_GDBM
  key.dptr = path;
  key.dsize = ksize;
  
  databuf.dsize = dlen;
  databuf.dptr = data;
  
  return gdbm_store(h->db, key, databuf, GDBM_REPLACE);
#endif
}

static char* get_foldername(const char *folder)
{
  char *p = NULL;
  char path[_POSIX_PATH_MAX];
  struct stat st;

  mutt_encode_path (path, sizeof (path), folder);

  /* if the folder is local, canonify the path to avoid
   * to ensure equivalent paths share the hcache */
  if (stat (path, &st) == 0)
  {
    p = safe_malloc (PATH_MAX+1);
    if (!realpath (path, p))
      mutt_str_replace (&p, path);
  } else
    p = safe_strdup (path);

  return p;
}

#if HAVE_QDBM
static int
hcache_open_qdbm (struct header_cache* h, const char* path)
{
  int    flags = VL_OWRITER | VL_OCREAT;

  if (option(OPTHCACHECOMPRESS))
    flags |= VL_OZCOMP;

  h->db = vlopen (path, flags, VL_CMPLEX);
  if (h->db)
    return 0;
  else
    return -1;
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

#elif HAVE_TC
static int
hcache_open_tc (struct header_cache* h, const char* path)
{
  h->db = tcbdbnew();
  if (!h->db)
      return -1;
  if (option(OPTHCACHECOMPRESS))
    tcbdbtune(h->db, 0, 0, 0, -1, -1, BDBTDEFLATE);
  if (tcbdbopen(h->db, path, BDBOWRITER | BDBOCREAT))
    return 0;
  else
  {
    dprint(2, (debugfile, "tcbdbopen %s failed: %s\n", path, errno));
    tcbdbdel(h->db);
    return -1;
  }
}

void
mutt_hcache_close(header_cache_t *h)
{
  if (!h)
    return;

  if (!tcbdbclose(h->db))
      dprint (2, (debugfile, "tcbdbclose failed for %s: %d\n", h->folder, errno));
  tcbdbdel(h->db);
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

  return tcbdbout(h->db, path, ksize);
}

#elif HAVE_GDBM
static int
hcache_open_gdbm (struct header_cache* h, const char* path)
{
  int pagesize;

  if (mutt_atoi (HeaderCachePageSize, &pagesize) < 0 || pagesize <= 0)
    pagesize = 16384;

  h->db = gdbm_open((char *) path, pagesize, GDBM_WRCREAT, 00600, NULL);
  if (h->db)
    return 0;

  /* if rw failed try ro */
  h->db = gdbm_open((char *) path, pagesize, GDBM_READER, 00600, NULL);
  if (h->db)
    return 0;

  return -1;
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

static int
hcache_open_db4 (struct header_cache* h, const char* path)
{
  struct stat sb;
  int ret;
  u_int32_t createflags = DB_CREATE;
  int pagesize;

  if (mutt_atoi (HeaderCachePageSize, &pagesize) < 0 || pagesize <= 0)
    pagesize = 16384;

  snprintf (h->lockfile, _POSIX_PATH_MAX, "%s-lock-hack", path);

  h->fd = open (h->lockfile, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
  if (h->fd < 0)
    return -1;

  if (mx_lock_file (h->lockfile, h->fd, 1, 0, 5))
    goto fail_close;

  ret = db_env_create (&h->env, 0);
  if (ret)
    goto fail_unlock;

  ret = (*h->env->open)(h->env, NULL, DB_INIT_MPOOL | DB_CREATE | DB_PRIVATE,
	0600);
  if (ret)
    goto fail_env;

  ret = db_create (&h->db, h->env, 0);
  if (ret)
    goto fail_env;

  if (stat(path, &sb) != 0 && errno == ENOENT)
  {
    createflags |= DB_EXCL;
    h->db->set_pagesize(h->db, pagesize);
  }

  ret = (*h->db->open)(h->db, NULL, path, h->folder, DB_BTREE, createflags,
                       0600);
  if (ret)
    goto fail_db;

  return 0;

  fail_db:
  h->db->close (h->db, 0);
  fail_env:
  h->env->close (h->env, 0);
  fail_unlock:
  mx_unlock_file (h->lockfile, h->fd, 0);
  fail_close:
  close (h->fd);
  unlink (h->lockfile);

  return -1;
}

void
mutt_hcache_close(header_cache_t *h)
{
  if (!h)
    return;

  h->db->close (h->db, 0);
  h->env->close (h->env, 0);
  mx_unlock_file (h->lockfile, h->fd, 0);
  close (h->fd);
  unlink (h->lockfile);
  FREE (&h->folder);
  FREE (&h);
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

header_cache_t *
mutt_hcache_open(const char *path, const char *folder, hcache_namer_t namer)
{
  struct header_cache *h = safe_calloc(1, sizeof (struct header_cache));
  int (*hcache_open) (struct header_cache* h, const char* path);
  struct stat sb;

#if HAVE_QDBM
  hcache_open = hcache_open_qdbm;
#elif HAVE_TC
  hcache_open= hcache_open_tc;
#elif HAVE_GDBM
  hcache_open = hcache_open_gdbm;
#elif HAVE_DB4
  hcache_open = hcache_open_db4;
#endif

  /* Calculate the current hcache version from dynamic configuration */
  if (hcachever == 0x0) {
    union {
      unsigned char charval[16];
      unsigned int intval;
    } digest;
    struct md5_ctx ctx;
    SPAM_LIST *spam;
    RX_LIST *nospam;

    hcachever = HCACHEVER;

    md5_init_ctx(&ctx);

    /* Seed with the compiled-in header structure hash */
    md5_process_bytes(&hcachever, sizeof(hcachever), &ctx);

    /* Mix in user's spam list */
    for (spam = SpamList; spam; spam = spam->next)
    {
      md5_process_bytes(spam->rx->pattern, strlen(spam->rx->pattern), &ctx);
      md5_process_bytes(spam->template, strlen(spam->template), &ctx);
    }

    /* Mix in user's nospam list */
    for (nospam = NoSpamList; nospam; nospam = nospam->next)
    {
      md5_process_bytes(nospam->rx->pattern, strlen(nospam->rx->pattern), &ctx);
    }

    /* Get a hash and take its bytes as an (unsigned int) hash version */
    md5_finish_ctx(&ctx, digest.charval);
    hcachever = digest.intval;
  }

  h->db = NULL;
  h->folder = get_foldername(folder);
  h->crc = hcachever;

  if (!path || path[0] == '\0')
  {
    FREE(&h->folder);
    FREE(&h);
    return NULL;
  }

  path = mutt_hcache_per_folder(path, h->folder, namer);

  if (!hcache_open (h, path))
    return h;
  else
  {
    /* remove a possibly incompatible version */
    if (!stat (path, &sb) && !unlink (path))
    {
      if (!hcache_open (h, path))
        return h;
    }
    FREE(&h->folder);
    FREE(&h);

    return NULL;
  }
}

#if HAVE_DB4
const char *mutt_hcache_backend (void)
{
  return DB_VERSION_STRING;
}
#elif HAVE_GDBM
const char *mutt_hcache_backend (void)
{
  return gdbm_version;
}
#elif HAVE_QDBM
const char *mutt_hcache_backend (void)
{
  return "qdbm " _QDBM_VERSION;
}
#elif HAVE_TC
const char *mutt_hcache_backend (void)
{
  return "tokyocabinet " _TC_VERSION;
}
#endif
