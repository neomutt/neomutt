/**
 * @file
 * Imap-specific Email data
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

#ifndef MUTT_IMAP_EDATA_H
#define MUTT_IMAP_EDATA_H

#include <stdbool.h>

struct Email;

/**
 * struct ImapEmailData - IMAP-specific Email data - @extends Email
 */
struct ImapEmailData
{
  /* server-side flags */
  bool read    : 1; ///< Email has been read
  bool old     : 1; ///< Email has been seen
  bool deleted : 1; ///< Email has been deleted
  bool flagged : 1; ///< Email has been flagged
  bool replied : 1; ///< Email has been replied to

  bool parsed : 1;

  unsigned int uid; ///< 32-bit Message UID
  unsigned int msn; ///< Message Sequence Number

  char *flags_system;
  char *flags_remote;
};

void                  imap_edata_free(void **ptr);
struct ImapEmailData *imap_edata_get (struct Email *e);
struct ImapEmailData *imap_edata_new (void);

#endif /* MUTT_IMAP_EDATA_H */
