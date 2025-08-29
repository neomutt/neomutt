/**
 * @file
 * Nntp-specific Account data
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

#ifndef MUTT_NNTP_ADATA_H
#define MUTT_NNTP_ADATA_H

#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>

struct Connection;

/**
 * struct NntpAccountData - NNTP-specific Account data - @extends Account
 */
struct NntpAccountData
{
  bool hasCAPABILITIES    : 1; ///< Server supports CAPABILITIES command
  bool hasSTARTTLS        : 1; ///< Server supports STARTTLS command
  bool hasDATE            : 1; ///< Server supports DATE command
  bool hasLIST_NEWSGROUPS : 1; ///< Server supports LIST_NEWSGROUPS command
  bool hasXGTITLE         : 1; ///< Server supports XGTITLE command
  bool hasLISTGROUP       : 1; ///< Server supports LISTGROUP command
  bool hasLISTGROUPrange  : 1; ///< Server supports LISTGROUPrange command
  bool hasOVER            : 1; ///< Server supports OVER command
  bool hasXOVER           : 1; ///< Server supports XOVER command
  unsigned int use_tls    : 3;
  unsigned int status     : 3;
  bool cacheable          : 1;
  bool newsrc_modified    : 1;
  FILE *fp_newsrc;
  char *newsrc_file;
  char *authenticators;
  char *overview_fmt;
  off_t size;
  time_t mtime;
  time_t newgroups_time;
  time_t check_time;
  unsigned int groups_num;
  unsigned int groups_max;
  struct NntpMboxData **groups_list;
  struct HashTable *groups_hash; ///< Hash Table: "newsgroup" -> NntpMboxData
  struct Connection *conn;       ///< Connection to NNTP Server
};

void                    nntp_adata_free(void **ptr);
struct NntpAccountData *nntp_adata_new (struct Connection *conn);

#endif /* MUTT_NNTP_ADATA_H */
