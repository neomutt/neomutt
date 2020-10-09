/**
 * @file
 * Maildir-specific Email data
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_MAILDIR_EDATA_H
#define MUTT_MAILDIR_EDATA_H

struct Email;

/**
 * struct MaildirEmailData - Maildir-specific Email data - @extends Email
 *
 * @note Also used by MH Mailboxes
 */
struct MaildirEmailData
{
  char *maildir_flags; ///< Unknown Maildir flags
};

void                     maildir_edata_free(void **ptr);
struct MaildirEmailData *maildir_edata_get(struct Email *e);
struct MaildirEmailData *maildir_edata_new(void);

#endif /* MUTT_MAILDIR_EDATA_H */
