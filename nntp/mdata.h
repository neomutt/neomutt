/**
 * @file
 * Nntp-specific Mailbox data
 *
 * @authors
 * Copyright (C) 2021-2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_NNTP_MDATA_H
#define MUTT_NNTP_MDATA_H

#include <stdbool.h>
#include <time.h>
#include "lib.h"

/**
 * struct NntpMboxData - NNTP-specific Mailbox data - @extends Mailbox
 */
struct NntpMboxData
{
  char *group;                                 ///< Name of newsgroup
  char *desc;                                  ///< Description of newsgroup
  anum_t first_message;                        ///< First article number
  anum_t last_message;                         ///< Last article number
  anum_t last_loaded;                          ///< Last loaded article
  anum_t last_cached;                          ///< Last cached article
  anum_t unread;                               ///< Unread articles
  bool subscribed   : 1;                       ///< Subscribed to this newsgroup
  bool has_new_mail : 1;                       ///< Has new articles
  bool allowed      : 1;                       ///< Posting allowed
  bool deleted      : 1;                       ///< Newsgroup is deleted
  unsigned int newsrc_len;                     ///< Length of newsrc entry
  struct NewsrcEntry *newsrc_ent;              ///< Newsrc entries
  struct NntpAccountData *adata;               ///< Account data
  struct NntpAcache acache[NNTP_ACACHE_LEN];   ///< Article cache
  struct BodyCache *bcache;                    ///< Body cache

  struct timespec mtime;                       ///< Time Mailbox was last changed
};

void nntp_mdata_free(void **ptr);

#endif /* MUTT_NNTP_MDATA_H */
