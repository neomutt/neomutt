/**
 * @file
 * Mh Email helper
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_MH_MDEMAIL_H
#define MUTT_MH_MDEMAIL_H

#include <stdbool.h>
#include <sys/types.h>
#include "mutt/lib.h"

/**
 * struct MhEmail - A Mh Email helper
 *
 * Used during scanning
 */
struct MhEmail
{
  struct Email *email;           ///< Temporary Email
  char *        canon_fname;     ///< Canonical filename for hashing
  bool          header_parsed;   ///< Has the Email header been parsed?
  ino_t         inode;           ///< Inode number of the file
};
ARRAY_HEAD(MhEmailArray, struct MhEmail *);

void            mharray_clear(struct MhEmailArray *mha);
void            mh_entry_free(struct MhEmail **ptr);
struct MhEmail *mh_entry_new (void);

#endif /* MUTT_MH_MDEMAIL_H */
