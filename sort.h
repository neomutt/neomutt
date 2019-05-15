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

#ifndef MUTT_SORT_H
#define MUTT_SORT_H

#include <stdbool.h>
#include "mutt/mutt.h"
#include "config/lib.h"
#include "options.h"
#include "where.h"

struct Address;
struct Context;

/* These Config Variables are only used in sort.c */
extern bool C_ReverseAlias;

#define SORT_CODE(x) ((OptAuxSort ? C_SortAux : C_Sort) & SORT_REVERSE) ? -(x) : x

/**
 * typedef sort_t - Prototype for a function to compare two emails
 * @param a First email
 * @param b Second email
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
typedef int sort_t(const void *a, const void *b);

sort_t *mutt_get_sort_func(enum SortType method);

void mutt_sort_headers(struct Context *ctx, bool init);
int perform_auxsort(int retval, const void *a, const void *b);

const char *mutt_get_name(const struct Address *a);

/* These variables are backing for config items */
WHERE short C_Sort;    ///< Config: Sort method for the index
WHERE short C_SortAux; ///< Config: Secondary sort method for the index

/* FIXME: This one does not belong to here */
WHERE short C_PgpSortKeys; ///< Config: Sort order for PGP keys

#endif /* MUTT_SORT_H */
