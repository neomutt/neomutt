/**
 * @file
 * Assorted sorting methods
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
#include <sys/types.h>
#include "config/lib.h"
#include "core/lib.h"
#include "options.h" // IWYU pragma: keep

struct Address;
struct ThreadsContext;

/**
 * typedef sort_t - Prototype for generic comparison function, compatible with qsort
 * @param a First item
 * @param b Second item
 * @retval <0 a precedes b
 * @retval  0 a and b are identical
 * @retval >0 b precedes a
 */
typedef int (*sort_t)(const void *a, const void *b);

/**
 * typedef sort_mail_t - Prototype for comparing two emails
 * @param a       First item
 * @param b       Second item
 * @param reverse true if this is a reverse sort (smaller b precedes a)
 * @retval <0 a precedes b
 * @retval  0 a and b are identical
 * @retval >0 b precedes a
 */
typedef int (*sort_mail_t)(const struct Email *a, const struct Email *b, bool reverse);

int mutt_compare_emails(const struct Email *a, const struct Email *b,
                        enum MailboxType type, short sort, short sort_aux);

void mutt_sort_headers(struct Mailbox *m, struct ThreadsContext *threads, bool init, off_t *vsize);

const char *mutt_get_name(const struct Address *a);

#endif /* MUTT_SORT_H */
