/**
 * @file
 * Representation of the email's header
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_MUTT_HEADER_H
#define MUTT_MUTT_HEADER_H

struct Buffer;
struct Email;
struct EmailList;
struct Mailbox;

void mutt_edit_headers(const char *editor, const char *body, struct Email *e, struct Buffer *fcc);
void mutt_label_hash_add(struct Mailbox *m, struct Email *e);
void mutt_label_hash_remove(struct Mailbox *m, struct Email *e);
int mutt_label_message(struct Mailbox *m, struct EmailList *el);
void mutt_make_label_hash(struct Mailbox *m);

#endif /* MUTT_MUTT_HEADER_H */
