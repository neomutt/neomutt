/**
 * @file
 * Maildir Account
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

#ifndef MUTT_MAILDIR_ACCOUNT_H
#define MUTT_MAILDIR_ACCOUNT_H

#include <stdbool.h>

struct Account;
struct Mailbox;

bool maildir_ac_add      (struct Account *a, struct Mailbox *m);
bool maildir_ac_owns_path(struct Account *a, const char *path);

#endif /* MUTT_MAILDIR_ACCOUNT_H */
