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

#ifndef MUTT_EMAIL_SORT_H
#define MUTT_EMAIL_SORT_H

#include <stdbool.h>
#include "core/lib.h"

struct Address;
struct Email;
struct MailboxView;

/**
 * @defgroup sort_email_api Email Sorting API
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
typedef int (*sort_email_t)(const struct Email *a, const struct Email *b, bool reverse);

/**
 * enum EmailSortType - Methods for sorting Emails
 */
enum EmailSortType
{
  EMAIL_SORT_DATE,             ///< Sort by the date the email was sent
  EMAIL_SORT_DATE_RECEIVED,    ///< Sort by when the message was delivered locally
  EMAIL_SORT_FROM,             ///< Sort by the email's From field
  EMAIL_SORT_LABEL,            ///< Sort by the emails label
  EMAIL_SORT_SCORE,            ///< Sort by the email's score
  EMAIL_SORT_SIZE,             ///< Sort by the size of the email
  EMAIL_SORT_SPAM,             ///< Sort by the email's spam score
  EMAIL_SORT_SUBJECT,          ///< Sort by the email's subject
  EMAIL_SORT_THREADS,          ///< Sort by email threads
  EMAIL_SORT_TO,               ///< Sort by the email's To field
  EMAIL_SORT_UNSORTED,         ///< Sort by the order the messages appear in the mailbox
};

int mutt_compare_emails(const struct Email *a, const struct Email *b,
                        enum MailboxType type, short sort, short sort_aux);

void mutt_sort_headers(struct MailboxView *mv, bool init);
void mutt_sort_unsorted(struct Mailbox *m);

const char *mutt_get_name(const struct Address *a);

#endif /* MUTT_EMAIL_SORT_H */
