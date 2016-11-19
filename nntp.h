/*
 * Copyright (C) 1998 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999 Andrej Gritsenko <andrej@lucky.net>
 * Copyright (C) 2000-2012 Vsevolod Volkov <vvv@mutt.org.ua>
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
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _NNTP_H_
#define _NNTP_H_ 1

#include "mutt_socket.h"
#include "mailbox.h"
#include "bcache.h"

#if USE_HCACHE
#include "hcache.h"
#endif

#include <time.h>
#include <sys/types.h>
#include <stdint.h>

#define NNTP_PORT 119
#define NNTP_SSL_PORT 563

/* number of entries in article cache */
#define NNTP_ACACHE_LEN 10

/* article number type and format */
#define anum_t uint32_t
#define ANUM "%u"

enum
{
  NNTP_NONE = 0,
  NNTP_OK,
  NNTP_BYE
};

typedef struct
{
  unsigned int hasCAPABILITIES : 1;
  unsigned int hasSTARTTLS : 1;
  unsigned int hasDATE : 1;
  unsigned int hasLIST_NEWSGROUPS : 1;
  unsigned int hasXGTITLE : 1;
  unsigned int hasLISTGROUP : 1;
  unsigned int hasLISTGROUPrange : 1;
  unsigned int hasOVER : 1;
  unsigned int hasXOVER : 1;
  unsigned int use_tls : 3;
  unsigned int status : 3;
  unsigned int cacheable : 1;
  unsigned int newsrc_modified : 1;
  FILE *newsrc_fp;
  char *newsrc_file;
  char *authenticators;
  char *overview_fmt;
  off_t size;
  time_t mtime;
  time_t newgroups_time;
  time_t check_time;
  unsigned int groups_num;
  unsigned int groups_max;
  void **groups_list;
  HASH *groups_hash;
  CONNECTION *conn;
} NNTP_SERVER;

typedef struct
{
  anum_t first;
  anum_t last;
} NEWSRC_ENTRY;

typedef struct
{
  unsigned int index;
  char *path;
} NNTP_ACACHE;

typedef struct
{
  char *group;
  char *desc;
  anum_t firstMessage;
  anum_t lastMessage;
  anum_t lastLoaded;
  anum_t lastCached;
  anum_t unread;
  unsigned int subscribed : 1;
  unsigned int new : 1;
  unsigned int allowed : 1;
  unsigned int deleted : 1;
  unsigned int newsrc_len;
  NEWSRC_ENTRY *newsrc_ent;
  NNTP_SERVER *nserv;
  NNTP_ACACHE acache[NNTP_ACACHE_LEN];
  body_cache_t *bcache;
} NNTP_DATA;

typedef struct
{
  anum_t article_num;
  unsigned int parsed : 1;
} NNTP_HEADER_DATA;

#define NHDR(hdr) ((NNTP_HEADER_DATA*)((hdr)->data))

/* internal functions */
int nntp_add_group (char *, void *);
int nntp_active_save_cache (NNTP_SERVER *);
int nntp_check_new_groups (NNTP_SERVER *);
int nntp_fastclose_mailbox (CONTEXT *);
int nntp_open_connection (NNTP_SERVER *);
void nntp_newsrc_gen_entries (CONTEXT *);
void nntp_bcache_update (NNTP_DATA *);
void nntp_article_status (CONTEXT *, HEADER *, char *, anum_t);
void nntp_group_unread_stat (NNTP_DATA *);
void nntp_data_free (void *);
void nntp_acache_free (NNTP_DATA *);
void nntp_delete_group_cache (NNTP_DATA *);

/* exposed interface */
NNTP_SERVER *nntp_select_server (char *, int);
NNTP_DATA *mutt_newsgroup_subscribe (NNTP_SERVER *, char *);
NNTP_DATA *mutt_newsgroup_unsubscribe (NNTP_SERVER *, char *);
NNTP_DATA *mutt_newsgroup_catchup (NNTP_SERVER *, char *);
NNTP_DATA *mutt_newsgroup_uncatchup (NNTP_SERVER *, char *);
int nntp_active_fetch (NNTP_SERVER *);
int nntp_newsrc_update (NNTP_SERVER *);
int nntp_open_mailbox (CONTEXT *);
int nntp_sync_mailbox (CONTEXT *ctx, int *index_hint);
int nntp_check_mailbox (CONTEXT *, int*);
int nntp_fetch_message (CONTEXT *, MESSAGE *, int);
int nntp_post (const char *);
int nntp_check_msgid (CONTEXT *, const char *);
int nntp_check_children (CONTEXT *, const char *);
int nntp_newsrc_parse (NNTP_SERVER *);
void nntp_newsrc_close (NNTP_SERVER *);
void nntp_buffy (char *, size_t);
void nntp_expand_path (char *, size_t, ACCOUNT *);
void nntp_clear_cache (NNTP_SERVER *);
const char *nntp_format_str (char *, size_t, size_t, int, char, const char *,
			     const char *, const char *, const char *,
			     unsigned long, format_flag);

NNTP_SERVER *CurrentNewsSrv INITVAL (NULL);

#ifdef USE_HCACHE
header_cache_t *nntp_hcache_open (NNTP_DATA *);
void nntp_hcache_update (NNTP_DATA *, header_cache_t *);
#endif

extern struct mx_ops mx_nntp_ops;

#endif /* _NNTP_H_ */
