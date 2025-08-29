/**
 * @file
 * Send email using sendmail
 *
 * @authors
 * Copyright (C) 2019-2021 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_SEND_SENDMAIL_H
#define MUTT_SEND_SENDMAIL_H

#include <stdbool.h>

struct AddressList;
struct ConfigSubset;
struct Mailbox;

int mutt_invoke_sendmail(struct Mailbox *m, struct AddressList *from,
                         struct AddressList *to, struct AddressList *cc,
                         struct AddressList *bcc, const char *msg,
                         bool eightbit, struct ConfigSubset *sub);

#endif /* MUTT_SEND_SENDMAIL_H */
