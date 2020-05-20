/**
 * @file
 * Handle mailing lists
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_MAILLIST_H
#define MUTT_MAILLIST_H

#include <stddef.h>
#include <stdbool.h>

struct Address;
struct AddressList;

bool check_for_mailing_list     (struct AddressList *al, const char *pfx, char *buf, int buflen);
bool check_for_mailing_list_addr(struct AddressList *al, char *buf, int buflen);
bool first_mailing_list         (char *buf, size_t buflen, struct AddressList *al);
bool mutt_is_mail_list          (const struct Address *addr);
bool mutt_is_subscribed_list    (const struct Address *addr);

#endif /* MUTT_MAILLIST_H */
