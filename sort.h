/**
 * @file
 * Assorted sorting methods
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
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

#ifndef _MUTT_SORT_H
#define _MUTT_SORT_H

#include "mapping.h"
#include "where.h"

struct Address;
struct Context;

#define SORT_DATE     1 /**< the date the mail was sent. */
#define SORT_SIZE     2
#define SORT_SUBJECT  3
#define SORT_ALPHA    3 /**< makedoc.c requires this */
#define SORT_FROM     4
#define SORT_ORDER    5 /**< the order the messages appear in the mailbox. */
#define SORT_THREADS  6
#define SORT_RECEIVED 7 /**< when the message were delivered locally */
#define SORT_TO       8
#define SORT_SCORE    9
#define SORT_ALIAS    10
#define SORT_ADDRESS  11
#define SORT_KEYID    12
#define SORT_TRUST    13
#define SORT_SPAM     14
#define SORT_COUNT    15
#define SORT_UNREAD   16
#define SORT_FLAGGED  17
#define SORT_PATH     18
#define SORT_LABEL    19
#define SORT_DESC     20

/* Sort and sort_aux are shorts, and are a composite of a
 * constant sort operation number and a set of compounded
 * bitflags.
 *
 * Everything below SORT_MASK is a constant. There's room for
 * SORT_MASK constant SORT_ values.
 *
 * Everything above is a bitflag. It's OK to move SORT_MASK
 * down by powers of 2 if we need more, so long as we don't
 * collide with the constants above. (Or we can just expand
 * sort and sort_aux to uint32_t.)
 */
#define SORT_MASK    ((1 << 8) - 1)
#define SORT_REVERSE (1 << 8)
#define SORT_LAST    (1 << 9)

typedef int sort_t(const void *a, const void *b);
sort_t *mutt_get_sort_func(int method);

void mutt_sort_headers(struct Context *ctx, int init);
int mutt_select_sort(int reverse);

WHERE short BrowserSort INITVAL(SORT_SUBJECT);
WHERE short Sort INITVAL(SORT_DATE);
WHERE short SortAux INITVAL(SORT_DATE); /* auxiliary sorting method */
WHERE short SortAlias INITVAL(SORT_ALIAS);
WHERE short SidebarSortMethod INITVAL(SORT_ORDER);

/* FIXME: This one does not belong to here */
WHERE short PgpSortKeys INITVAL(SORT_ADDRESS);

extern const struct Mapping SortMethods[];

const char *mutt_get_name(struct Address *a);

#endif /* _MUTT_SORT_H */
