/**
 * @file
 * Mailbox path
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_CORE_PATH_H
#define MUTT_CORE_PATH_H

#include <stdint.h>

typedef uint8_t PathFlags;       ///< Flags for Path, e.g. #MPATH_RESOLVED
#define MPATH_NO_FLAGS        0  ///< No flags are set
#define MPATH_RESOLVED  (1 << 0) ///< Path has been resolved, see mx_path_resolve()
#define MPATH_TIDY      (1 << 1) ///< Path has been tidied, see MxOps::path_tidy()
#define MPATH_CANONICAL (1 << 2) ///< Path is canonical, see MxOps::path_canon()
#define MPATH_ROOT      (1 << 3) ///< Path is at the root of an Account (it has no parent)

/**
 * Path - A path to a Mailbox, file or directory
 */
struct Path
{
  char *orig;      ///< User-entered path
  char *canon;     ///< Canonical path
  char *desc;      ///< Descriptive name
  char *pretty;    ///< Abbreviated version (for display)
  int type;        ///< Path type, enum MailboxType
  PathFlags flags; ///< Flags describing what's known about the path
};

void         mutt_path_free(struct Path **ptr);
struct Path *mutt_path_new(void);

#endif /* MUTT_CORE_PATH_H */
