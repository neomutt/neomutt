/**
 * @file
 * IMAP Message Sets
 *
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_IMAP_MSG_SET_H
#define MUTT_IMAP_MSG_SET_H

#include <stdbool.h>
#include "mutt/lib.h"
#include "mutt.h"

struct Mailbox;

/// Set of Email UIDs to work on
ARRAY_HEAD(UidArray, unsigned int);

int imap_exec_msg_set(struct Mailbox *m, const char *pre, const char *post, enum MessageType flag, bool changed, bool invert);

#endif /* MUTT_IMAP_MSG_SET_H */
