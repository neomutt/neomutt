/**
 * @file
 * Usenet network mailbox type; talk to an NNTP server
 *
 * @authors
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
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
 * @page lib_nntp Nntp
 *
 * Usenet network mailbox type; talk to an NNTP server
 *
 * | File                      | Description                   |
 * | :------------------------ | :---------------------------- |
 * | nntp/adata.c              | @subpage nntp_adata           |
 * | nntp/complete.c           | @subpage nntp_complete        |
 * | nntp/config.c             | @subpage nntp_config          |
 * | nntp/edata.c              | @subpage nntp_edata           |
 * | nntp/expando_browser.c    | @subpage nntp_expando_browser |
 * | nntp/expando_newsrc.c     | @subpage nntp_expando_newsrc  |
 * | nntp/mdata.c              | @subpage nntp_mdata           |
 * | nntp/newsrc.c             | @subpage nntp_newsrc          |
 * | nntp/nntp.c               | @subpage nntp_nntp            |
 */

#ifndef MUTT_NNTP_LIB_H
#define MUTT_NNTP_LIB_H

#include <stdbool.h>
#include <stdio.h>
#include "core/lib.h"
#include "expando/lib.h"
#include "expando_browser.h" // IWYU pragma: keep

struct Buffer;
struct ConnAccount;
struct Email;
struct NntpAccountData;
struct stat;

extern struct NntpAccountData *CurrentNewsSrv; ///< Current NNTP news server
extern const struct MxOps MxNntpOps;
extern const struct ExpandoRenderCallback NntpRenderCallbacks[];

/* article number type and format */
#define anum_t long
#define ANUM_FMT "%ld"

/**
 * struct NntpAcache - NNTP article cache
 */
struct NntpAcache
{
  unsigned int index; ///< Index number
  char *path;         ///< Cache path
};

/**
 * struct NewsrcEntry - An entry in a .newsrc (subscribed newsgroups)
 */
struct NewsrcEntry
{
  anum_t first; ///< First article number in run
  anum_t last;  ///< Last article number in run
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
int nntp_sort_unsorted(const struct Email *a, const struct Email *b, bool reverse);
enum MailboxType nntp_path_probe(const char *path, const struct stat *st);
int nntp_complete(struct Buffer *buf);

#endif /* MUTT_NNTP_LIB_H */
