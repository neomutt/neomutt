/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

#define SORT_DATE	1   /* the date the mail was sent. */
#define SORT_SIZE	2
#define SORT_SUBJECT	3
#define SORT_ALPHA	3   /* makedoc.c requires this */
#define SORT_FROM	4
#define SORT_ORDER	5   /* the order the messages appear in the mailbox. */
#define SORT_THREADS	6
#define SORT_RECEIVED	7   /* when the message were delivered locally */
#define SORT_TO		8
#define SORT_SCORE	9
#define SORT_ALIAS	10
#define SORT_ADDRESS	11
#define SORT_KEYID	12
#define SORT_TRUST	13
#define SORT_SPAM	14
/* dgc: Sort & SortAux are shorts, so I'm bumping these bitflags up from
 * bits 4 & 5 to bits 8 & 9 to make room for more sort keys in the future. */
#define SORT_MASK	0xff
#define SORT_REVERSE	(1<<8)
#define SORT_LAST	(1<<9)

typedef int sort_t (const void *, const void *);
sort_t *mutt_get_sort_func (int);

void mutt_clear_threads (CONTEXT *);
void mutt_sort_headers (CONTEXT *, int);
void mutt_sort_threads (CONTEXT *, int);
int mutt_select_sort (int);
THREAD *mutt_sort_subthreads (THREAD *, int);

WHERE short BrowserSort INITVAL (SORT_SUBJECT);
WHERE short Sort INITVAL (SORT_DATE);
WHERE short SortAux INITVAL (SORT_DATE); /* auxiliary sorting method */
WHERE short SortAlias INITVAL (SORT_ALIAS);

/* FIXME: This one does not belong to here */
WHERE short PgpSortKeys INITVAL (SORT_ADDRESS);

#include "mapping.h"
extern const struct mapping_t SortMethods[];
