/**
 * @file
 * Send email to an SMTP server
 *
 * @authors
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2019-2020 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_SEND_SMTP_H
#define MUTT_SEND_SMTP_H

#include "config.h"
#include <stdbool.h>

struct AddressList;
struct ConfigSubset;

bool smtp_auth_is_valid(const char *authenticator);
int mutt_smtp_send(const struct AddressList *from, const struct AddressList *to,
                   const struct AddressList *cc, const struct AddressList *bcc,
                   const char *msgfile, bool eightbit, struct ConfigSubset *sub);

#endif /* MUTT_SEND_SMTP_H */
