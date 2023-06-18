/**
 * @file
 * Imap-specific Mailbox data
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

#ifndef MUTT_IMAP_MDATA_H
#define MUTT_IMAP_MDATA_H

#include <stdint.h>
#include <time.h>
#include "private.h"
#include "mutt/lib.h"

struct Mailbox;
struct ImapAccountData;

/**
 * struct ImapMboxData - IMAP-specific Mailbox data - @extends Mailbox
 *
 * This data is specific to a Mailbox of an IMAP server
 */
struct ImapMboxData
{
  char *name;        ///< Mailbox name
  char *munge_name;  ///< Munged version of the mailbox name
  char *real_name;   ///< Original Mailbox name, e.g.: INBOX can be just \0

  ImapOpenFlags reopen;        ///< Flags, e.g. #IMAP_REOPEN_ALLOW
  ImapOpenFlags check_status;  ///< Flags, e.g. #IMAP_NEWMAIL_PENDING
  unsigned int new_mail_count; ///< Set when EXISTS notifies of new mail

  // IMAP STATUS information
  struct ListHead flags;
  uint32_t uidvalidity;
  unsigned int uid_next;
  unsigned long long modseq;
  unsigned int messages;
  unsigned int recent;
  unsigned int unseen;

  // Cached data used only when the mailbox is opened
  struct HashTable *uid_hash;               ///< Hash Table: "uid" -> Email
  ARRAY_HEAD(MSNArray, struct Email *) msn; ///< look up headers by (MSN-1)
  struct BodyCache *bcache;                 ///< Email body cache

  struct HeaderCache *hcache; ///< Email header cache
  struct timespec mtime;      ///< Time Mailbox was last changed
};

void                 imap_mdata_free(void **ptr);
struct ImapMboxData *imap_mdata_get (struct Mailbox *m);
struct ImapMboxData *imap_mdata_new (struct ImapAccountData *adata, const char* name);

#endif /* MUTT_IMAP_MDATA_H */
