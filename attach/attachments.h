/**
 * @file
 * Miscellaneous email parsing routines
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_ATTACH_ATTACHMENTS_H
#define MUTT_ATTACH_ATTACHMENTS_H

#include <stdio.h>

struct Email;
struct Mailbox;

/**
 * enum NotifyAttach - Attachments notification types
 *
 * Observers of #NT_ATTACH will not be passed any Event data.
 *
 * @note Notifications are sent **after** the event.
 */
enum NotifyAttach
{
  NT_ATTACH_ADD = 1,    ///< Attachment regex has been added
  NT_ATTACH_DELETE,     ///< Attachment regex has been deleted
  NT_ATTACH_DELETE_ALL, ///< All Attachment regexes have been deleted
};

void attach_init(void);
void attach_free(void);

void mutt_attachments_reset (struct Mailbox *m);
int  mutt_count_body_parts  (struct Mailbox *m, struct Email *e, FILE *fp);
void mutt_parse_mime_message(struct Email *e, FILE *fp);

#endif /* MUTT_ATTACH_ATTACHMENTS_H */
