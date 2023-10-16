/**
 * @file
 * Mailbox helper functions
 *
 * @authors
 * Copyright (C) 2019-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2022 Pietro Cerutti <gahr@gahr.ch>
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

#ifndef MUTT_MUTT_MAILBOX_H
#define MUTT_MUTT_MAILBOX_H

#include <stdbool.h>
#include "core/lib.h"

struct Buffer;
struct stat;

int  mutt_mailbox_check       (struct Mailbox *m_cur, CheckStatsFlags flags);
void mailbox_restore_timestamp(const char *path, struct stat *st);
bool mutt_mailbox_list        (void);
struct Mailbox *mutt_mailbox_next(struct Mailbox *m_cur, struct Buffer *s);
struct Mailbox *mutt_mailbox_next_unread(struct Mailbox *m_cur, struct Buffer *s);
bool mutt_mailbox_notify      (struct Mailbox *m_cur);
void mutt_mailbox_set_notified(struct Mailbox *m);

bool mailbox_check    (struct Mailbox *m);
int mailbox_check_all (CheckStatsFlags flags);

#endif /* MUTT_MUTT_MAILBOX_H */
