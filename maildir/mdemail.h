/**
 * @file
 * Maildir Email helper
 *
 * @authors
 * Copyright (C) 2020-2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_MAILDIR_MDEMAIL_H
#define MUTT_MAILDIR_MDEMAIL_H

#include <stdbool.h>
#include <sys/types.h>
#include "mutt/lib.h"

/**
 * struct MdEmail - A Maildir Email helper
 */
struct MdEmail
{
  struct Email *email;           ///< Temporary Email
  char *        canon_fname;     ///< Canonical filename for hashing
  bool          header_parsed;   ///< Has the Email header been parsed?
  ino_t         inode;           ///< Inode number of the file
};
ARRAY_HEAD(MdEmailArray, struct MdEmail *);

void            maildir_entry_free(struct MdEmail **ptr);
struct MdEmail *maildir_entry_new(void);
void            maildirarray_clear(struct MdEmailArray *mda);

#endif /* MUTT_MAILDIR_MDEMAIL_H */
