/**
 * @file
 * Assorted sorting methods
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2018-2023 Richard Russon <rich@flatcap.org>
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
#include "core/lib.h"

struct Address;
struct Email;
struct MailboxView;

#define mutt_numeric_cmp(a,b) ((a) < (b) ? -1 : ((a) > (b) ? 1 : 0))

/**
 * @defgroup sort_mail_api Mail Sorting API
 *
 * Prototype for an email comparison function
 *
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

void mutt_sort_headers(struct MailboxView *mv, bool init);
void mutt_sort_order(struct Mailbox *m);

const char *mutt_get_name(const struct Address *a);

#endif /* MUTT_SORT_H */
