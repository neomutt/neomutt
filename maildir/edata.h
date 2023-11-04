/**
 * @file
 * Maildir-specific Email data
 *
 * @authors
 * Copyright (C) 2020-2024 Richard Russon <rich@flatcap.org>
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

#include <stdint.h>

struct Email;

typedef uint8_t MaildirFlags;      ///< Maildir filename flags, e.g. #MD_MF_FLAGGED
#define MD_MF_NO_FLAGS          0  ///< No flags are set
#define MD_MF_FLAGGED     (1 << 0) ///< Email is flagged
#define MD_MF_REPLIED     (1 << 1) ///< Email has been replied to
#define MD_MF_SEEN        (1 << 2) ///< Email has been seen
#define MD_MF_TRASHED     (1 << 3) ///< Email is marked as deleted

/**
 * struct MaildirEmailData - Maildir-specific Email data - @extends Email
 */
struct MaildirEmailData
{
  short        uid_start;      ///< Start  of unique part of filename
  short        uid_length;     ///< Length of unique part of filename
  MaildirFlags disk_flags;     ///< Cached Maildir filename flags
  char        *custom_flags;   ///< Custom Maildir flags (e.g Dovecot labels)
};

void                     maildir_edata_free(void **ptr);
struct MaildirEmailData *maildir_edata_get(struct Email *e);
struct MaildirEmailData *maildir_edata_new(void);

#endif /* MUTT_MAILDIR_EDATA_H */
