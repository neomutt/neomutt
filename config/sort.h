/**
 * @file
 * Type representing a sort option
 *
 * @authors
 * Copyright (C) 2017-2018 Richard Russon <rich@flatcap.org>
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

#ifndef _CONFIG_SORT_H
#define _CONFIG_SORT_H

#include "mutt/mapping.h"

struct ConfigSet;

extern const struct Mapping SortAliasMethods[];
extern const struct Mapping SortAuxMethods[];
extern const struct Mapping SortBrowserMethods[];
extern const struct Mapping SortKeyMethods[];
extern const struct Mapping SortMethods[];
extern const struct Mapping SortSidebarMethods[];

/* ... DT_SORT */
#define DT_SORT_INDEX   0x000 /**< Sort id for #SortMethods */
#define DT_SORT_ALIAS   0x040 /**< Sort id for #SortAliasMethods */
#define DT_SORT_BROWSER 0x080 /**< Sort id for #SortBrowserMethods */
#define DT_SORT_KEYS    0x100 /**< Sort id for #SortKeyMethods */
#define DT_SORT_AUX     0x200 /**< Sort id for #SortAliasMethods */
#define DT_SORT_SIDEBAR 0x400 /**< Sort id for #SortSidebarMethods */

#define SORT_DATE      1 /**< Sort by the date the email was sent. */
#define SORT_SIZE      2 /**< Sort by the size of the email */
#define SORT_ALPHA     3 /**< Required by makedoc.c */
#define SORT_SUBJECT   3 /**< Sort by the email's subject */
#define SORT_FROM      4 /**< Sort by the email's From field */
#define SORT_ORDER     5 /**< Sort by the order the messages appear in the mailbox */
#define SORT_THREADS   6 /**< Sort by email threads */
#define SORT_RECEIVED  7 /**< Sort by when the message were delivered locally */
#define SORT_TO        8 /**< Sort by the email's To field */
#define SORT_SCORE     9 /**< Sort by the email's score */
#define SORT_ALIAS    10 /**< Sort by email alias */
#define SORT_ADDRESS  11 /**< Sort by email address */
#define SORT_KEYID    12 /**< Sort by the encryption key's ID */
#define SORT_TRUST    13 /**< Sort by encryption key's trust level */
#define SORT_SPAM     14 /**< Sort by the email's spam score */
#define SORT_COUNT    15 /**< Sort by number of emails in a folder */
#define SORT_UNREAD   16 /**< Sort by the number of unread emails */
#define SORT_FLAGGED  17 /**< Sort by the number of flagged emails */
#define SORT_PATH     18 /**< Sort by the folder's path */
#define SORT_LABEL    19 /**< Sort by the emails label */
#define SORT_DESC     20 /**< Sort by the folder's description */

/* Sort and sort_aux are shorts, and are a composite of a constant sort
 * operation number and a set of compounded bitflags.
 *
 * Everything below SORT_MASK is a constant. There's room for SORT_MASK
 * constant SORT_ values.
 *
 * Everything above is a bitflag. It's OK to move SORT_MASK down by powers of 2
 * if we need more, so long as we don't collide with the constants above. (Or
 * we can just expand sort and sort_aux to uint32_t.)
 */
#define SORT_MASK    ((1 << 8) - 1) /**< Mask for the sort id */
#define SORT_REVERSE  (1 << 8)      /**< Reverse the order of the sort */
#define SORT_LAST     (1 << 9)      /**< Sort thread by last-X, e.g. received date */

void sort_init(struct ConfigSet *cs);

#endif /* _CONFIG_SORT_H */
