/**
 * @file
 * Usenet network mailbox type; talk to an NNTP server
 *
 * @authors
 * Copyright (C) 1998 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999 Andrej Gritsenko <andrej@lucky.net>
 * Copyright (C) 2000-2012 Vsevolod Volkov <vvv@mutt.org.ua>
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

#ifndef _MUTT_NNTP_H
#define _MUTT_NNTP_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include "format_flags.h"
#include "mx.h"
#ifdef USE_HCACHE
#include "hcache/hcache.h"
#endif

struct Account;
struct Header;
struct Context;

#define NNTP_PORT 119
#define NNTP_SSL_PORT 563

/* number of entries in article cache */
#define NNTP_ACACHE_LEN 10

/* article number type and format */
#define anum_t uint32_t
#define ANUM "%u"

/**
 * enum NntpStatus - NNTP server return values
 */
enum NntpStatus
{
  NNTP_NONE = 0,
  NNTP_OK,
  NNTP_BYE
};

/**
 * struct NntpServer - NNTP-specific server data
 */
struct NntpServer
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
};

/**
 * struct NewsrcEntry - An entry in a .newsrc (subscribed newsgroups)
 */
struct NewsrcEntry
{
  anum_t first;
  anum_t last;
};

/**
 * struct NntpAcache - NNTP article cache
 */
struct NntpAcache
{
  unsigned int index;
  char *path;
};

/**
 * struct NntpData - NNTP-specific server data
 */
struct NntpData
{
  char *group;
  char *desc;
  anum_t first_message;
  anum_t last_message;
  anum_t last_loaded;
  anum_t last_cached;
  anum_t unread;
  bool subscribed : 1;
  bool new : 1;
  bool allowed : 1;
  bool deleted : 1;
  unsigned int newsrc_len;
  struct NewsrcEntry *newsrc_ent;
  struct NntpServer *nserv;
  struct NntpAcache acache[NNTP_ACACHE_LEN];
  struct BodyCache *bcache;
};

/**
 * struct NntpHeaderData - NNTP-specific header data
 */
struct NntpHeaderData
{
  anum_t article_num;
  bool parsed : 1;
};

#define NHDR(hdr) ((struct NntpHeaderData *) ((hdr)->data))

/* internal functions */
int nntp_add_group(char *line, void *data);
int nntp_active_save_cache(struct NntpServer *nserv);
int nntp_check_new_groups(struct NntpServer *nserv);
int nntp_open_connection(struct NntpServer *nserv);
void nntp_newsrc_gen_entries(struct Context *ctx);
void nntp_bcache_update(struct NntpData *nntp_data);
void nntp_article_status(struct Context *ctx, struct Header *hdr, char *group, anum_t anum);
void nntp_group_unread_stat(struct NntpData *nntp_data);
void nntp_data_free(void *data);
void nntp_acache_free(struct NntpData *nntp_data);
void nntp_delete_group_cache(struct NntpData *nntp_data);

/* exposed interface */
struct NntpServer *nntp_select_server(char *server, bool leave_lock);
struct NntpData *mutt_newsgroup_subscribe(struct NntpServer *nserv, char *group);
struct NntpData *mutt_newsgroup_unsubscribe(struct NntpServer *nserv, char *group);
struct NntpData *mutt_newsgroup_catchup(struct NntpServer *nserv, char *group);
struct NntpData *mutt_newsgroup_uncatchup(struct NntpServer *nserv, char *group);
int nntp_active_fetch(struct NntpServer *nserv, bool new);
int nntp_newsrc_update(struct NntpServer *nserv);
int nntp_post(const char *msg);
int nntp_check_msgid(struct Context *ctx, const char *msgid);
int nntp_check_children(struct Context *ctx, const char *msgid);
int nntp_newsrc_parse(struct NntpServer *nserv);
void nntp_newsrc_close(struct NntpServer *nserv);
void nntp_buffy(char *buf, size_t len);
void nntp_expand_path(char *line, size_t len, struct Account *acct);
void nntp_clear_cache(struct NntpServer *nserv);
const char *nntp_format_str(char *buf, size_t buflen, size_t col, int cols, char op,
                            const char *src, const char *prec, const char *if_str,
                            const char *else_str, unsigned long data, enum FormatFlag flags);

struct NntpServer *CurrentNewsSrv;

#ifdef USE_HCACHE
header_cache_t *nntp_hcache_open(struct NntpData *nntp_data);
void nntp_hcache_update(struct NntpData *nntp_data, header_cache_t *hc);
#endif

extern struct MxOps mx_nntp_ops;

#endif /* _MUTT_NNTP_H */
