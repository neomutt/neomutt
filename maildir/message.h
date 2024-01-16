/**
 * @file
 * Maildir Message
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

#ifndef MUTT_MAILDIR_MESSAGE_H
#define MUTT_MAILDIR_MESSAGE_H

#include <stdbool.h>

struct Email;
struct Mailbox;
struct Message;

int  maildir_msg_close      (struct Mailbox *m, struct Message *msg);
int  maildir_msg_commit     (struct Mailbox *m, struct Message *msg);
bool maildir_msg_open_new   (struct Mailbox *m, struct Message *msg, const struct Email *e);
bool maildir_msg_open       (struct Mailbox *m, struct Message *msg, struct Email *e);
int  maildir_msg_save_hcache(struct Mailbox *m, struct Email *e);

#endif /* MUTT_MAILDIR_MESSAGE_H */
