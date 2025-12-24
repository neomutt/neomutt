/**
 * @file
 * Maildir-specific data
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

#ifndef MUTT_MAILDIR_PRIVATE_H
#define MUTT_MAILDIR_PRIVATE_H

#include <stdbool.h>
#include "mutt/lib.h"

/**
 * struct Filename - Name of a Maildir Email file
 */
struct Filename
{
  char *sub_name;     ///< Sub-directory/filename
  short uid_start;    ///< Start  of unique part of filename
  short uid_length;   ///< Length of unique part of filename
  bool  is_cur;       ///< File is in the 'cur' directory
};
ARRAY_HEAD(FilenameArray, struct Filename);

#endif /* MUTT_MAILDIR_PRIVATE_H */
