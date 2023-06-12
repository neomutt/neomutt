/**
 * @file
 * Nntp-specific Mailbox data
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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
#include "lib.h"

/**
 * struct NntpMboxData - NNTP-specific Mailbox data - @extends Mailbox
 */
struct NntpMboxData
{
  char *group; ///< Name of newsgroup
  char *desc;  ///< Description of newsgroup
  anum_t first_message;
  anum_t last_message;
  anum_t last_loaded;
  anum_t last_cached;
  anum_t unread;
  bool subscribed   : 1;
  bool has_new_mail : 1;
  bool allowed      : 1;
  bool deleted      : 1;
  unsigned int newsrc_len;
  struct NewsrcEntry *newsrc_ent;
  struct NntpAccountData *adata;
  struct NntpAcache acache[NNTP_ACACHE_LEN];
  struct BodyCache *bcache;
  struct timespec mtime; ///< Time Mailbox was last changed
};

void nntp_mdata_free(void **ptr);

#endif /* MUTT_NNTP_MDATA_H */
