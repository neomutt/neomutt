/**
 * @file
 * Type representing a sort option
 *
 * @authors
 * Copyright (C) 2017-2019 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_CONFIG_SORT2_H
#define MUTT_CONFIG_SORT2_H

#include "mutt/lib.h"

extern const struct Mapping SortMethods[];

/**
 * enum SortType - Methods for sorting
 */
enum SortType
{
  SORT_DATE     =  1, ///< Sort by the date the email was sent
  SORT_SIZE     =  2, ///< Sort by the size of the email
  SORT_ALPHA    =  3, ///< Required by makedoc.c
  SORT_SUBJECT  =  3, ///< Sort by the email's subject
  SORT_FROM     =  4, ///< Sort by the email's From field
  SORT_ORDER    =  5, ///< Sort by the order the messages appear in the mailbox
  SORT_THREADS  =  6, ///< Sort by email threads
  SORT_RECEIVED =  7, ///< Sort by when the message were delivered locally
  SORT_TO       =  8, ///< Sort by the email's To field
  SORT_SCORE    =  9, ///< Sort by the email's score
  SORT_ALIAS    = 10, ///< Sort by email alias
  SORT_ADDRESS  = 11, ///< Sort by email address
  SORT_KEYID    = 12, ///< Sort by the encryption key's ID
  SORT_TRUST    = 13, ///< Sort by encryption key's trust level
  SORT_SPAM     = 14, ///< Sort by the email's spam score
  SORT_COUNT    = 15, ///< Sort by number of emails in a folder
  SORT_UNREAD   = 16, ///< Sort by the number of unread emails
  SORT_FLAGGED  = 17, ///< Sort by the number of flagged emails
  SORT_PATH     = 18, ///< Sort by the folder's path
  SORT_LABEL    = 19, ///< Sort by the emails label
  SORT_DESC     = 20, ///< Sort by the folder's description

  SORT_MAX,
};

/* `$sort` and `$sort_aux` are shorts, and are a composite of a constant sort
 * operation number and a set of compounded bitflags.
 *
 * Everything below SORT_MASK is a constant. There's room for SORT_MASK
 * constant SORT_ values.
 *
 * Everything above is a bitflag. It's OK to move SORT_MASK down by powers of 2
 * if we need more, so long as we don't collide with the constants above. (Or
 * we can just expand sort and sort_aux to uint32_t.)
 */
#define SORT_MASK    ((1 << 8) - 1) ///< Mask for the sort id
#define SORT_REVERSE  (1 << 8)      ///< Reverse the order of the sort
#define SORT_LAST     (1 << 9)      ///< Sort thread by last-X, e.g. received date

#endif /* MUTT_CONFIG_SORT2_H */
