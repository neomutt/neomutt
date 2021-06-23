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

/**
 * @page lib_nntp NNTP: Usenet network mailbox type; talk to an NNTP server
 *
 * Usenet network mailbox type; talk to an NNTP server
 *
 * | File            | Description            |
 * | :-------------- | :--------------------- |
 * | nntp/adata.c    | @subpage nntp_adata    |
 * | nntp/browse.c   | @subpage nntp_browse   |
 * | nntp/complete.c | @subpage nntp_complete |
 * | nntp/config.c   | @subpage nntp_config   |
 * | nntp/edata.c    | @subpage nntp_edata    |
 * | nntp/mdata.c    | @subpage nntp_mdata    |
 * | nntp/newsrc.c   | @subpage nntp_newsrc   |
 * | nntp/nntp.c     | @subpage nntp_nntp     |
 */

#ifndef MUTT_NNTP_LIB_H
#define MUTT_NNTP_LIB_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "core/lib.h"
#include "format_flags.h"

struct ConnAccount;
struct NntpAccountData;
struct stat;

extern struct NntpAccountData *CurrentNewsSrv; ///< Current NNTP news server
extern struct MxOps MxNntpOps;

/* article number type and format */
#define anum_t uint32_t
#define ANUM "%u"

/**
 * struct NntpAcache - NNTP article cache
 */
struct NntpAcache
{
  unsigned int index;
  char *path;
};

/**
 * struct NewsrcEntry - An entry in a .newsrc (subscribed newsgroups)
 */
struct NewsrcEntry
{
  anum_t first;
  anum_t last;
};

/* number of entries in article cache */
#define NNTP_ACACHE_LEN 10

struct NntpAccountData *nntp_select_server(struct Mailbox *m, const char *server, bool leave_lock);
struct NntpMboxData *mutt_newsgroup_subscribe(struct NntpAccountData *adata, char *group);
struct NntpMboxData *mutt_newsgroup_unsubscribe(struct NntpAccountData *adata, char *group);
struct NntpMboxData *mutt_newsgroup_catchup(struct Mailbox *m, struct NntpAccountData *adata, char *group);
struct NntpMboxData *mutt_newsgroup_uncatchup(struct Mailbox *m, struct NntpAccountData *adata, char *group);
int nntp_active_fetch(struct NntpAccountData *adata, bool mark_new);
int nntp_newsrc_update(struct NntpAccountData *adata);
int nntp_post(struct Mailbox *m, const char *msg);
int nntp_check_msgid(struct Mailbox *m, const char *msgid);
int nntp_check_children(struct Mailbox *m, const char *msgid);
int nntp_newsrc_parse(struct NntpAccountData *adata);
void nntp_newsrc_close(struct NntpAccountData *adata);
void nntp_mailbox(struct Mailbox *m, char *buf, size_t buflen);
void nntp_expand_path(char *buf, size_t buflen, struct ConnAccount *acct);
void nntp_clear_cache(struct NntpAccountData *adata);
const char *nntp_format_str(char *buf, size_t buflen, size_t col, int cols, char op, const char *src, const char *prec, const char *if_str, const char *else_str, intptr_t data, MuttFormatFlags flags);
int nntp_compare_order(const struct Email *a, const struct Email *b, bool reverse);
enum MailboxType nntp_path_probe(const char *path, const struct stat *st);
const char *group_index_format_str(char *buf, size_t buflen, size_t col, int cols, char op, const char *src, const char *prec, const char *if_str, const char *else_str, intptr_t data, MuttFormatFlags flags);
int nntp_complete(char *buf, size_t buflen);

#endif /* MUTT_NNTP_LIB_H */
