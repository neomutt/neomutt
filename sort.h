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

#include <stdbool.h>
#include "mutt/mutt.h"
#include "config/lib.h"
#include "where.h"

struct Address;
struct Context;

/* These Config Variables are only used in sort.c */
extern bool ReverseAlias;

#define SORTCODE(x) (Sort & SORT_REVERSE) ? -(x) : x

typedef int sort_t(const void *a, const void *b);
sort_t *mutt_get_sort_func(int method);

void mutt_sort_headers(struct Context *ctx, int init);
int perform_auxsort(int retval, const void *a, const void *b);

extern const struct Mapping SortMethods[];

const char *mutt_get_name(struct Address *a);

/* These variables are backing for config items */
WHERE short Sort;
WHERE short SortAux; /* auxiliary sorting method */

/* FIXME: This one does not belong to here */
WHERE short PgpSortKeys;

#endif /* _MUTT_SORT_H */
