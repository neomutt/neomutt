/**
 * Copyright (C) 1998 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999 Andrej Gritsenko <andrej@lucky.net>
 * Copyright (C) 2000-2012 Vsevolod Volkov <vvv@mutt.org.ua>
 *
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

#ifndef _MUTT_NNTP_H
#define _MUTT_NNTP_H 1

#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include "bcache.h"
#include "mailbox.h"
#include "mutt_socket.h"
#ifdef USE_HCACHE
#include "hcache/hcache.h"
#endif

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
  bool hasCAPABILITIES : 1;
  bool hasSTARTTLS : 1;
  bool hasDATE : 1;
  bool hasLIST_NEWSGROUPS : 1;
  bool hasXGTITLE : 1;
  bool hasLISTGROUP : 1;
  bool hasLISTGROUPrange : 1;
  bool hasOVER : 1;
  bool hasXOVER : 1;
  unsigned int use_tls : 3;
  unsigned int status : 3;
  bool cacheable : 1;
  bool newsrc_modified : 1;
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
  struct Hash *groups_hash;
  struct Connection *conn;
} NNTP_SERVER;

struct NewsrcEntry
{
  anum_t first;
  anum_t last;
};

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
  bool subscribed : 1;
  bool new : 1;
  bool allowed : 1;
  bool deleted : 1;
  unsigned int newsrc_len;
  struct NewsrcEntry *newsrc_ent;
  NNTP_SERVER *nserv;
  NNTP_ACACHE acache[NNTP_ACACHE_LEN];
  struct BodyCache *bcache;
} NNTP_DATA;

typedef struct
{
  anum_t article_num;
  bool parsed : 1;
} NNTP_HEADER_DATA;

#define NHDR(hdr) ((NNTP_HEADER_DATA *) ((hdr)->data))

/* internal functions */
int nntp_add_group(char *line, void *data);
int nntp_active_save_cache(NNTP_SERVER *nserv);
int nntp_check_new_groups(NNTP_SERVER *nserv);
int nntp_open_connection(NNTP_SERVER *nserv);
void nntp_newsrc_gen_entries(struct Context *ctx);
void nntp_bcache_update(NNTP_DATA *nntp_data);
void nntp_article_status(struct Context *ctx, struct Header *hdr, char *group, anum_t anum);
void nntp_group_unread_stat(NNTP_DATA *nntp_data);
void nntp_data_free(void *data);
void nntp_acache_free(NNTP_DATA *nntp_data);
void nntp_delete_group_cache(NNTP_DATA *nntp_data);

/* exposed interface */
NNTP_SERVER *nntp_select_server(char *server, int leave_lock);
NNTP_DATA *mutt_newsgroup_subscribe(NNTP_SERVER *nserv, char *group);
NNTP_DATA *mutt_newsgroup_unsubscribe(NNTP_SERVER *nserv, char *group);
NNTP_DATA *mutt_newsgroup_catchup(NNTP_SERVER *nserv, char *group);
NNTP_DATA *mutt_newsgroup_uncatchup(NNTP_SERVER *nserv, char *group);
int nntp_active_fetch(NNTP_SERVER *nserv);
int nntp_newsrc_update(NNTP_SERVER *nserv);
int nntp_post(const char *msg);
int nntp_check_msgid(struct Context *ctx, const char *msgid);
int nntp_check_children(struct Context *ctx, const char *msgid);
int nntp_newsrc_parse(NNTP_SERVER *nserv);
void nntp_newsrc_close(NNTP_SERVER *nserv);
void nntp_buffy(char *buf, size_t len);
void nntp_expand_path(char *line, size_t len, struct Account *acct);
void nntp_clear_cache(NNTP_SERVER *nserv);
const char *nntp_format_str(char *dest, size_t destlen, size_t col, int cols,
                            char op, const char *src, const char *fmt,
                            const char *ifstring, const char *elsestring,
                            unsigned long data, format_flag flags);

NNTP_SERVER *CurrentNewsSrv INITVAL(NULL);

#ifdef USE_HCACHE
header_cache_t *nntp_hcache_open(NNTP_DATA *nntp_data);
void nntp_hcache_update(NNTP_DATA *nntp_data, header_cache_t *hc);
#endif

extern struct mx_ops mx_nntp_ops;

#endif /* _MUTT_NNTP_H */
