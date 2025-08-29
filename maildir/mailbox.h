/**
 * @file
 * Maildir Mailbox
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_MAILDIR_MAILBOX_H
#define MUTT_MAILDIR_MAILBOX_H

#include <stdbool.h>
#include <stdint.h>
#include "core/lib.h"

struct Email;

enum MxStatus      maildir_mbox_check      (struct Mailbox *m);
enum MxStatus      maildir_mbox_check_stats(struct Mailbox *m, uint8_t flags);
enum MxStatus      maildir_mbox_close      (struct Mailbox *m);
enum MxOpenReturns maildir_mbox_open       (struct Mailbox *m);
bool               maildir_mbox_open_append(struct Mailbox *m, OpenMailboxFlags flags);
enum MxStatus      maildir_mbox_sync       (struct Mailbox *m);
void               maildir_parse_flags     (struct Email *e, const char *path);

#endif /* MUTT_MAILDIR_MAILBOX_H */
