/**
 * @file
 * Notmuch-specific Email data
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

#ifndef MUTT_NOTMUCH_EDATA_H
#define MUTT_NOTMUCH_EDATA_H

#include "core/lib.h"

struct Email;

/**
 * struct NmEmailData - Notmuch-specific Email data - @extends Email
 */
struct NmEmailData
{
  char *folder;           ///< Location of the Email
  char *oldpath;
  char *virtual_id;       ///< Unique Notmuch Id
  enum MailboxType type;  ///< Type of Mailbox the Email is in
};

void                  nm_edata_free(void **ptr);
struct NmEmailData *  nm_edata_get (struct Email *e);
struct NmEmailData *  nm_edata_new (void);

#endif /* MUTT_NOTMUCH_EDATA_H */
