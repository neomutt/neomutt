/**
 * @file
 * Keep track of the current Account and Mailbox
 *
 * @authors
 * Copyright (C) 1996-2000,2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_TRACKER_H
#define MUTT_TRACKER_H

struct Account *     ct_get_account(void);
struct Mailbox *     ct_get_mailbox(void);
struct ConfigSubset *ct_get_sub(void);
void                 ct_pop(void);
void                 ct_push_top(void);
void                 ct_set_account(struct Account *a);
void                 ct_set_mailbox(struct Mailbox *m);

void ct_dump(void);

#endif /* MUTT_TRACKER_H */
