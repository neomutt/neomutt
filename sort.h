/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@cs.hmc.edu>
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
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */ 

#define SORT_DATE	1   /* the date the mail was sent. */
#define SORT_SIZE	2
#define SORT_SUBJECT	3
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
#define SORT_MASK	0xf
#define SORT_REVERSE	(1<<4)
#define SORT_LAST	(1<<5)

typedef int sort_t (const void *, const void *);
sort_t *mutt_get_sort_func (int);

void mutt_clear_threads (CONTEXT *);
void mutt_sort_headers (CONTEXT *, int);
void mutt_sort_threads (CONTEXT *, int);
int mutt_select_sort (int);
THREAD *mutt_sort_subthreads (THREAD *);

WHERE short BrowserSort INITVAL (SORT_SUBJECT);
WHERE short Sort INITVAL (SORT_DATE);
WHERE short SortAux INITVAL (SORT_DATE); /* auxiallary sorting method */
WHERE short SortAlias INITVAL (SORT_ALIAS);
#ifdef HAVE_PGP
WHERE short PgpSortKeys INITVAL (SORT_ADDRESS);
#endif

#include "mapping.h"
extern const struct mapping_t SortMethods[];
