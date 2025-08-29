/**
 * @file
 * Usenet network mailbox type; talk to an NNTP server
 *
 * @authors
 * Copyright (C) 2018-2021 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_NNTP_PRIVATE_H
#define MUTT_NNTP_PRIVATE_H

#include <stdbool.h>
#include <stdint.h>
#include "lib.h"

struct Email;
struct HeaderCache;
struct Mailbox;
struct NntpAccountData;
struct NntpMboxData;

#define NNTP_PORT 119
#define NNTP_SSL_PORT 563

/**
 * enum NntpStatus - NNTP server return values
 */
enum NntpStatus
{
  NNTP_NONE = 0, ///< No connection to server
  NNTP_OK,       ///< Connected to server
  NNTP_BYE,      ///< Disconnected from server
};

void                    nntp_acache_free       (struct NntpMboxData *mdata);
int                     nntp_active_save_cache (struct NntpAccountData *adata);
int                     nntp_add_group         (char *line, void *data);
void                    nntp_article_status    (struct Mailbox *m, struct Email *e, char *group, anum_t anum);
void                    nntp_bcache_update     (struct NntpMboxData *mdata);
int                     nntp_check_new_groups  (struct Mailbox *m, struct NntpAccountData *adata);
void                    nntp_delete_group_cache(struct NntpMboxData *mdata);
void                    nntp_group_unread_stat (struct NntpMboxData *mdata);
void                    nntp_hash_destructor_t (int type, void *obj, intptr_t data);
void                    nntp_hashelem_free     (int type, void *obj, intptr_t data);
struct HeaderCache *    nntp_hcache_open       (struct NntpMboxData *mdata);
void                    nntp_hcache_update     (struct NntpMboxData *mdata, struct HeaderCache *hc);
void                    nntp_newsrc_gen_entries(struct Mailbox *m);
int                     nntp_open_connection   (struct NntpAccountData *adata);

#endif /* MUTT_NNTP_PRIVATE_H */
