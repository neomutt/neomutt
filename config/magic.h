/**
 * @file
 * Type representing a mailbox
 *
 * @authors
 * Copyright (C) 2017-2018 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_CONFIG_MAGIC_H
#define MUTT_CONFIG_MAGIC_H

struct ConfigSet;

extern const char *MagicValues[];

/**
 * enum MailboxType - Supported mailbox formats
 */
enum MailboxType
{
  MUTT_MAILBOX_ERROR = -1, ///< Error occurred examining mailbox
  MUTT_UNKNOWN = 0,        ///< Mailbox wasn't recognised
  MUTT_MBOX,               ///< 'mbox' Mailbox type
  MUTT_MMDF,               ///< 'mmdf' Mailbox type
  MUTT_MH,                 ///< 'MH' Mailbox type
  MUTT_MAILDIR,            ///< 'Maildir' Mailbox type
  MUTT_NNTP,               ///< 'NNTP' (Usenet) Mailbox type
  MUTT_IMAP,               ///< 'IMAP' Mailbox type
  MUTT_NOTMUCH,            ///< 'Notmuch' (virtual) Mailbox type
  MUTT_POP,                ///< 'POP3' Mailbox type
  MUTT_COMPRESSED,         ///< Compressed file Mailbox type
};

void magic_init(struct ConfigSet *cs);

#endif /* MUTT_CONFIG_MAGIC_H */
